// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/util/Flags.h>
#include <fmt/format.h>
#include <sstream>
#include <iostream>
#include <iomanip>

extern char** environ;

namespace flow::util {

// {{{ Flags::Error
Flags::Error::Error(ErrorCode code, std::string arg)
    : std::runtime_error{fmt::format("{}: {}", 
          FlagsErrorCategory::get().message(static_cast<int>(code)), arg)},
      code_{code},
      arg_{std::move(arg)} {
}
// }}}

// {{{ Flag
Flags::Flag::Flag(
    const std::string& opt,
    const std::string& val,
    FlagStyle fs,
    FlagType ft)
    : type_(ft),
      style_(fs),
      name_(opt),
      value_(val) {
}
// }}}

Flags::Flags()
    : flagDefs_(),
      parametersEnabled_(false),
      parametersPlaceholder_(),
      parametersHelpText_(),
      set_(),
      raw_() {
}

void Flags::set(const Flag& flag) {
  set_[flag.name()] = std::make_pair(flag.type(), flag.value());
}

void Flags::set(const std::string& opt,
                const std::string& val,
                FlagStyle fs,
                FlagType ft) {
  set(Flag{opt, val, fs, ft});
}

bool Flags::isSet(const std::string& flag) const {
  return set_.find(flag) != set_.end();
}

std::string Flags::asString(const std::string& flag) const {
  auto i = set_.find(flag);
  if (i == set_.end())
    throw Error{ErrorCode::NotFound, flag};

  return i->second.second;
}

std::string Flags::getString(const std::string& flag) const {
  auto i = set_.find(flag);
  if (i == set_.end())
    throw Error{ErrorCode::NotFound, flag};

  if (i->second.first != FlagType::String)
    throw Error{ErrorCode::TypeMismatch, flag};

  return i->second.second;
}

long int Flags::getNumber(const std::string& flag) const {
  auto i = set_.find(flag);
  if (i == set_.end())
    throw Error{ErrorCode::NotFound, flag};

  if (i->second.first != FlagType::Number)
    throw Error{ErrorCode::TypeMismatch, flag};

  return std::stoi(i->second.second);
}

float Flags::getFloat(const std::string& flag) const {
  auto i = set_.find(flag);
  if (i == set_.end())
    throw Error{ErrorCode::NotFound, flag};

  if (i->second.first != FlagType::Float)
    throw Error{ErrorCode::TypeMismatch, flag};

  return std::stof(i->second.second);
}

bool Flags::getBool(const std::string& flag) const {
  auto i = set_.find(flag);
  if (i == set_.end())
    return false;

  return i->second.second == "true";
}

const std::vector<std::string>& Flags::parameters() const {
  return raw_;
}

void Flags::setParameters(const std::vector<std::string>& v) {
  raw_ = v;
}

std::string Flags::to_s() const {
  std::stringstream sstr;

  int i = 0;
  for (const std::pair<std::string, FlagValue>& flag: set_) {
    if (i)
      sstr << ' ';

    i++;

    switch (flag.second.first) {
      case FlagType::Bool:
        if (flag.second.second == "true")
          sstr << "--" << flag.first;
        else
          sstr << "--" << flag.first << "=false";
        break;
      case FlagType::String:
        sstr << "--" << flag.first << "=\"" << flag.second.second << "\"";
        break;
      default:
        sstr << "--" << flag.first << "=" << flag.second.second;
        break;
    }
  }

  return sstr.str();
}

Flags& Flags::define(
    const std::string& longOpt,
    char shortOpt,
    bool required,
    FlagType type,
    const std::string& valuePlaceholder,
    const std::string& helpText,
    const std::optional<std::string>& defaultValue,
    std::function<void(const std::string&)> callback) {

  FlagDef fd;
  fd.type = type;
  fd.longOption = longOpt;
  fd.shortOption = shortOpt;
  fd.required = required;
  fd.valuePlaceholder = valuePlaceholder;
  fd.helpText = helpText;
  fd.defaultValue = defaultValue;
  fd.callback = callback;

  flagDefs_.emplace_back(fd);

  return *this;
}

Flags& Flags::defineString(
    const std::string& longOpt,
    char shortOpt,
    const std::string& valuePlaceholder,
    const std::string& helpText,
    std::optional<std::string> defaultValue,
    std::function<void(const std::string&)> callback) {

  return define(longOpt, shortOpt, false, FlagType::String, valuePlaceholder,
                helpText, defaultValue, callback);
}

Flags& Flags::defineNumber(
    const std::string& longOpt,
    char shortOpt,
    const std::string& valuePlaceholder,
    const std::string& helpText,
    std::optional<long int> defaultValue,
    std::function<void(long int)> callback) {

  return define(
      longOpt, shortOpt, false, FlagType::Number, valuePlaceholder,
      helpText, defaultValue.has_value() ? std::make_optional(std::to_string(*defaultValue))
                                         : std::nullopt,
      [=](const std::string& value) {
        if (callback) {
          callback(std::stoi(value));
        }
      });
}

Flags& Flags::defineFloat(
    const std::string& longOpt,
    char shortOpt,
    const std::string& valuePlaceholder,
    const std::string& helpText,
    std::optional<float> defaultValue,
    std::function<void(float)> callback) {

  return define(
      longOpt, shortOpt, false, FlagType::Float, valuePlaceholder,
      helpText, defaultValue.has_value() ? std::make_optional(std::to_string(*defaultValue))
                                         : std::nullopt,
      [=](const std::string& value) {
        if (callback) {
          callback(std::stof(value));
        }
      });
}

Flags& Flags::defineBool(
    const std::string& longOpt,
    char shortOpt,
    const std::string& helpText,
    std::function<void(bool)> callback) {

  return define(
      longOpt, shortOpt, false, FlagType::Bool, "<bool>",
      helpText, std::nullopt,
      [=](const std::string& value) {
        if (callback) {
          callback(value == "true");
        }
      });
}

Flags& Flags::enableParameters(const std::string& valuePlaceholder,
                           const std::string& helpText) {
  parametersEnabled_ = true;
  parametersPlaceholder_ = valuePlaceholder;
  parametersHelpText_ = helpText;

  return *this;
}

const Flags::FlagDef* Flags::findDef(const std::string& longOption) const {
  for (const auto& flag: flagDefs_)
    if (flag.longOption == longOption)
      return &flag;

  return nullptr;
}

const Flags::FlagDef* Flags::findDef(char shortOption) const {
  for (const auto& flag: flagDefs_)
    if (flag.shortOption == shortOption)
      return &flag;

  return nullptr;
}

// -----------------------------------------------------------------------------
void Flags::parse(int argc, const char* argv[]) {
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i)
    args.push_back(argv[i]);

