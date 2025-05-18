#ifndef MCEAS_LOGGING_H
#define MCEAS_LOGGING_H

#include "internal/common.h"
#include <string>
#include <iostream>
#include <fstream>
#include <mutex>
#include <chrono>
#include <sstream>

namespace mceas {

// Глобальный класс логирования
class Logger {
public:
    // Получение экземпляра логгера (singleton)
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }
    
    // Установка уровня логирования
    void setLogLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(mutex_);
        log_level_ = level;
    }
    
    // Установка файла для логирования
    bool setLogFile(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (log_file_.is_open()) {
            log_file_.close();
        }
        
        log_file_.open(path, std::ios::out | std::ios::app);
        return log_file_.is_open();
    }
    
    // Запись сообщения в лог
    void log(LogLevel level, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Если уровень сообщения ниже установленного, не логируем
        if (level < log_level_) {
            return;
        }
        
        // Формируем строку с временной меткой и уровнем сообщения
        std::string level_str;
        switch (level) {
            case LogLevel::DEBUG:   level_str = "DEBUG";   break;
            case LogLevel::INFO:    level_str = "INFO";    break;
            case LogLevel::WARNING: level_str = "WARNING"; break;
            case LogLevel::ERROR:   level_str = "ERROR";   break;
            case LogLevel::CRITICAL:level_str = "CRITICAL";break;
            default:                level_str = "UNKNOWN"; break;
        }
        
        // Получаем текущее время
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
        
        // Формируем полное сообщение
        std::string full_message = ss.str() + " [" + level_str + "] " + message;
        
        // Выводим в консоль
        std::cout << full_message << std::endl;
        
        // Если файл открыт, пишем и в него
        if (log_file_.is_open()) {
            log_file_ << full_message << std::endl;
            log_file_.flush();
        }
    }
    
private:
    // Конструктор, деструктор и операторы скрыты для singleton
    Logger() : log_level_(LogLevel::INFO) {}
    ~Logger() {
        if (log_file_.is_open()) {
            log_file_.close();
        }
    }
    
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    // Уровень логирования
    LogLevel log_level_;
    
    // Файл для логирования
    std::ofstream log_file_;
    
    // Мьютекс для потокобезопасности
    std::mutex mutex_;
};

// Удобные функции-обертки для различных уровней логирования
inline void LogDebug(const std::string& message) {
    Logger::getInstance().log(LogLevel::DEBUG, message);
}

inline void LogInfo(const std::string& message) {
    Logger::getInstance().log(LogLevel::INFO, message);
}

inline void LogWarning(const std::string& message) {
    Logger::getInstance().log(LogLevel::WARNING, message);
}

inline void LogError(const std::string& message) {
    Logger::getInstance().log(LogLevel::ERROR, message);
}

inline void LogCritical(const std::string& message) {
    Logger::getInstance().log(LogLevel::CRITICAL, message);
}

} // namespace mceas

#endif // MCEAS_LOGGING_H