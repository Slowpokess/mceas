#ifndef MCEAS_COMM_CLIENT_H
#define MCEAS_COMM_CLIENT_H

#include "internal/comm/protocols/protocol_base.h"
#include "internal/comm/crypto/crypto_base.h"
#include "internal/types.h"
#include <string>
#include <memory>
#include <queue>
#include <mutex>
#include <vector>
#include <atomic>
#include <thread>

namespace mceas {
namespace comm {

// Конфигурация клиента
struct ClientConfig {
    std::string server_url;         // URL сервера
    std::string protocol_name;      // Имя используемого протокола
    ProtocolConfig protocol_config; // Конфигурация протокола
    crypto::CryptoConfig crypto_config; // Конфигурация шифрования
    bool use_encryption;            // Использовать ли шифрование
    bool compress_data;             // Сжимать ли данные
    int retry_count;                // Количество попыток отправки
    int heartbeat_interval_ms;      // Интервал между проверками связи
    
    // Конструктор с настройками по умолчанию
    ClientConfig() : protocol_name("https"), use_encryption(true), 
                   compress_data(true), retry_count(3), 
                   heartbeat_interval_ms(60000) {}
};

// Клиент для связи с сервером
class Client {
public:
    // Конструктор
    Client();
    
    // Деструктор
    ~Client();
    
    // Инициализация клиента
    bool initialize(const ClientConfig& config);
    
    // Подключение к серверу
    bool connect();
    
    // Отключение от сервера
    bool disconnect();
    
    // Проверка состояния соединения
    ConnectionState getConnectionState() const;
    
    // Отправка данных телеметрии
    bool sendData(const std::string& agent_id, const std::map<std::string, std::string>& data);
    
    // Отправка результата команды
    bool sendCommandResult(const std::string& agent_id, const CommandResult& result);
    
    // Получение доступных команд от сервера
    std::vector<Command> receiveCommands(const std::string& agent_id);
    
    // Добавление сообщения в очередь на отправку (для офлайн-режима)
    void queueMessage(const Message& message);
    
    // Установка обработчика полученных команд
    void setCommandHandler(std::function<void(const Command&)> handler);
    
    // Установка ID агента
    void setAgentId(const std::string& agent_id);
    
    // Получение информации о клиенте
    std::map<std::string, std::string> getClientInfo() const;

private:
    // Метод для работы в фоновом потоке
    void backgroundWorker();
    
    // Обработка очереди сообщений
    void processMessageQueue();

    // Метод для обработки полученных сообщений
    void handleReceivedMessage(const Message& message);
    
    // Отправка проверки связи
    bool sendHeartbeat();
    
    // Общий метод отправки сообщения
    bool sendMessage(const Message& message);
    
    // Аутентификация на сервере
    bool authenticate();
    
    // Протокол для связи
    std::unique_ptr<IProtocol> protocol_;
    
    // Шифрование
    std::unique_ptr<crypto::ICrypto> crypto_;
    
    // Конфигурация клиента
    ClientConfig config_;
    
    // ID агента
    std::string agent_id_;
    
    // Очередь сообщений для отправки
    std::queue<Message> message_queue_;
    
    // Флаг работы фонового потока
    std::atomic<bool> running_;
    
    // Поток для фоновых операций
    std::unique_ptr<std::thread> background_thread_;
    
    // Обработчик команд
    std::function<void(const Command&)> command_handler_;
    
    // Мьютекс для потокобезопасности
    mutable std::mutex mutex_;
};

} // namespace comm
} // namespace mceas

#endif // MCEAS_COMM_CLIENT_H