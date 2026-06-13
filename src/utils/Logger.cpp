#include "Logger.h"

#include <algorithm>
#include <ctime>
#include <format>

std::vector<StackFrame> Logger::callStack;
std::string Logger::currentFile = "<unknown>";
LogLevel Logger::minLevel = LogLevel::INFO;
bool Logger::timestampsEnabled = true;
std::ostream* Logger::outStream = &std::cout;
std::ostream* Logger::errStream = &std::cerr;

std::string Logger::timestamp()
{
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#if defined(_MSC_VER)
  localtime_s(&tm, &time);
#else
  localtime_r(&time, &tm);
#endif
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
  return oss.str();
}

std::string Logger::formatLocation(const SourceLocation& loc)
{
  if (!loc.isValid()) {
    return "";
  }

  if (loc.line > 0 && loc.column > 0) {
    return std::format("{}:{}:{}", loc.file, loc.line, loc.column);
  }
  if (loc.line > 0) {
    return std::format("{}:{}", loc.file, loc.line);
  }
  return loc.file;
}

std::string Logger::levelToString(LogLevel level)
{
  switch (level) {
    case LogLevel::TRACE:
      return "TRACE";
    case LogLevel::DEBUG:
      return "DEBUG";
    case LogLevel::INFO:
      return "INFO";
    case LogLevel::WARNING:
      return "WARNING";
    case LogLevel::ERROR:
      return "ERROR";
    case LogLevel::CRITICAL:
      return "CRITICAL";
    default:
      return "UNKNOWN";
  }
}

void Logger::configure(LogLevel minLevelValue, bool useTimestamps)
{
  minLevel = minLevelValue;
  timestampsEnabled = useTimestamps;
}

void Logger::setOutputStreams(std::ostream* out, std::ostream* err)
{
  if (out) {
    outStream = out;
  }
  if (err) {
    errStream = err;
  }
}

bool Logger::isEnabled(LogLevel level)
{
  return static_cast<int>(level) >= static_cast<int>(minLevel);
}

void Logger::log(LogLevel level,
                 const std::string& message,
                 const SourceLocation& loc,
                 const std::string& errorType)
{
  if (!isEnabled(level)) {
    return;
  }

  std::ostream& output =
      (level == LogLevel::INFO || level == LogLevel::DEBUG || level == LogLevel::TRACE)
          ? *outStream
          : *errStream;

  std::string prefix;
  if (timestampsEnabled) {
    prefix = std::format("[{}] ", timestamp());
  }
  prefix += std::format("[{}]", levelToString(level));

  if (level == LogLevel::ERROR || level == LogLevel::CRITICAL) {
    if (!callStack.empty()) {
      output << "Traceback (most recent call last):" << std::endl;
      for (auto it = callStack.rbegin(); it != callStack.rend(); ++it) {
        const auto& frame = *it;
        std::string location = formatLocation(frame.location);
        output << std::format("  at {} {}",
                              (frame.function.empty() ? "<module>" : frame.function),
                              (location.empty() ? "" : "(" + location + ")"))
               << std::endl;
      }
    }

    output << std::format("{}: {}", errorType, message) << std::endl;

    if (callStack.empty()) {
      SourceLocation effectiveLoc = loc;
      if (!effectiveLoc.isValid() && currentFile != "<unknown>") {
        effectiveLoc = SourceLocation(currentFile, 0, 0);
      }

      if (effectiveLoc.isValid()) {
        output << std::format("    at {}", formatLocation(effectiveLoc)) << std::endl;
      }
    }
  }
  else {
    output << prefix;
    if (loc.isValid()) {
      output << std::format(" {}:", formatLocation(loc));
    }
    output << " " << message << std::endl;
  }
}

void Logger::debug(const std::string& message, const SourceLocation& loc)
{
  log(LogLevel::DEBUG, message, loc, "Debug");
}

void Logger::info(const std::string& message, const SourceLocation& loc)
{
  log(LogLevel::INFO, message, loc, "Info");
}

void Logger::warning(const std::string& message, const SourceLocation& loc)
{
  log(LogLevel::WARNING, message, loc, "Warning");
}

void Logger::error(const std::string& message,
                   const SourceLocation& loc,
                   const std::string& errorType)
{
  log(LogLevel::ERROR, message, loc, errorType);
}

void Logger::error(const std::string& message,
                   const std::vector<StackFrame>& trace,
                   const SourceLocation& loc,
                   const std::string& errorType)
{
  if (!isEnabled(LogLevel::ERROR)) {
    return;
  }

  std::ostream& output = *errStream;
  output << "Traceback (most recent call last):" << std::endl;
  for (auto it = trace.rbegin(); it != trace.rend(); ++it) {
    const auto& frame = *it;
    std::string location = formatLocation(frame.location);
    output << std::format("  at {} {}",
                          (frame.function.empty() ? "<module>" : frame.function),
                          (location.empty() ? "" : "(" + location + ")"))
           << std::endl;
  }
  output << std::format("{}: {}", errorType, message) << std::endl;

  if (trace.empty()) {
    SourceLocation effectiveLoc = loc;
    if (!effectiveLoc.isValid() && currentFile != "<unknown>") {
      effectiveLoc = SourceLocation(currentFile, 0, 0);
    }

    if (effectiveLoc.isValid()) {
      output << std::format("    at {}", formatLocation(effectiveLoc)) << std::endl;
    }
  }
}

std::vector<StackFrame> Logger::captureTrace()
{
  return callStack;
}

void Logger::pushFrame(const std::string& function, const SourceLocation& loc)
{
  callStack.emplace_back(function, loc);
}

void Logger::popFrame()
{
  if (!callStack.empty()) {
    callStack.pop_back();
  }
}

void Logger::printTraceback(const std::string& errorType, const std::string& message)
{
  if (!isEnabled(LogLevel::ERROR)) {
    return;
  }

  std::cerr << "Traceback (most recent call last):" << std::endl;
  for (auto it = callStack.rbegin(); it != callStack.rend(); ++it) {
    const auto& frame = *it;
    std::string location = formatLocation(frame.location);
    std::cerr << std::format("  at {} {}",
                             (frame.function.empty() ? "<module>" : frame.function),
                             (location.empty() ? "" : "(" + location + ")"))
              << std::endl;
  }
  std::cerr << std::format("{}: {}", errorType, message) << std::endl;
}

void Logger::printTraceback(const std::string& errorType,
                            const std::string& message,
                            const std::vector<StackFrame>& trace)
{
  if (!isEnabled(LogLevel::ERROR)) {
    return;
  }

  std::cerr << "Traceback (most recent call last):" << std::endl;
  for (auto it = trace.rbegin(); it != trace.rend(); ++it) {
    const auto& frame = *it;
    std::string location = formatLocation(frame.location);
    std::cerr << std::format("  at {} {}",
                             (frame.function.empty() ? "<module>" : frame.function),
                             (location.empty() ? "" : "(" + location + ")"))
              << std::endl;
  }
  std::cerr << std::format("{}: {}", errorType, message) << std::endl;
}

void Logger::setCurrentFile(const std::string& file)
{
  currentFile = file.empty() ? "<unknown>" : file;
}

std::string Logger::getCurrentFile()
{
  return currentFile;
}
