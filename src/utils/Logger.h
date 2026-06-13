#ifndef LOGGER_H
#define LOGGER_H

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

/**
 * @brief Represents the severity level of a log message.
 */
enum class LogLevel
{
  TRACE,
  DEBUG,
  INFO,
  WARNING,
  ERROR,
  CRITICAL
};

/**
 * @brief Tracks the location (file, line, column) in a source script.
 */
struct SourceLocation
{
  std::string file;
  int line;
  int column;

  SourceLocation(std::string f = "<unknown>", int l = 0, int c = 0)
      : file(std::move(f)), line(l), column(c)
  {}

  /** @return True if the location data is valid and initialized. */
  bool isValid() const
  {
    return !file.empty() && file != "<unknown>";
  }
};

/**
 * @brief Exception thrown by the logger to indicate a controlled runtime fault.
 */
struct LoggerException : public std::runtime_error
{
  SourceLocation location;
  std::string errorType;

  LoggerException(std::string message, SourceLocation loc = {}, std::string type = "Error")
      : std::runtime_error(std::move(message)), location(std::move(loc)), errorType(std::move(type))
  {}
};

/**
 * @brief Represents a single frame in the interpreter's call stack.
 */
struct StackFrame
{
  std::string function;
  SourceLocation location;

  StackFrame(std::string fn, SourceLocation loc) : function(std::move(fn)), location(std::move(loc))
  {}
};

/**
 * @brief Runtime error exception containing a full stack trace.
 */
struct RuntimeError : public std::runtime_error
{
  std::vector<StackFrame> trace;

  RuntimeError(std::string message, std::vector<StackFrame> trace_)
      : std::runtime_error(std::move(message)), trace(std::move(trace_))
  {}
};

/**
 * @brief Static utility for logging diagnostic information and runtime errors.
 */
class Logger
{
public:
  static void configure(LogLevel minLevel = LogLevel::INFO, bool useTimestamps = true);
  static void setOutputStreams(std::ostream* out, std::ostream* err);
  static bool isEnabled(LogLevel level);

  static void log(LogLevel level,
                  const std::string& message,
                  const SourceLocation& loc = {},
                  const std::string& errorType = "Error");
  static void debug(const std::string& message, const SourceLocation& loc = {});
  static void info(const std::string& message, const SourceLocation& loc = {});
  static void warning(const std::string& message, const SourceLocation& loc = {});
  static void error(const std::string& message,
                    const SourceLocation& loc = {},
                    const std::string& errorType = "RuntimeError");
  static void error(const std::string& message,
                    const std::vector<StackFrame>& trace,
                    const SourceLocation& loc = {},
                    const std::string& errorType = "RuntimeError");

  static std::vector<StackFrame> captureTrace();
  static void pushFrame(const std::string& function, const SourceLocation& loc);
  static void popFrame();
  static void printTraceback(const std::string& errorType, const std::string& message);
  static void printTraceback(const std::string& errorType,
                             const std::string& message,
                             const std::vector<StackFrame>& trace);
  static void setCurrentFile(const std::string& file);
  static std::string getCurrentFile();

private:
  static std::string timestamp();
  static std::string formatLocation(const SourceLocation& loc);
  static std::string levelToString(LogLevel level);
  static std::vector<StackFrame> callStack;
  static std::string currentFile;
  static LogLevel minLevel;
  static bool timestampsEnabled;
  static std::ostream* outStream;
  static std::ostream* errStream;
};

#endif // LOGGER_H
