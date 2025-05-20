#ifndef MCEAS_COMM_MESSAGE_H
#define MCEAS_COMM_MESSAGE_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace mceas {
namespace comm {

// Типы сообщений
enum class MessageType {
    DATA,           // Данные телеметрии
    COMMAND,        // Команда от сервера
    COMMAND_RESULT, // Результат выполнения команды
    HEARTBEAT,      // Проверка связи
    AUTH,           // Аутентификация
    ERROR           // Сообщение об ошибке
};

// Структура сообщения
struct Message {
    MessageType type;                          // Тип сообщения
    std::string agent_id;                      // Идентификатор агента
    std::string message_id;                    // Уникальный ID сообщения
    std::string timestamp;                     // Временная метка
    std::vector<uint8_t> payload;              // Полезная нагрузка
    std::map<std::string, std::string> headers; // Дополнительные заголовки
    
    // Сериализация сообщения в бинарный формат
    std::vector<uint8_t> serialize() const;
    
    // Десериализация из бинарного формата
    static Message deserialize(const std::vector<uint8_t>& data);
    
    // Создание сообщения с данными
    static Message createDataMessage(const std::string& agent_id, const std::vector<uint8_t>& data);
    
    // Создание сообщения с командой
    static Message createCommandMessage(const std::string& agent_id, const std::string& command_id, 
                                      const std::string& action, 
                                      const std::map<std::string, std::string>& params);
    
    // Создание сообщения с результатом выполнения команды
    static Message createCommandResultMessage(const std::string& agent_id, const std::string& command_id, 
                                           bool success, const std::string& result);
};

} // namespace comm
} // namespace mceas

#endif // MCEAS_COMM_MESSAGE_H