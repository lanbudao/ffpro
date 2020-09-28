#ifndef AVLOG_H
#define AVLOG_H

#include "sdk/DPTR.h"
#include "sdk/global.h"

NAMESPACE_BEGIN

#ifdef MESSAGELOGCONTEXT
  #define MESSAGELOG_FILE __FILE__
  #define MESSAGELOG_LINE __LINE__
  #define MESSAGELOG_FUNC __FUNCSIG__
#else
  #define MESSAGELOG_FILE nullptr
  #define MESSAGELOG_LINE 0
  #define MESSAGELOG_FUNC nullptr
#endif

/**
 * @brief Do not show Timestamp, thread id and log level defaultly
 */
#define HIDE_LOG_DETAIL

#define AVDebug      MessageLogger(MESSAGELOG_FILE, MESSAGELOG_LINE, MESSAGELOG_FUNC).debug
#define AVInfo       MessageLogger(MESSAGELOG_FILE, MESSAGELOG_LINE, MESSAGELOG_FUNC).info
#define AVWarning    MessageLogger(MESSAGELOG_FILE, MESSAGELOG_LINE, MESSAGELOG_FUNC).warning
#define AVError      MessageLogger(MESSAGELOG_FILE, MESSAGELOG_LINE, MESSAGELOG_FUNC).error
#define AVFatal      MessageLogger(MESSAGELOG_FILE, MESSAGELOG_LINE, MESSAGELOG_FUNC).fatal

/**
 * @brief If you need to print the output to the local file, you need to
 * call 'setLogFile' to specify the file to print.
 */

class FFPRO_EXPORT MessageLogContext
{
public:
    MessageLogContext()
        : version(2), line(0), file(nullptr), function(nullptr), category(nullptr) {}
    MessageLogContext(const char *fileName, int lineNumber, const char *functionName, const char *categoryName)
        : version(2), line(lineNumber), file(fileName), function(functionName), category(categoryName) {}

    int version;
    int line;
    const char *file;
    const char *function;
    const char *category;

private:
    friend class MessageLogger;
    friend class Debug;
};

class DebugPrivate;
class FFPRO_EXPORT Debug
{
    DPTR_DECLARE_PRIVATE(Debug)
public:
    Debug(LogLevel level = LogWarning);
    ~Debug();

    Debug log(const char *fmt, va_list list);
    Debug &operator<<(bool arg);
    Debug &operator<<(char arg);
    Debug &operator<<(int16_t arg);
    Debug &operator<<(uint16_t arg);
    Debug &operator<<(int32_t arg);
    Debug &operator<<(uint32_t arg);
    Debug &operator<<(int64_t arg);
    Debug &operator<<(uint64_t arg);
    Debug &operator<<(double arg);
    Debug &operator<<(const char *arg);
    Debug &operator<<(const std::string &arg);

    void setContext(MessageLogContext &ctx);

private:
    DPTR_DECLARE(Debug)
};

class FFPRO_EXPORT MessageLogger
{
    DISABLE_COPY(MessageLogger)
public:
    MessageLogger();
    MessageLogger(const char *file, int line, const char *function);

    void debug(const char *msg, ...);
    void info(const char *msg, ...);
    void warning(const char *msg, ...);
    void error(const char *msg, ...);
    void fatal(const char *msg, ...);

    Debug debug() const;
    Debug info() const;
    Debug warning() const;
    Debug error() const;
    Debug fatal() const;

private:
    MessageLogContext context;

};

/// Set log level, defalt WARN.
void FFPRO_EXPORT setLogLevel(LogLevel level);

/// Get log level.
LogLevel FFPRO_EXPORT getLogLevel();

/// Set log file (basename and dir, no suffix), defalut `./run`.
/// The Program will automatically add suffix (`.log`).
void FFPRO_EXPORT setLogFile(const char *file);

void FFPRO_EXPORT setRollSize(uint32_t MB);

NAMESPACE_END
#endif //Debug_H