  parse(args);
}

std::error_code Flags::tryParse(const std::vector<std::string>& args) {
  try {
    parse(args);
  } catch (const Error& parseError) {
    return parseError.code();
  }
  return std::error_code();
}

void Flags::parse(const std::vector<std::string>& args) {
  auto invokeCallback = [&](const FlagDef* fd, FlagStyle style, const std::string& value) {
    if (fd) {
      set(fd->longOption, value, style, fd->type);
      if (fd->callback) {
        fd->callback(value);
      }
    }
  };

  enum class ParsingState {
    Options,
    Parameters,
  };

  std::vector<std::string> params;
  ParsingState pstate = ParsingState::Options;
  size_t i = 0;

  while (i < args.size()) {
    std::string arg = args[i];
    i++;
    if (pstate == ParsingState::Parameters) {
      params.push_back(arg);
    } else if (arg == "--") {
      if (parametersEnabled_) {
        pstate = ParsingState::Parameters;
      } else {
        throw Error{ErrorCode::UnknownOption, arg};
      }
    } else if (arg.size() > 2 && arg[0] == '-' && arg[1] == '-') {
      // longopt
      std::string name = arg.substr(2);
      size_t eq = name.find('=');
      if (eq != name.npos) { // --name=value
        std::string value = name.substr(eq + 1);
        name = name.substr(0, eq);
        const FlagDef* fd = findDef(name);
        if (fd == nullptr) {
          throw Error{ErrorCode::UnknownOption, arg};
        } else {
          invokeCallback(fd, FlagStyle::LongWithValue, value);
        }
      } else { // --name [VALUE]
        const FlagDef* fd = findDef(name);
        if (fd == nullptr) {
          throw Error{ErrorCode::UnknownOption, arg};
        } else if (fd->type == FlagType::Bool) { // --name
          invokeCallback(fd, FlagStyle::LongSwitch, "true");
        } else { // --name VALUE
          if (i >= args.size())
            throw Error{ErrorCode::MissingOption, arg};

          std::string value = args[i];
          i++;

          invokeCallback(fd, FlagStyle::LongWithValue, value);
        }
      }
    } else if (arg.size() > 1 && arg[0] == '-') {
      // shortopt
      arg = arg.substr(1);
      while (!arg.empty()) {
        const FlagDef* fd = findDef(arg[0]);
        if (fd == nullptr) { // option not found
          throw Error{ErrorCode::UnknownOption, "-" + arg.substr(0, 1)};
        } else if (fd->type == FlagType::Bool) {
          invokeCallback(fd, FlagStyle::ShortSwitch, "true");
          arg = arg.substr(1);
        } else if (arg.size() > 1) { // -fVALUE
          std::string value = arg.substr(1);
          invokeCallback(fd, FlagStyle::ShortSwitch, value);
          arg.clear();
        } else { // -f VALUE
          std::string name = fd->longOption;

          if (i >= args.size()) {
            char option[3] = { '-', fd->shortOption, '\0' };
            throw Error{ErrorCode::MissingOptionValue, option};
          }

          arg.clear();
          std::string value = args[i];
          i++;

          if (!value.empty() && value[0] == '-') {
            char option[3] = { '-', fd->shortOption, '\0' };
            throw Error{ErrorCode::MissingOptionValue, option};
          }

          invokeCallback(fd, FlagStyle::ShortSwitch, value);
        }
      }
    } else if (parametersEnabled_) {
      params.push_back(arg);
    } else {
      // oops
      throw Error{ErrorCode::UnknownOption, arg};
    }
  }

  setParameters(params);

  // fill any missing default flags
  for (const FlagDef& fd: flagDefs_) {
    if (fd.defaultValue.has_value()) {
      if (!isSet(fd.longOption)) {
        invokeCallback(&fd, FlagStyle::LongWithValue, fd.defaultValue.value());
      }
    } else if (fd.type == FlagType::Bool) {
      if (!isSet(fd.longOption)) {
        invokeCallback(&fd, FlagStyle::LongWithValue, "false");
      }
    }
  }
}

