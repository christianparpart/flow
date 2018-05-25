// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <flow/SourceLocation.h>

#include <algorithm>
#include <fmt/format.h>
#include <string>
#include <system_error>
#include <vector>

namespace flow::diagnostics {

enum class Type {
  TokenError,
  SyntaxError,
  TypeError,
  Warning,
  LinkError
};

struct Message {
  Type type;
  SourceLocation sourceLocation;
  std::string text;

  Message(Type ty, SourceLocation sl, std::string t) : type{ty}, sourceLocation{sl}, text{t} {}

  std::string string() const;

  bool operator==(const Message& other) const noexcept;
  bool operator!=(const Message& other) const noexcept { return !(*this == other); }
};

class Report {
 public:
  virtual ~Report() {}

  template<typename... Args> void tokenError(const SourceLocation& sloc, const std::string& f, Args... args) {
    emplace_back(Type::TokenError, sloc, fmt::format(f, std::move(args)...));
  }

  template<typename... Args> void syntaxError(const SourceLocation& sloc, const std::string& f, Args... args) {
    emplace_back(Type::SyntaxError, sloc, fmt::format(f, std::move(args)...));
  }

  template<typename... Args> void typeError(const SourceLocation& sloc, const std::string& f, Args... args) {
    emplace_back(Type::TypeError, sloc, fmt::format(f, std::move(args)...));
  }

  template<typename... Args> void warning(const SourceLocation& sloc, const std::string& f, Args... args) {
    emplace_back(Type::Warning, sloc, fmt::format(f, std::move(args)...));
  }

  template<typename... Args> void linkError(const std::string& f, Args... args) {
    emplace_back(Type::LinkError, SourceLocation{}, fmt::format(f, std::move(args)...));
  }

  void emplace_back(Type ty, SourceLocation sl, std::string t) {
    push_back(Message(ty, std::move(sl), std::move(t)));
  }

  virtual void push_back(Message msg) = 0;
  virtual bool containsFailures() const noexcept = 0;
};

using MessageList = std::vector<Message>;

class BufferedReport : public Report {
 public:
  void push_back(Message msg) override;
  bool containsFailures() const noexcept override;

  void log() const;

  const MessageList& messages() const noexcept { return messages_; }

  void clear();
  size_t size() const noexcept { return messages_.size(); }
  const Message& operator[](size_t i) const { return messages_[i]; }

  using iterator = MessageList::iterator;
  using const_iterator = MessageList::const_iterator;
  iterator begin() noexcept { return messages_.begin(); }
  iterator end() noexcept { return messages_.end(); }
  const_iterator begin() const noexcept { return messages_.begin(); }
  const_iterator end() const noexcept { return messages_.end(); }

  bool contains(const Message& m) const noexcept;

  bool operator==(const BufferedReport& other) const noexcept;
  bool operator!=(const BufferedReport& other) const noexcept { return !(*this == other); }

 private:
  MessageList messages_;
};

class ConsoleReport : public Report {
 public:
  ConsoleReport();

  void push_back(Message msg) override;
  bool containsFailures() const noexcept override;

 private:
  size_t errorCount_;
};

std::ostream& operator<<(std::ostream& os, const Report& report);

using DifferenceReport = std::pair<MessageList, MessageList>;

DifferenceReport difference(const BufferedReport& first, const BufferedReport& second);

class DiagnosticsError : public std::runtime_error {
 public:
  explicit DiagnosticsError(SourceLocation sloc, const std::string& msg) : std::runtime_error{msg} {}

  const SourceLocation& sourceLocation() const noexcept { return sloc_; }

 private:
  SourceLocation sloc_;
};

class LexerError : public DiagnosticsError {
 public:
  LexerError(SourceLocation sloc, const std::string& msg) : DiagnosticsError{sloc, msg} {}
};

class SyntaxError : public DiagnosticsError {
 public:
  SyntaxError(SourceLocation sloc, const std::string& msg) : DiagnosticsError{sloc, msg} {}
};

class TypeError : public DiagnosticsError {
 public:
  TypeError(SourceLocation sloc, const std::string& msg) : DiagnosticsError{sloc, msg} {}
};

} // namespace flow

namespace fmt {
  template<>
  struct formatter<flow::diagnostics::Type> {
    using Type = flow::diagnostics::Type;

    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const Type& v, FormatContext &ctx) {
      switch (v) {
        case Type::TokenError:
          return format_to(ctx.begin(), "TokenError");
        case Type::SyntaxError:
          return format_to(ctx.begin(), "SyntaxError");
        case Type::TypeError:
          return format_to(ctx.begin(), "TypeError");
        case Type::Warning:
          return format_to(ctx.begin(), "Warning");
        case Type::LinkError:
          return format_to(ctx.begin(), "LinkError");
        default:
          return format_to(ctx.begin(), "{}", static_cast<unsigned>(v));
      }
    }
  };
}

namespace fmt {
  template<>
  struct formatter<flow::diagnostics::Message> {
    using Message = flow::diagnostics::Message;

    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const Message& v, FormatContext &ctx) {
      return format_to(ctx.begin(), "{}", v.string());
    }
  };
}
