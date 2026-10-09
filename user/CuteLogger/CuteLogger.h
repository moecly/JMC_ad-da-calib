#pragma once

#include "OutStreamAbstract.h"
#include "TaskLock.h"

#define LogTrace CuteLogger(__FILE__, __FUNCTION__, __LINE__, Logger::TraceLevel)
#define LogDebug CuteLogger(__FILE__, __FUNCTION__, __LINE__, Logger::DebugLevel)
#define LogInfo CuteLogger(__FILE__, __FUNCTION__, __LINE__, Logger::InfoLevel)
#define LogWarn CuteLogger(__FILE__, __FUNCTION__, __LINE__, Logger::WarnLevel)
#define LogError CuteLogger(__FILE__, __FUNCTION__, __LINE__, Logger::ErrorLevel)
#define LogFatal CuteLogger(__FILE__, __FUNCTION__, __LINE__, Logger::FatalLevel)

class Logger {
public:
    enum LogLevel {
        TraceLevel,
        DebugLevel,
        InfoLevel,
        WarnLevel,
        ErrorLevel,
        FatalLevel,
    };
    Logger(LogLevel logLevel=FatalLevel);
    ~Logger();
    static Logger& GetInstance();
    void Write(const char* file, const char* function, const int line, const char* message, const Logger::LogLevel logLevel);
    void RegisterOutStream(OutStreamAbstract* outStream);
    void SetLogLevel(const LogLevel logLevel);
private:
    const char* FormatData(const char* file, const char* function, const int line, const char* message, const Logger::LogLevel logLevel);

    OutStreamAbstract *m_outStream;
    LogLevel m_logLevel;
    TaskLock m_lock;
    const char* m_newLine;
};

Logger& GetLoggerInstance();

class CuteLogger {
public: 
    CuteLogger(const char* file, const char* function, const int line, const Logger::LogLevel logLevel);
    ~CuteLogger();
    CuteLogger& operator<<(const char* message);
    CuteLogger& operator<<(int value);
    CuteLogger& operator<<(unsigned int value);
    CuteLogger& operator<<(long value);
    CuteLogger& operator<<(unsigned long value);
    CuteLogger& operator<<(char value);
    CuteLogger& operator<<(unsigned char value);
private:
    static constexpr int kMessageSize = 256;
    const char* m_file;
    const char* m_function;
    const int m_line;
    char m_message[kMessageSize];
    const Logger::LogLevel m_logLevel;
    int m_length;
};