// -----------------------------------------------------------------------------

std::string Flags::helpText(size_t width, size_t helpTextOffset) const {
  std::stringstream sstr;

  for (const FlagDef& fd: flagDefs_)
    sstr << fd.makeHelpText(width, helpTextOffset);

  if (parametersEnabled_) {
    sstr << std::endl;

    size_t p = sstr.tellp();
    sstr << "    [--] " << parametersPlaceholder_;
    size_t column = static_cast<size_t>(sstr.tellp()) - p;

    if (column < helpTextOffset)
      sstr << std::setw(helpTextOffset - column) << ' ';
    else
      sstr << std::endl << std::setw(helpTextOffset) << ' ';

    sstr << parametersHelpText_ << std::endl;
  }

  return sstr.str();
}

static std::string wordWrap(
    const std::string& text,
    size_t currentWidth,
    size_t width,
    size_t indent) {

  std::stringstream sstr;

  size_t i = 0;
  while (i < text.size()) {
    if (currentWidth >= width) {
      sstr << std::endl << std::setw(indent) << ' ';
      currentWidth = 0;
    }

    sstr << text[i];
    currentWidth++;
    i++;
  }

  return sstr.str();
}

std::error_code make_error_code(Flags::ErrorCode errc) {
  return std::error_code(static_cast<int>(errc), FlagsErrorCategory::get());
}

// {{{ Flags::FlagDef
std::string Flags::FlagDef::makeHelpText(size_t width,
                                         size_t helpTextOffset) const {
  std::stringstream sstr;

  sstr << ' ';

  // short option
  if (shortOption)
    sstr << "-" << shortOption << ", ";
  else
    sstr << "    ";

  // long option
  sstr << "--" << longOption;

  // value placeholder
  if (type != FlagType::Bool) {
    if (!valuePlaceholder.empty()) {
      sstr << "=" << valuePlaceholder;
    } else {
      sstr << "=VALUE";
    }
  }

  // spacer
  size_t column = sstr.tellp();
  if (column < helpTextOffset) {
    sstr << std::setw(helpTextOffset - sstr.tellp()) << ' ';
  } else {
    sstr << std::endl << std::setw(helpTextOffset) << ' ';
    column = helpTextOffset;
  }

  // help output with default value hint.
  if (type != FlagType::Bool && defaultValue.has_value()) {
    sstr << wordWrap(helpText + " [" + *defaultValue + "]",
                     column, width, helpTextOffset);
  } else {
    sstr << wordWrap(helpText, column, width, helpTextOffset);
  }

  sstr << std::endl;

  return sstr.str();
}
// }}}

// {{{ FlagsErrorCategory
FlagsErrorCategory& FlagsErrorCategory::get() {
  static FlagsErrorCategory cat;
  return cat;
}

const char* FlagsErrorCategory::name() const noexcept {
  return "Flags";
}

std::string FlagsErrorCategory::message(int ec) const {
  switch (static_cast<Flags::ErrorCode>(ec)) {
  case Flags::ErrorCode::TypeMismatch:
    return "Type Mismatch";
  case Flags::ErrorCode::UnknownOption:
    return "Unknown Option";
  case Flags::ErrorCode::MissingOption:
    return "Missing Option";
  case Flags::ErrorCode::MissingOptionValue:
    return "Missing Option Value";
  case Flags::ErrorCode::NotFound:
    return "Flag Not Found";
  default:
    return "<UNKNOWN>";
  }
}
// }}}

}  // namespace flow::util
