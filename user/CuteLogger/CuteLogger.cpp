#include "CuteLogger.h"

#include <cstring>
#include <cstdio>

Logger::Logger(LogLevel logLevel)
 : m_logLevel(logLevel),
   m_newLine("\r\n") {
}

Logger::~Logger() {}

Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

void Logger::Write(const char* file, const char* function, const int line, const char* message, const Logger::LogLevel logLevel) {
    if (logLevel < m_logLevel) return;
    m_lock.Lock();
    const char* data = FormatData(file, function, line, message, logLevel);
    m_outStream->Write(data);
    delete[] data;
    m_lock.UnLock();
}

void Logger::RegisterOutStream(OutStreamAbstract* outStream) {
    if (outStream == nullptr) return;
    m_outStream = outStream;
}

void Logger::SetLogLevel(const LogLevel logLevel) {
    m_logLevel = logLevel;
}

namespace {
const char* BaseName(const char* path) {
    const char* base = path;
    for (const char* p = path; *p; p++) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }
    return base;
}
}  // namespace

const char* Logger::FormatData(const char* file, const char* function, const int line, const char* message, const Logger::LogLevel logLevel) {
    const int bufferSize = 2048;
    char* buffer = new char[bufferSize];
    memset(buffer, 0, bufferSize);

    const char* levelStr;
    switch (logLevel) {
    case Logger::TraceLevel:
        levelStr = "TRACE";
        break;
    case Logger::DebugLevel:
        levelStr = "DEBUG";
        break;
    case Logger::InfoLevel:
        levelStr = "INFO";
        break;
    case Logger::WarnLevel:
        levelStr = "WARNING";
        break;
    case Logger::ErrorLevel:
        levelStr = "ERROR";
        break;
    case Logger::FatalLevel:
        levelStr = "FATAL";
        break;
    default:
        levelStr = "UNKNOWN";
        break;
    }

    snprintf(buffer, bufferSize, "[%s] %s:%d (%s): %s%s", levelStr, BaseName(file), line, function, message, m_newLine);
    return buffer;
}

Logger& GetLoggerInstance() {
    return Logger::GetInstance();
}

CuteLogger::CuteLogger(const char *file, const char *function, const int line, const Logger::LogLevel logLevel, const int maxSize) 
 : m_file(file),
   m_function(function),
   m_line(line),
   m_logLevel(logLevel),
   m_maxSize(maxSize),
   m_length(0) {
    m_message = new char[maxSize];
    memset(m_message, 0, maxSize);
}

CuteLogger::~CuteLogger() {
    if (m_message) {
        GetLoggerInstance().Write(m_file, m_function, m_line, m_message, m_logLevel);
        delete[] m_message;
    }
}

CuteLogger& CuteLogger::operator<<(const char* message) {
    size_t len = strlen(message);
    if (m_length + len > m_maxSize) return *this;
    strcat(m_message, message);
    m_length += len;
    return *this;
}

CuteLogger& CuteLogger::operator<<(int value) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", value);
    size_t len = strlen(buffer);
    if (m_length + len > m_maxSize) return *this;
    strcat(m_message, buffer);
    m_length += len;
    return *this;
}

CuteLogger& CuteLogger::operator<<(unsigned int value) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%u", value);
    size_t len = strlen(buffer);
    if (m_length + len > m_maxSize) return *this;
    strcat(m_message, buffer);
    m_length += len;
    return *this;
}

CuteLogger& CuteLogger::operator<<(long value) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%ld", value);
    size_t len = strlen(buffer);
    if (m_length + len > m_maxSize) return *this;
    strcat(m_message, buffer);
    m_length += len;
    return *this;
}

CuteLogger& CuteLogger::operator<<(unsigned long value) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%lu", value);
    size_t len = strlen(buffer);
    if (m_length + len > m_maxSize) return *this;
    strcat(m_message, buffer);
    m_length += len;
    return *this;
}

CuteLogger& CuteLogger::operator<<(char value) {
    char buffer[2];
    buffer[0] = value;
    buffer[1] = '\0';
    size_t len = 1;
    if (m_length + len > m_maxSize) return *this;
    strcat(m_message, buffer);
    m_length += len;
    return *this;
}

CuteLogger& CuteLogger::operator<<(unsigned char value) {
    char buffer[2];
    buffer[0] = static_cast<char>(value);
    buffer[1] = '\0';
    size_t len = 1;
    if (m_length + len > m_maxSize) return *this;
    strcat(m_message, buffer);
    m_length += len;
    return *this;
}

