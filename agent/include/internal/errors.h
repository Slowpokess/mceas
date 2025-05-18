#ifndef MCEAS_ERRORS_H
#define MCEAS_ERRORS_H

#include <string>
#include <stdexcept>

namespace mceas {

// Базовый класс для исключений MCEAS
class MCEASException : public std::runtime_error {
public:
    explicit MCEASException(const std::string& message) : std::runtime_error(message) {}
};

// Исключение для ошибок конфигурации
class ConfigurationException : public MCEASException {
public:
    explicit ConfigurationException(const std::string& message) : MCEASException("Configuration error: " + message) {}
};

// Исключение для ошибок модулей
class ModuleException : public MCEASException {
public:
    explicit ModuleException(const std::string& message) : MCEASException("Module error: " + message) {}
};

// Исключение для ошибок коммуникации
class CommunicationException : public MCEASException {
public:
    explicit CommunicationException(const std::string& message) : MCEASException("Communication error: " + message) {}
};

// Исключение для ошибок безопасности
class SecurityException : public MCEASException {
public:
    explicit SecurityException(const std::string& message) : MCEASException("Security error: " + message) {}
};

// Исключение для системных ошибок
class SystemException : public MCEASException {
public:
    explicit SystemException(const std::string& message) : MCEASException("System error: " + message) {}
};

} // namespace mceas

#endif // MCEAS_ERRORS_H