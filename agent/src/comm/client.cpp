#include "internal/comm/client.h"
#include "internal/logging.h"
#include <functional>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <string>
#include <json.hpp> 


namespace mceas {
namespace comm {

Client::Client() : running_(false) {
}

Client::~Client() {
    if (running_) {
        disconnect();
    }
}

bool Client::initialize(const ClientConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    config_ = config;
    
    try {
        // Создаем основной протокол через фабрику
        NetworkConstraints network_constraints;
        // Здесь в реальном приложении можно определить ограничения сети
        
        protocol_ = ProtocolFactory::createProtocol(
            config.protocol_name, network_constraints);
        
        if (!protocol_) {
            LogError("Failed to create protocol: " + config.protocol_name);
            
            // Пытаемся создать резервный протокол
            protocol_ = ProtocolFactory::createFallbackProtocol(config.protocol_name);
            
            if (!protocol_) {
                return false;
            }
            
            LogInfo("Using fallback protocol");
        }
        
        // Если нужно шифрование, создаем объект шифрования
        if (config.use_encryption) {
            crypto_ = crypto::CryptoFactory::createCrypto(config.crypto_config.algorithm);
            if (!crypto_) {
                LogError("Failed to create crypto with algorithm: " + config.crypto_config.algorithm);
                return false;
            }
            
            // Инициализируем шифрование
            if (!crypto_->initialize(config.crypto_config)) {
                LogError("Failed to initialize crypto");
                return false;
            }
        }
        
        LogInfo("Client initialized with protocol: " + config.protocol_name);
        return true;
    }
    catch (const std::exception& e) {
        LogError("Error initializing client: " + std::string(e.what()));
        return false;
    }
}

bool Client::connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!protocol_) {
        LogError("Protocol not initialized");
        return false;
    }
    
    try {
        // Подключаем протокол
        if (!protocol_->connect(config_.protocol_config)) {
            LogError("Failed to connect using protocol");
            return false;
        }
        
        // Если нужна аутентификация, проводим ее
        if (!authenticate()) {
            LogError("Authentication failed");
            protocol_->disconnect();
            return false;
        }
        
        // Запускаем фоновый поток обработки
        running_ = true;
        background_thread_ = std::make_unique<std::thread>(&Client::backgroundWorker, this);
        
        LogInfo("Client connected to server: " + config_.server_url);
        return true;
    }
    catch (const std::exception& e) {
        LogError("Error connecting client: " + std::string(e.what()));
        return false;
    }
}

bool Client::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!protocol_) {
        return false;
    }
    
    try {
        // Останавливаем фоновый поток
        running_ = false;
        
        if (background_thread_ && background_thread_->joinable()) {
            background_thread_->join();
        }
        
        // Отключаем протокол
        bool result = protocol_->disconnect();
        
        LogInfo("Client disconnected from server");
        return result;
    }
    catch (const std::exception& e) {
        LogError("Error disconnecting client: " + std::string(e.what()));
        return false;
    }
}

ConnectionState Client::getConnectionState() const {
    if (!protocol_) {
        return ConnectionState::DISCONNECTED;
    }
    
    return protocol_->getConnectionState();
}

bool Client::sendData(const std::string& agent_id, const std::map<std::string, std::string>& data) {
    try {
        // Проверка ID агента
        std::string id = agent_id.empty() ? agent_id_ : agent_id;
        if (id.empty()) {
            LogError("Agent ID not set for sending data");
            return false;
        }
        
        // Преобразуем данные в JSON
        nlohmann::json j = data;
        std::string json_str = j.dump();
        
        // Создаем сообщение с данными
        std::vector<uint8_t> payload(json_str.begin(), json_str.end());
        Message message = Message::createDataMessage(id, payload);
        
        // Отправляем сообщение
        return sendMessage(message);
    }
    catch (const std::exception& e) {
        LogError("Error sending data: " + std::string(e.what()));
        return false;
    }
}

bool Client::sendCommandResult(const std::string& agent_id, const CommandResult& result) {
    try {
        // Проверка ID агента
        std::string id = agent_id.empty() ? agent_id_ : agent_id;
        if (id.empty()) {
            LogError("Agent ID not set for sending command result");
            return false;
        }
        
        // Создаем сообщение с результатом
        Message message = Message::createCommandResultMessage(
            id, result.command_id, result.success, result.result);
        
        // Если есть сообщение об ошибке, добавляем его в заголовки
        if (!result.error_message.empty()) {
            message.headers["error_message"] = result.error_message;
        }
        
        // Отправляем сообщение
        return sendMessage(message);
    }
    catch (const std::exception& e) {
        LogError("Error sending command result: " + std::string(e.what()));
        return false;
    }
}

