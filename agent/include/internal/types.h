#ifndef MCEAS_TYPES_H
#define MCEAS_TYPES_H

#include <string>
#include <vector>
#include <map>
#include "common.h"

namespace mceas {

// Структура для конфигурации агента
struct AgentConfig {
    std::string agent_id;           // Уникальный идентификатор агента
    std::string server_url;         // URL сервера для коммуникации
    bool encryption_enabled;        // Включено ли шифрование
    std::map<ModuleType, bool> enabled_modules; // Включенные модули
    int update_interval;            // Интервал обновления (в секундах)
};

// Структура для результатов модуля
struct ModuleResult {
    ModuleType type;
    std::string module_name;
    std::map<std::string, std::string> data;
    bool success;
    std::string error_message;
};

// Структура для команды агенту
struct Command {
    std::string command_id;
    std::string action;
    std::map<std::string, std::string> parameters;
};

// Структура для результата выполнения команды
struct CommandResult {
    std::string command_id;
    bool success;
    std::string result;
    std::string error_message;
};

} // namespace mceas

#endif // MCEAS_TYPES_H