// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/Diagnostics.h>
#include <flow/lang/Lexer.h>
#include <flow/util/IPAddress.h>

#include <cstring>
#include <sstream>
#include <system_error>
#include <unordered_map>

#if defined(HAVE_GLOB_H)
#include <glob.h>
#endif

namespace flow::lang {

inline std::string escape(char value)  // {{{
{
  switch (value) {
    case '\t':
      return "<TAB>";
    case '\r':
      return "<CR>";
    case '\n':
      return "<LF>";
    case ' ':
      return "<SPACE>";
    default:
      break;
  }
  if (std::isprint(value)) {
    std::string s;
    s += value;
    return s;
  } else {
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%02X", value);
    return buf;
  }
}  // }}}

static inline std::string unescape(const std::string& value)  // {{{
{
  std::string result;

  for (auto i = value.begin(), e = value.end(); i != e; ++i) {
    if (*i != '\\') {
      result += *i;
    } else if (++i != e) {
      switch (*i) {
        case '\\':
          result += '\\';
          break;
        case 'r':
          result += '\r';
          break;
        case 'n':
          result += '\n';
          break;
        case 't':
          result += '\t';
          break;
        default:
          result += *i;
      }
    }
  }

  return result;
}
// }}}

Lexer::Lexer(diagnostics::Report* report)
    : report_{*report},
      contexts_(),
      currentChar_(EOF),
      ipv6HexDigits_(0),
      location_(),
      token_(Token::Eof),
      stringValue_(),
      ipValue_(),
      numberValue_(0),
      interpolationDepth_(0) {}

Lexer::Scope::Scope()
    : filename(), basedir(), stream(), currPos(), nextPos(), backupChar() {}

void Lexer::Scope::setStream(const std::string& filename,
                                 std::unique_ptr<std::istream>&& istream) {
  this->filename = filename;
  this->stream = std::move(istream);
  this->basedir = "TODO";
}

void Lexer::openLocalFile(const std::string& filename) {
  enterScope(filename);
  nextToken();
}

void Lexer::openString(const std::string& content) {
  auto sstr = std::make_unique<std::stringstream>();
  (*sstr) << content;
  openStream(std::move(sstr), "<string>");
}

void Lexer::openStream(
    std::unique_ptr<std::istream>&& ifs,
    const std::string& filename) {
  enterScope(std::move(ifs), filename);
  nextToken();
}

Lexer::Scope* Lexer::enterScope(
    std::unique_ptr<std::istream>&& ifs,
    const std::string& filename) {
  std::unique_ptr<Scope> cx = std::make_unique<Scope>();
  if (!cx) return nullptr;

  cx->setStream(filename, std::move(ifs));
  cx->backupChar = currentChar();

  contexts_.push_front(std::move(cx));

  // parse first char in new context
  currentChar_ = '\0';  // something that is not EOF
  nextChar();

  return contexts_.front().get();
}

Lexer::Scope* Lexer::enterScope(const std::string& filename) {
  auto ifs = std::make_unique<std::ifstream>();

  ifs->open(filename);
  if (!ifs->good())
    throw std::system_error(errno, std::system_category());

  return enterScope(std::move(ifs), filename);
}

void Lexer::leaveScope() {
  currentChar_ = scope()->backupChar;
  contexts_.pop_front();
}

int Lexer::peekChar() { return scope()->stream->peek(); }

bool Lexer::eof() const {
  return currentChar_ == EOF || scope()->stream->eof();
}

int Lexer::nextChar(bool interscope) {
  if (currentChar_ == EOF) return currentChar_;

  location_.end = scope()->currPos;
  scope()->currPos = scope()->nextPos;

  int ch = scope()->stream->get();
  if (ch == EOF) {
    currentChar_ = ch;

    if (interscope && contexts_.size() > 1) {
      leaveScope();
    }
    return currentChar_;
  }

  currentChar_ = ch;
  // content_ += static_cast<char>(currentChar_);
  scope()->nextPos.offset++;

  if (currentChar_ != '\n') {
    scope()->nextPos.column++;
  } else {
    scope()->nextPos.column = 1;
    scope()->nextPos.line++;
  }

  return currentChar_;
}

bool Lexer::consume(char ch) {
  bool result = currentChar() == ch;
  nextChar();
  return result;
}

/**
 * @retval true data pending
 * @retval false EOF reached
 */
bool Lexer::consumeSpace() {
  // skip spaces
  for (;; nextChar()) {
    if (eof()) return false;

    if (std::isspace(currentChar_)) continue;

    if (std::isprint(currentChar_)) break;

    // TODO proper error reporting through API callback
    fmt::print("{}[{}:{}]: invalid byte {}\n",
          location_.filename, line(), column(), currentChar() & 0xFF);
  }

  if (eof()) return false;

  if (currentChar() == '#') {
    bool maybeCommand = scope()->currPos.column == 1;
    std::string line;
    nextChar();
    // skip chars until EOL
    for (;;) {
      if (eof()) {
        token_ = Token::Eof;
        if (maybeCommand) processCommand(line);
        return token_ != Token::Eof;
      }

      if (currentChar() == '\n') {
        if (maybeCommand) processCommand(line);
        return consumeSpace();
      }

      line += static_cast<char>(currentChar());
      nextChar();
    }
  }

  if (currentChar() == '/' && peekChar() == '*') {  // "/*" ... "*/"
    // parse multiline comment
    nextChar();

    for (;;) {
      if (eof()) {
        token_ = Token::Eof;
        // reportError(Error::UnexpectedEof);
        return false;
      }

      if (currentChar() == '*' && peekChar() == '/') {
        nextChar();  // skip '*'
        nextChar();  // skip '/'
        break;
      }

      nextChar();
    }

    return consumeSpace();
  }

  return true;
}

void Lexer::processCommand(const std::string& line) {
  // `#include "glob"`

  if (strncmp(line.c_str(), "include", 7) != 0) return;

  // TODO this is *very* basic sub-tokenization, but it does well until properly
  // implemented.

  size_t beg = line.find('"');
  size_t end = line.rfind('"');
  if (beg == std::string::npos || end == std::string::npos) {
    report_.tokenError(lastLocation(), "Malformed #include line");
    return;
  }

  std::string pattern = line.substr(beg + 1, end - beg - 1);

#if defined(HAVE_GLOB_H)
  glob_t gl;

  int globOptions = 0;
#if defined(GLOB_TILDE)
  globOptions |= GLOB_TILDE;
#endif

  int rv = glob(pattern.c_str(), globOptions, nullptr, &gl);
  if (rv != 0) {
    static std::unordered_map<int, const char*> globErrs = {
        {GLOB_NOSPACE, "No space"},
        {GLOB_ABORTED, "Aborted"},
        {GLOB_NOMATCH, "No Match"}, };
    report_.tokenError(lastLocation(), "glob() error: {}", globErrs[rv]);
    return;
  }

  // put globbed files on stack in reverse order, to be lexed in the right order
  for (size_t i = 0; i < gl.gl_pathc; ++i) {
    const char* filename = gl.gl_pathv[gl.gl_pathc - i - 1];
    enterScope(filename);
  }

  globfree(&gl);
#else
  enterScope(pattern);
#endif
}

Token Lexer::nextToken() {
  if (!consumeSpace()) return token_ = Token::Eof;

  lastLocation_ = location_;

  location_.filename = scope()->filename;
  location_.begin = scope()->currPos;

  switch (currentChar()) {
    case '~':
      nextChar();
      return token_ = Token::BitNot;
    case '=':
      switch (nextChar()) {
        case '=':
          nextChar();
          return token_ = Token::Equal;
        case '^':
          nextChar();
          return token_ = Token::PrefixMatch;
        case '$':
          nextChar();
          return token_ = Token::SuffixMatch;
        case '~':
          nextChar();
          return token_ = Token::RegexMatch;
        case '>':
          nextChar();
          return token_ = Token::HashRocket;
        default:
          return token_ = Token::Assign;
      }
    case '<':
      switch (nextChar()) {
        case '<':
          nextChar();
          return token_ = Token::Shl;
        case '=':
          nextChar();
          return token_ = Token::LessOrEqual;
        default:
          return token_ = Token::Less;
      }
    case '>':
      switch (nextChar()) {
        case '>':
          nextChar();
          return token_ = Token::Shr;
        case '=':
          nextChar();
          return token_ = Token::GreaterOrEqual;
        default:
          return token_ = Token::Greater;
      }
    case '^':
      nextChar();
      return token_ = Token::BitXor;
    case '|':
      switch (nextChar()) {
        case '|':
          nextChar();
          return token_ = Token::Or;
        case '=':
          nextChar();
          return token_ = Token::OrAssign;
        default:
          return token_ = Token::BitOr;
      }
    case '&':
      switch (nextChar()) {
        case '&':
          nextChar();
          return token_ = Token::And;
        case '=':
          nextChar();
          return token_ = Token::AndAssign;
        default:
          return token_ = Token::BitAnd;
      }
    case '.':
      if (nextChar() == '.') {
        if (nextChar() == '.') {
          nextChar();
          return token_ = Token::Ellipsis;
        }
        return token_ = Token::DblPeriod;
      }
      return token_ = Token::Period;
    case ':':
      if (peekChar() == ':') {
        stringValue_.clear();
        return continueParseIPv6(false);
      } else {
        nextChar();
        return token_ = Token::Colon;
      }
    case ';':
      nextChar();
      return token_ = Token::Semicolon;
    case ',':
      nextChar();
      return token_ = Token::Comma;
    case '{':
      nextChar();
      return token_ = Token::Begin;
    case '}':
      if (interpolationDepth_) {
        return token_ = parseInterpolationFragment(false);
      } else {
        nextChar();
        return token_ = Token::End;
      }
    case '(':
      nextChar();
      return token_ = Token::RndOpen;
    case ')':
      nextChar();
      return token_ = Token::RndClose;
    case '[':
      nextChar();
      return token_ = Token::BrOpen;
    case ']':
      nextChar();
      return token_ = Token::BrClose;
    case '+':
      nextChar();
      return token_ = Token::Plus;
    case '-':
      nextChar();
      return token_ = Token::Minus;
    case '*':
      switch (nextChar()) {
        case '*':
          nextToken();
          return token_ = Token::Pow;
        default:
          return token_ = Token::Mul;
      }
    case '/':
      // if (expectsValue) {
      //   return token_ = parseString('/', Token::RegExp);
      // }

      nextChar();
      return token_ = Token::Div;
    case '%':
      nextChar();
      return token_ = Token::Mod;
    case '!':
      switch (nextChar()) {
        case '=':
          nextChar();
          return token_ = Token::UnEqual;
        default:
          return token_ = Token::Not;
      }
    case '$':
      return token_ = parseEnvVar();
    case '\'':
      return token_ = parseRawString();
    case '"':
      ++interpolationDepth_;
      return token_ = parseInterpolationFragment(true);
    case '0':
      return parseNumber(8);
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      return parseNumber(10);
    default:
      if (std::isalpha(currentChar()) || currentChar() == '_')
        return token_ = parseIdent();

      report_.tokenError(lastLocation(),
                         "unknown character {} ({})",
                         escape(currentChar()), (int)(currentChar() & 0xFF));

      nextChar();
      return token_ = Token::Unknown;
  }

  return token_;
}

Token Lexer::parseEnvVar() {
  stringValue_.clear();
  nextChar(); // skip leading '$'

  while (!eof() && (currentChar() == '_' || std::isalnum(currentChar()))) {
    stringValue_ += static_cast<char>(currentChar());
    nextChar();
  }

  if (char* value = getenv(stringValue_.c_str()); value != nullptr) {
    stringValue_ = value;
  } else {
    stringValue_.clear();
  }

  return Token::String;
}

Token Lexer::parseRawString() {
  Token result = parseString(Token::String);

  if (result == Token::String)
    stringValue_ = unescape(stringValue_);

  return result;
}

Token Lexer::parseString(Token result) {
  int delim = currentChar();
  int last = -1;

  nextChar();  // skip left delimiter
  stringValue_.clear();

  while (!eof() && (currentChar() != delim || (last == '\\'))) {
    stringValue_ += static_cast<char>(currentChar());

    last = currentChar();
    nextChar();
  }

  if (currentChar() == delim) {
    nextChar();

    return token_ = result;
  }

  return token_ = Token::Unknown;
}

Token Lexer::parseInterpolationFragment(bool start) {
  int last = -1;
  stringValue_.clear();

  // skip either '"' or '}' depending on your we entered
  nextChar();

  for (;;) {
    if (eof()) return token_ = Token::Eof;

    if (currentChar() == '"' && last != '\\') {
      nextChar();
      --interpolationDepth_;
      return token_ =
                 start ? Token::String : Token::InterpolatedStringEnd;
    }

    if (currentChar() == '\\') {
      nextChar();

      if (eof()) return token_ = Token::Eof;

      switch (currentChar()) {
        case 'r':
          stringValue_ += '\r';
          break;
        case 'n':
          stringValue_ += '\n';
          break;
        case 't':
          stringValue_ += '\t';
          break;
        case '\\':
          stringValue_ += '\\';
          break;
        default:
          stringValue_ += '\\';
          stringValue_ += static_cast<char>(currentChar());
          break;
      }
    } else if (currentChar() == '#') {
      nextChar();
      if (currentChar() == '{') {
        nextChar();
        return token_ = Token::InterpolatedStringFragment;
      } else {
        stringValue_ += '#';
      }
      stringValue_ += static_cast<char>(currentChar());
    } else {
      stringValue_ += static_cast<char>(currentChar());
    }

    last = currentChar();
    nextChar();
  }
}

Token Lexer::parseNumber(int base) {
  stringValue_.clear();
  numberValue_ = 0;

  while (currentChar() >= '0' && (currentChar() - '0') < base) {
    numberValue_ *= base;
    numberValue_ += currentChar() - '0';
    stringValue_ += static_cast<char>(currentChar());
    nextChar();
  }

  // ipv6HexDigit4 *(':' ipv6HexDigit4) ['::' [ipv6HexSeq]]
  if (stringValue_.size() <= 4 && currentChar() == ':')
    return continueParseIPv6(true);

  if (stringValue_.size() < 4 && isHexChar())
    return continueParseIPv6(false);

  if (currentChar() != '.')
    return token_ = Token::Number;

  // 2nd IP component
  stringValue_ += '.';
  nextChar();
  while (std::isdigit(currentChar())) {
    stringValue_ += static_cast<char>(currentChar());
    nextChar();
  }

  // 3rd IP component
  if (!consume('.')) return token_ = Token::Unknown;

  stringValue_ += '.';
  while (std::isdigit(currentChar())) {
    stringValue_ += static_cast<char>(currentChar());
    nextChar();
  }

  // 4th IP component
  if (!consume('.')) return token_ = Token::Unknown;

  stringValue_ += '.';
  while (std::isdigit(currentChar())) {
    stringValue_ += static_cast<char>(currentChar());
    nextChar();
  }

  ipValue_.set(stringValue_.c_str(), util::IPAddress::Family::V4);

  if (currentChar() != '/') return token_ = Token::IP;

  // IPv4 CIDR
  return continueCidr(32);
}

Token Lexer::parseIdent() {
  stringValue_.clear();
  stringValue_ += static_cast<char>(currentChar());
  bool isHex = isHexChar();

  nextChar();

  while (std::isalnum(currentChar()) || currentChar() == '_' ||
         currentChar() == '.') {
    stringValue_ += static_cast<char>(currentChar());
    if (!isHexChar()) isHex = false;

    nextChar();
  }

  if (currentChar() == ':' && !isHex) {
    nextChar();  // skip ':'
    return Token::NamedParam;
  }

  // ipv6HexDigit4 *(':' ipv6HexDigit4) ['::' [ipv6HexSeq]]
  if (stringValue_.size() <= 4 && isHex && currentChar() == ':')
    return continueParseIPv6(true);

  if (stringValue_.size() < 4 && isHex && isHexChar())
    return continueParseIPv6(false);

  static struct {
    const char* symbol;
    Token token;
  } keywords[] = {{"in", Token::In},
                  {"var", Token::Var},
                  {"match", Token::Match},
                  {"on", Token::On},
                  {"for", Token::For},
                  {"do", Token::Do},
                  {"if", Token::If},
                  {"then", Token::Then},
                  {"else", Token::Else},
                  {"unless", Token::Unless},
                  {"import", Token::Import},
                  {"from", Token::From},
                  {"handler", Token::Handler},
                  {"and", Token::And},
                  {"or", Token::Or},
                  {"xor", Token::Xor},
                  {"not", Token::Not},
                  {"shl", Token::Shl},
                  {"shr", Token::Shr},
                  {"bool", Token::BoolType},
                  {"int", Token::NumberType},
                  {"string", Token::StringType},
                  {nullptr, Token::Unknown}};

  for (auto i = keywords; i->symbol; ++i)
    if (strcmp(i->symbol, stringValue_.c_str()) == 0) return token_ = i->token;

  if (stringValue_ == "true" || stringValue_ == "yes") {
    numberValue_ = 1;
    return token_ = Token::Boolean;
  }

  if (stringValue_ == "false" || stringValue_ == "no") {
    numberValue_ = 0;
    return token_ = Token::Boolean;
  }

  return token_ = Token::Ident;
}
// {{{ IPv6 address parser
// IPv6_HexPart ::= IPv6_HexSeq                        # (1)
//                | IPv6_HexSeq "::" [IPv6_HexSeq]     # (2)
//                            | "::" [IPv6_HexSeq]     # (3)
//
bool Lexer::ipv6HexPart() {
  bool rv;

  if (currentChar() == ':' && peekChar() == ':') {  // (3)
    stringValue_ = "::";
    nextChar();  // skip ':'
    nextChar();  // skip ':'
    rv = isHexChar() ? ipv6HexSeq() : true;
  } else if (!!(rv = ipv6HexSeq())) {
    if (currentChar() == ':' && peekChar() == ':') {  // (2)
      stringValue_ += "::";
      nextChar();  // skip ':'
      nextChar();  // skip ':'
      rv = isHexChar() ? ipv6HexSeq() : true;
    }
  }

  if (std::isalnum(currentChar_) || currentChar_ == ':') rv = false;

  return rv;
}

// 1*4HEXDIGIT *(':' 1*4HEXDIGIT)
bool Lexer::ipv6HexSeq() {
  if (!ipv6HexDigit4()) return false;

  while (currentChar() == ':' && peekChar() != ':') {
    stringValue_ += ':';
    nextChar();

    if (!ipv6HexDigit4()) return false;
  }

  return true;
}

// 1*4HEXDIGIT
bool Lexer::ipv6HexDigit4() {
  size_t i = ipv6HexDigits_;

  while (isHexChar()) {
    stringValue_ += static_cast<char>(currentChar());
    nextChar();
    ++i;
  }

  ipv6HexDigits_ = 0;

  return i >= 1 && i <= 4;
}

bool Lexer::continueParseRegEx(char delim) {
  int last = -1;

  stringValue_.clear();

  while (!eof() && (currentChar() != delim || (last == '\\'))) {
    stringValue_ += static_cast<char>(currentChar());
    last = currentChar();
    nextChar();
  }

  if (currentChar() == delim) {
    nextChar();
    token_ = Token::RegExp;
    return true;
  }

  token_ = Token::Unknown;
  return false;
}

// ipv6HexDigit4 *(':' ipv6HexDigit4) ['::' [ipv6HexSeq]]
// where the first component, ipv6HexDigit4 is already parsed
Token Lexer::continueParseIPv6(bool firstComplete) {
  bool rv = true;
  if (firstComplete) {
    while (currentChar() == ':' && peekChar() != ':') {
      stringValue_ += ':';
      nextChar();

      if (!ipv6HexDigit4()) {
        return token_ = Token::Unknown;
      }
    }

    if (currentChar() == ':' && peekChar() == ':') {
      stringValue_ += "::";
      nextChar();
      nextChar();
      rv = isHexChar() ? ipv6HexSeq() : true;
    }
  } else {
    ipv6HexDigits_ = stringValue_.size();
    rv = ipv6HexPart();
  }

  // parse embedded IPv4 remainer
  while (currentChar_ == '.' && std::isdigit(peekChar())) {
    stringValue_ += '.';
    nextChar();

    while (std::isdigit(currentChar_)) {
      stringValue_ += static_cast<char>(currentChar_);
      nextChar();
    }
  }

  if (!rv)
    // Invalid IPv6
    return token_ = Token::Unknown;

  if (!ipValue_.set(stringValue_.c_str(), util::IPAddress::Family::V6))
    // Invalid IPv6
    return token_ = Token::Unknown;

  if (currentChar_ != '/') return token_ = Token::IP;

  return continueCidr(128);
}

Token Lexer::continueCidr(size_t range) {
  // IPv6 CIDR
  nextChar();  // consume '/'

  if (!std::isdigit(currentChar())) {
    report_.tokenError(lastLocation(),
        "{}[{}:{}]: invalid byte {}",
        location_.filename,
        line(),
        column(),
        (int)(currentChar() & 0xFF));
    return token_ = Token::Unknown;
  }

  numberValue_ = 0;
  while (std::isdigit(currentChar())) {
    numberValue_ *= 10;
    numberValue_ += currentChar() - '0';
    stringValue_ += static_cast<char>(currentChar());
    nextChar();
  }

  if (numberValue_ > static_cast<decltype(numberValue_)>(range)) {
    report_.tokenError(lastLocation(),
                       "{}[{}:{}]: CIDR prefix out of range.",
                       location_.filename, line(), column());
    return token_ = Token::Unknown;
  }

  return token_ = Token::Cidr;
}
// }}}

}  // namespace flow