std::vector<Command> Client::receiveCommands(const std::string& agent_id) {
    std::vector<Command> commands;
    
    if (!protocol_) {
        LogError("Protocol not initialized");
        return commands;
    }
    
    // Проверка ID агента
    std::string id = agent_id.empty() ? agent_id_ : agent_id;
    if (id.empty()) {
        LogError("Agent ID not set for receiving commands");
        return commands;
    }
    
    try {
        // Создаем сообщение запроса команд
        Message request;
        request.type = MessageType::COMMAND;
        request.agent_id = id;
        request.message_id = "request_commands_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        request.timestamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        request.headers["request_type"] = "get_commands";
        
        // Отправляем запрос и ждем ответа
        auto response = protocol_->sendAndWaitForResponse(request, config_.protocol_config.timeout_ms);
        
        if (!response) {
            LogWarning("No response received for commands request");
            return commands;
        }
        
        // Обрабатываем ответ
        if (response->type == MessageType::COMMAND) {
            // Парсим JSON из payload
            std::string json_str(response->payload.begin(), response->payload.end());
            nlohmann::json j = nlohmann::json::parse(json_str);
            
            // Обрабатываем команды
            if (j.is_array()) {
                for (const auto& cmd_json : j) {
                    Command cmd;
                    cmd.command_id = cmd_json["command_id"].get<std::string>();
                    cmd.action = cmd_json["action"].get<std::string>();
                    
                    // Преобразуем параметры
                    if (cmd_json.contains("parameters") && cmd_json["parameters"].is_object()) {
                        for (auto it = cmd_json["parameters"].begin(); it != cmd_json["parameters"].end(); ++it) {
                            cmd.parameters[it.key()] = it.value().get<std::string>();
                        }
                    }
                    
                    commands.push_back(cmd);
                }
            }
        }
        
        LogInfo("Received " + std::to_string(commands.size()) + " commands from server");
    }
    catch (const std::exception& e) {
        LogError("Error receiving commands: " + std::string(e.what()));
    }
    
    return commands;
}

void Client::queueMessage(const Message& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    message_queue_.push(message);
    LogDebug("Message queued for sending, queue size: " + std::to_string(message_queue_.size()));
}

void Client::setCommandHandler(std::function<void(const Command&)> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    command_handler_ = handler;
}

void Client::setAgentId(const std::string& agent_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    agent_id_ = agent_id;
}

std::map<std::string, std::string> Client::getClientInfo() const {
    std::map<std::string, std::string> info;
    
    info["protocol"] = config_.protocol_name;
    info["server_url"] = config_.server_url;
    info["encryption"] = config_.use_encryption ? "enabled" : "disabled";
    info["compression"] = config_.compress_data ? "enabled" : "disabled";
    
    // Добавляем информацию о состоянии соединения
    switch (getConnectionState()) {
        case ConnectionState::DISCONNECTED: info["state"] = "disconnected"; break;
        case ConnectionState::CONNECTING: info["state"] = "connecting"; break;
        case ConnectionState::CONNECTED: info["state"] = "connected"; break;
        case ConnectionState::ERROR: info["state"] = "error"; break;
        default: info["state"] = "unknown"; break;
    }
    
    return info;
}

