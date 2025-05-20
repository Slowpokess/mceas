#ifndef MCEAS_COMM_HTTP_PROTOCOL_H
#define MCEAS_COMM_HTTP_PROTOCOL_H

#include "internal/comm/protocols/protocol_base.h"
#include "internal/comm/connection.h"
#include <map>
#include <string>
#include <queue>
#include <chrono>
#include <mutex>

namespace mceas {
namespace comm {

class HttpProtocol : public IProtocol {
public:
    HttpProtocol();
    ~HttpProtocol() noexcept override;
    
    // Реализация методов интерфейса IProtocol
    bool connect(const ProtocolConfig& config) override;
    bool disconnect() override;
    ConnectionState getConnectionState() const override;
    bool send(const Message& message) override;
    std::optional<Message> receive() override;
    std::optional<Message> sendAndWaitForResponse(const Message& message, int timeout_ms) override;
    
private:
    // Внутренние методы
    
    // Отправка HTTP запроса с ретраями при ошибках
    bool sendHttpRequestWithRetry(const std::string& method, const std::string& url, 
                                 const std::map<std::string, std::string>& headers,
                                 const std::vector<uint8_t>& body,
                                 std::string& response_body,
                                 std::map<std::string, std::string>& response_headers,
                                 int& status_code);
    
    // Базовая отправка HTTP запроса
    bool sendHttpRequest(const std::string& method, const std::string& url, 
                        const std::map<std::string, std::string>& headers,
                        const std::vector<uint8_t>& body,
                        std::string& response_body,
                        std::map<std::string, std::string>& response_headers,
                        int& status_code);
    
    // Отправка фрагментированного сообщения
    bool sendChunkedMessage(const Message& message, size_t chunk_size);
    
    // Получение URL для сообщения
    std::string getUrlForMessage(const Message& message);
    
    // Получение заголовков для сообщения
    std::map<std::string, std::string> getHeadersForMessage(const Message& message);
    
    // Объект управления соединением
    Connection connection_;
    
    // Конфигурация
    ProtocolConfig config_;
    
    // Базовый URL для запросов
    std::string base_url_;
    
    // Очередь полученных сообщений
    std::queue<Message> received_messages_;
    
    // Информация о последнем запросе
    struct LastRequestInfo {
        std::string method;
        std::string url;
        int status_code;
        std::string error;
        std::chrono::system_clock::time_point timestamp;
    };
    
    LastRequestInfo last_request_;
    
    // Состояние соединения
    ConnectionState state_;
    
    // Мьютекс для потокобезопасности
    mutable std::mutex mutex_;
};

} // namespace comm
} // namespace mceas

#endif // MCEAS_COMM_HTTP_PROTOCOL_H