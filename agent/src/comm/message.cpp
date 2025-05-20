#include "internal/comm/message.h"
#include "internal/logging.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <random>
#include "external/json/json.hpp" // Используем nlohmann/json

namespace mceas {
namespace comm {

// Генерация уникального ID сообщения
static std::string generateMessageId() {
    // Генерируем случайную строку из 16 символов
    const char charset[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<int> distribution(0, sizeof(charset) - 2);
    
    std::string result;
    result.reserve(16);
    for (int i = 0; i < 16; ++i) {
        result += charset[distribution(generator)];
    }
    return result;
}

// Получение текущей метки времени в формате ISO 8601
static std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%dT%H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count() << "Z";
    return ss.str();
}

std::vector<uint8_t> Message::serialize() const {
    try {
        // Создаем JSON-объект с метаданными сообщения
        nlohmann::json j;
        
        // Добавляем основные поля
        j["type"] = static_cast<int>(type);
        j["agent_id"] = agent_id;
        j["message_id"] = message_id;
        j["timestamp"] = timestamp;
        
        // Добавляем заголовки
        j["headers"] = headers;
        
        // Сериализуем JSON в строку
        std::string json_str = j.dump();
        
        // Создаем результирующий буфер:
        // [4 байта длины JSON][JSON данные][Payload данные]
        std::vector<uint8_t> result;
        
        // Добавляем длину JSON в байтах (little-endian)
        uint32_t json_length = static_cast<uint32_t>(json_str.size());
        result.push_back(static_cast<uint8_t>(json_length & 0xFF));
        result.push_back(static_cast<uint8_t>((json_length >> 8) & 0xFF));
        result.push_back(static_cast<uint8_t>((json_length >> 16) & 0xFF));
        result.push_back(static_cast<uint8_t>((json_length >> 24) & 0xFF));
        
        // Добавляем JSON данные
        result.insert(result.end(), json_str.begin(), json_str.end());
        
        // Добавляем payload данные
        result.insert(result.end(), payload.begin(), payload.end());
        
        return result;
    }
    catch (const std::exception& e) {
        LogError("Error serializing message: " + std::string(e.what()));
        return std::vector<uint8_t>();
    }
}

Message Message::deserialize(const std::vector<uint8_t>& data) {
    Message message;
    
    try {
        if (data.size() < 4) {
            throw std::runtime_error("Data too short for message header");
        }
        
        // Извлекаем длину JSON
        uint32_t json_length = static_cast<uint32_t>(data[0]) |
                             (static_cast<uint32_t>(data[1]) << 8) |
                             (static_cast<uint32_t>(data[2]) << 16) |
                             (static_cast<uint32_t>(data[3]) << 24);
        
        if (data.size() < 4 + json_length) {
            throw std::runtime_error("Data too short for JSON content");
        }
        
        // Извлекаем JSON строку
        std::string json_str(data.begin() + 4, data.begin() + 4 + json_length);
        
        // Парсим JSON
        nlohmann::json j = nlohmann::json::parse(json_str);
        
        // Заполняем поля сообщения
        message.type = static_cast<MessageType>(j["type"].get<int>());
        message.agent_id = j["agent_id"].get<std::string>();
        message.message_id = j["message_id"].get<std::string>();
        message.timestamp = j["timestamp"].get<std::string>();
        
        // Заполняем заголовки
        message.headers = j["headers"].get<std::map<std::string, std::string>>();
        
        // Заполняем payload (оставшиеся данные после JSON)
        if (data.size() > 4 + json_length) {
            message.payload.assign(data.begin() + 4 + json_length, data.end());
        }
        
        return message;
    }
    catch (const std::exception& e) {
        LogError("Error deserializing message: " + std::string(e.what()));
        return message;
    }
}

Message Message::createDataMessage(const std::string& agent_id, const std::vector<uint8_t>& data) {
    Message message;
    message.type = MessageType::DATA;
    message.agent_id = agent_id;
    message.message_id = generateMessageId();
    message.timestamp = getCurrentTimestamp();
    message.payload = data;
    return message;
}

Message Message::createCommandMessage(const std::string& agent_id, const std::string& command_id, 
                                    const std::string& action, 
                                    const std::map<std::string, std::string>& params) {
    Message message;
    message.type = MessageType::COMMAND;
    message.agent_id = agent_id;
    message.message_id = generateMessageId();
    message.timestamp = getCurrentTimestamp();
    
    // Добавляем информацию о команде в заголовки
    message.headers["command_id"] = command_id;
    message.headers["action"] = action;
    
    // Сериализуем параметры команды в JSON и помещаем в payload
    nlohmann::json j = params;
    std::string params_json = j.dump();
    message.payload.assign(params_json.begin(), params_json.end());
    
    return message;
}

Message Message::createCommandResultMessage(const std::string& agent_id, const std::string& command_id, 
                                         bool success, const std::string& result) {
    Message message;
    message.type = MessageType::COMMAND_RESULT;
    message.agent_id = agent_id;
    message.message_id = generateMessageId();
    message.timestamp = getCurrentTimestamp();
    
    // Добавляем информацию о результате в заголовки
    message.headers["command_id"] = command_id;
    message.headers["success"] = success ? "true" : "false";
    
    // Помещаем результат в payload
    message.payload.assign(result.begin(), result.end());
    
    return message;
}

} // namespace comm
} // namespace mceas