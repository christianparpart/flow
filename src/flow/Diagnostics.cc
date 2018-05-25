// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/Diagnostics.h>
#include <fmt/format.h>
#include <iostream>

namespace flow::diagnostics {

// {{{ Message
std::string Message::string() const {
  switch (type) {
    case Type::Warning:
      return fmt::format("[{}] {}", sourceLocation, text);
    case Type::LinkError:
      return fmt::format("{}: {}", type, text);
    default:
      return fmt::format("[{}] {}: {}", sourceLocation, type, text);
  }
}

bool Message::operator==(const Message& other) const noexcept {
  // XXX ignore SourceLocation's filename & end
  return type == other.type &&
         sourceLocation.begin == other.sourceLocation.begin &&
         text == other.text;
}
// }}}
// {{{ ConsoleReport
ConsoleReport::ConsoleReport()
    : errorCount_{0} {
}

bool ConsoleReport::containsFailures() const noexcept {
  return errorCount_ != 0;
}

void ConsoleReport::push_back(Message message) {
  if (message.type != Type::Warning)
    errorCount_++;

  switch (message.type) {
    case Type::Warning:
      std::cerr << fmt::format("Warning: {}\n", message);
      break;
    default:
      std::cerr << fmt::format("Error: {}\n", message);
      break;
  }
}
// }}}
// {{{ BufferedReport
void BufferedReport::push_back(Message msg) {
  messages_.emplace_back(std::move(msg));
}

bool BufferedReport::containsFailures() const noexcept {
  return std::count_if(begin(), end(),
                       [](const Message& m) { return m.type != Type::Warning; })
         != 0;
}

void BufferedReport::clear() {
  messages_.clear();
}

void BufferedReport::log() const {
  for (const Message& message: messages_) {
    switch (message.type) {
      case Type::Warning:
        fmt::print("Warning: {}\n", message);
        break;
      default:
        fmt::print("Error: {}\n", message);
        break;
    }
  }
}

bool BufferedReport::operator==(const BufferedReport& other) const noexcept {
  if (size() != other.size())
    return false;

  for (size_t i = 0, e = size(); i != e; ++i)
    if (messages_[i] != other.messages_[i])
      return false;

  return true;
}

bool BufferedReport::contains(const Message& message) const noexcept {
  for (const Message& m: messages_)
    if (m == message)
      return true;

  return false;
}

DifferenceReport difference(const BufferedReport& first, const BufferedReport& second) {
  DifferenceReport diff;

  for (const Message& m: first)
    if (!second.contains(m))
      diff.first.push_back(m);

  for (const Message& m: second)
    if (!first.contains(m))
      diff.second.push_back(m);

  return diff;
}

std::ostream& operator<<(std::ostream& os, const BufferedReport& report) {
  for (const Message& message: report) {
    switch (message.type) {
      case Type::Warning:
        os << fmt::format("Warning: {}\n", message);
        break;
      default:
        os << fmt::format("Error: {}\n", message);
        break;
    }
  }
  return os;
}
// }}}

} // namespace flow::diagnostics
