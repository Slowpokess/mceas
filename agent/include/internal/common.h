#ifndef MCEAS_COMMON_H
#define MCEAS_COMMON_H

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <functional>

// Определение версии агента
#define MCEAS_AGENT_VERSION "0.1.0"

// Определение пространств имен
namespace mceas {

// Типы модулей
enum class ModuleType {
    BROWSER,    // Анализ браузеров
    CRYPTO,     // Анализ криптокошельков
    FILE,       // Файловая телеметрия
    BEHAVIOR    // Поведенческий аудит
};

// Коды состояния агента
enum class StatusCode {
    SUCCESS,
    ERROR_CONFIGURATION,
    ERROR_MODULE_LOAD,
    ERROR_COMMUNICATION,
    ERROR_SECURITY,
    ERROR_SYSTEM
};

// Уровни логирования
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

} // namespace mceas

#endif // MCEAS_COMMON_H