void Client::backgroundWorker() {
    LogInfo("Background worker started");
    
    while (running_) {
        try {
            // Проверяем состояние соединения
            auto state = protocol_->getConnectionState();
            if (state != ConnectionState::CONNECTED) {
                // Если соединение не установлено, пытаемся его восстановить
                if (config_.retry_count > 0) {
                    LogWarning("Connection lost, attempting to reconnect...");
                    
                    // Пытаемся переподключиться
                    for (int attempt = 0; attempt < config_.retry_count; ++attempt) {
                        if (protocol_->connect(config_.protocol_config)) {
                            LogInfo("Reconnection successful");
                            
                            // После переподключения нужно аутентифицироваться заново
                            if (!authenticate()) {
                                LogError("Re-authentication failed");
                                protocol_->disconnect();
                                continue;
                            }
                            
                            break;
                        }
                        
                        LogWarning("Reconnection attempt " + std::to_string(attempt + 1) + 
                                  " of " + std::to_string(config_.retry_count) + " failed");
                                  
                        // Пауза перед следующей попыткой
                        std::this_thread::sleep_for(std::chrono::milliseconds(
                            config_.protocol_config.retry_interval_ms));
                    }
                }
            }
            
            // Обрабатываем очередь сообщений
            processMessageQueue();
            
            // Проверяем наличие новых сообщений от сервера
            auto message_opt = protocol_->receive();
            while (message_opt) {
                handleReceivedMessage(*message_opt);
                message_opt = protocol_->receive();
            }
            
            // Отправляем проверку связи если нужно
            if (config_.heartbeat_interval_ms > 0) {
                static auto last_heartbeat = std::chrono::steady_clock::now();
                auto now = std::chrono::steady_clock::now();
                
                if (std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - last_heartbeat).count() >= config_.heartbeat_interval_ms) {
                    
                    sendHeartbeat();
                    last_heartbeat = now;
                }
            }
            
            // Небольшая пауза для снижения нагрузки
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        catch (const std::exception& e) {
            LogError("Error in background worker: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    LogInfo("Background worker stopped");
}

void Client::processMessageQueue() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (message_queue_.empty()) {
        return;
    }
    
    // Проверяем состояние соединения
    if (protocol_->getConnectionState() != ConnectionState::CONNECTED) {
        return;
    }
    
    LogDebug("Processing message queue, size: " + std::to_string(message_queue_.size()));
    
    // Обрабатываем очередь сообщений
    while (!message_queue_.empty()) {
        Message message = message_queue_.front();
        
        // Пытаемся отправить сообщение
        if (protocol_->send(message)) {
            // Если успешно, удаляем из очереди
            message_queue_.pop();
            LogDebug("Queued message sent successfully");
        }
        else {
            // Если неудачно, прекращаем обработку
            LogWarning("Failed to send queued message, will retry later");
            break;
        }
    }
}

bool Client::sendHeartbeat() {
    if (!protocol_ || protocol_->getConnectionState() != ConnectionState::CONNECTED) {
        return false;
    }
    
    try {
        // Создаем сообщение с проверкой связи
        Message message;
        message.type = MessageType::HEARTBEAT;
        message.agent_id = agent_id_;
        message.message_id = "heartbeat_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        message.timestamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        
        // Отправляем сообщение
        if (!protocol_->send(message)) {
            LogWarning("Failed to send heartbeat");
            return false;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        LogError("Error sending heartbeat: " + std::string(e.what()));
        return false;
    }
}

bool Client::sendMessage(const Message& message) {
    if (!protocol_) {
        LogError("Protocol not initialized");
        return false;
    }
    
    try {
        Message send_message = message;
        
        // Если нужно, выполняем сжатие данных
        if (config_.compress_data && !send_message.payload.empty()) {
            // Здесь будет код сжатия данных
            // Для примера просто добавляем заголовок
            send_message.headers["compressed"] = "true";
        }
        
        // Если нужно, выполняем шифрование
        if (config_.use_encryption && crypto_ && !send_message.payload.empty()) {
            send_message.payload = crypto_->encrypt(send_message.payload);
            send_message.headers["encrypted"] = "true";
        }
        
        // Проверяем состояние соединения
        if (protocol_->getConnectionState() != ConnectionState::CONNECTED) {
            // Добавляем сообщение в очередь для отправки позже
            queueMessage(send_message);
            LogDebug("Connection not available, message queued");
            return true;
        }
        
        // Отправляем сообщение
        bool sent = false;
        int retry_count = 0;
        const int max_retries = 3;
        
        while (!sent && retry_count < max_retries) {
            sent = protocol_->send(send_message);
            
            if (!sent) {
                retry_count++;
                
                // Если все попытки неудачны, пробуем сменить протокол
                if (retry_count >= max_retries) {
                    LogWarning("Failed to send message after " + std::to_string(max_retries) + 
                              " attempts, trying to switch protocol");
                    
                    auto state = protocol_->getConnectionState();
                    if (state == ConnectionState::ERROR || state == ConnectionState::FAILED) {
                        // Получаем имя текущего протокола
                        std::string current_protocol = config_.protocol_name;
                        
                        // Создаем резервный протокол
                        auto fallback_protocol = ProtocolFactory::createFallbackProtocol(current_protocol);
                        
                        if (fallback_protocol) {
                            // Отключаем текущий протокол
                            protocol_->disconnect();
                            
                            // Меняем протокол
                            protocol_ = std::move(fallback_protocol);
                            
                            // Подключаем новый протокол
                            if (protocol_->connect(config_.protocol_config)) {
                                LogInfo("Switched to fallback protocol");
                                
                                // Пробуем отправить еще раз
                                sent = protocol_->send(send_message);
                            }
                        }
                    }
                }
                else {
                    // Ждем перед следующей попыткой
                    std::this_thread::sleep_for(std::chrono::milliseconds(
                        config_.protocol_config.retry_interval_ms));
                }
            }
        }
        
        if (!sent) {
            // Если отправка не удалась, добавляем в очередь
            queueMessage(send_message);
            LogWarning("Failed to send message, queued for later");
            return false;
        }
        
        LogDebug("Message sent successfully, type: " + std::to_string(static_cast<int>(send_message.type)));
        return true;
    }
    catch (const std::exception& e) {
        LogError("Error sending message: " + std::string(e.what()));
        return false;
    }
}

bool Client::authenticate() {
    if (!protocol_ || agent_id_.empty()) {
        LogError("Cannot authenticate: protocol not initialized or agent ID not set");
        return false;
    }
    
    try {
        // Создаем сообщение аутентификации
        Message auth_message;
        auth_message.type = MessageType::AUTH;
        auth_message.agent_id = agent_id_;
        auth_message.message_id = "auth_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        auth_message.timestamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        
        // Добавляем информацию для аутентификации
        nlohmann::json auth_data;
        auth_data["agent_id"] = agent_id_;
        auth_data["timestamp"] = auth_message.timestamp;
        
        // В реальной системе здесь должна быть более сложная логика аутентификации,
        // например, цифровая подпись или хеш с добавлением соли
        
        std::string auth_json = auth_data.dump();
        auth_message.payload.assign(auth_json.begin(), auth_json.end());
        
        // Отправляем сообщение и ждем ответа
        auto response_opt = protocol_->sendAndWaitForResponse(auth_message, config_.protocol_config.timeout_ms);
        
        if (!response_opt) {
            LogError("No response received for authentication request");
            return false;
        }
        
        // Проверяем ответ
        if (response_opt->type == MessageType::AUTH && 
            response_opt->headers.find("status") != response_opt->headers.end() &&
            response_opt->headers.at("status") == "success") {
            
            LogInfo("Authentication successful");
            return true;
        }
        
        // Если аутентификация не удалась, выводим ошибку
        std::string error = "Authentication failed";
        if (response_opt->headers.find("error") != response_opt->headers.end()) {
            error += ": " + response_opt->headers.at("error");
        }
        
        LogError(error);
        return false;
    }
    catch (const std::exception& e) {
        LogError("Error during authentication: " + std::string(e.what()));
        return false;
    }
}

void Client::handleReceivedMessage(const Message& message) {
    try {
        // Обрабатываем сообщение в зависимости от типа
        switch (message.type) {
            case MessageType::COMMAND: {
                // Разбираем команду
                if (command_handler_) {
                    // Парсим JSON из payload
                    std::string json_str(message.payload.begin(), message.payload.end());
                    nlohmann::json j = nlohmann::json::parse(json_str);
                    
                    // Создаем объект команды
                    Command cmd;
                    cmd.command_id = j["command_id"].get<std::string>();
                    cmd.action = j["action"].get<std::string>();
                    
                    // Преобразуем параметры
                    if (j.contains("parameters") && j["parameters"].is_object()) {
                        for (auto it = j["parameters"].begin(); it != j["parameters"].end(); ++it) {
                            cmd.parameters[it.key()] = it.value().get<std::string>();
                        }
                    }
                    
                    // Вызываем обработчик
                    command_handler_(cmd);
                }
                break;
            }
            case MessageType::HEARTBEAT: {
                // Обрабатываем проверку связи
                Message response;
                response.type = MessageType::HEARTBEAT;
                response.agent_id = agent_id_;
                response.message_id = "heartbeat_response_" + message.message_id;
                response.timestamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
                response.headers["response_to"] = message.message_id;
                
                // Отправляем ответ
                protocol_->send(response);
                break;
            }
            // Другие типы сообщений...
            default:
                LogWarning("Received message of unknown type: " + std::to_string(static_cast<int>(message.type)));
                break;
        }
    }
    catch (const std::exception& e) {
        LogError("Error handling received message: " + std::string(e.what()));
    }
}

} // namespace comm
} // namespace mceas