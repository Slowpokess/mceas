#ifndef MCEAS_COMM_PROTOCOL_BASE_H
#define MCEAS_COMM_PROTOCOL_BASE_H

#include "internal/comm/message.h"
#include <string>
#include <optional>
#include <memory>
#include <vector>
#include <functional>
#include <map>

namespace mceas {
namespace comm {

// Состояние соединения
enum class ConnectionState {
    DISCONNECTED, // Не подключено
    CONNECTING,   // В процессе подключения
    CONNECTED,    // Подключено
    RECONNECTING, // В процессе переподключения
    FAILED,       // Соединение не удалось установить после всех попыток
    ERROR         // Ошибка соединения
};

// Ограничения сети
struct NetworkConstraints {
    bool has_internet;           // Есть ли доступ в интернет
    bool restricted_ports;       // Ограничены ли порты
    bool filtered_domains;       // Фильтруются ли домены
    bool deep_packet_inspection; // Есть ли глубокий анализ пакетов
    std::vector<int> allowed_ports; // Разрешенные порты
    
    // Конструктор с настройками по умолчанию
    NetworkConstraints() 
        : has_internet(true), restricted_ports(false), 
          filtered_domains(false), deep_packet_inspection(false) {
        // Стандартные веб-порты обычно доступны
        allowed_ports = {80, 443, 8080, 8443};
    }
};

// Конфигурация протокола
struct ProtocolConfig {
    std::string endpoint;      // Адрес сервера
    int port;                  // Порт
    bool use_encryption;       // Использовать ли шифрование
    int timeout_ms;            // Таймаут в миллисекундах
    int retry_count;           // Количество попыток переподключения
    int retry_interval_ms;     // Интервал между попытками в миллисекундах
    
    // Конструктор с настройками по умолчанию
    ProtocolConfig() 
        : port(0), use_encryption(true), timeout_ms(5000), 
          retry_count(3), retry_interval_ms(1000) {}
};

// Базовый интерфейс протокола
class IProtocol {
public:
    virtual ~IProtocol() noexcept = default;
    
    // Подключение к серверу
    virtual bool connect(const ProtocolConfig& config) = 0;
    
    // Отключение от сервера
    virtual bool disconnect() = 0;
    
    // Проверка состояния соединения
    virtual ConnectionState getConnectionState() const = 0;
    
    // Отправка сообщения
    virtual bool send(const Message& message) = 0;
    
    // Получение сообщения (если доступно)
    virtual std::optional<Message> receive() = 0;
    
    // Синхронное ожидание и получение ответа
    virtual std::optional<Message> sendAndWaitForResponse(const Message& message, int timeout_ms) = 0;
};

// Тип функции для создания протокола
using ProtocolCreator = std::function<std::unique_ptr<IProtocol>()>;

// Фабрика протоколов
class ProtocolFactory {
public:
    // Регистрация протокола
    static bool registerProtocol(const std::string& name, ProtocolCreator creator);
    
    // Создание протокола по имени
    static std::unique_ptr<IProtocol> createProtocol(const std::string& protocol_name);
    
    // Создание протокола с учетом ограничений сети
    static std::unique_ptr<IProtocol> createProtocol(
        const std::string& preferred_protocol,
        const NetworkConstraints& constraints);
    
    // Создание резервного протокола
    static std::unique_ptr<IProtocol> createFallbackProtocol(
        const std::string& failed_protocol);
    
    // Доступные протоколы
    static std::vector<std::string> getAvailableProtocols();
    
private:
    // Получение реестра протоколов
    static std::map<std::string, ProtocolCreator>& getProtocolRegistry();
    
    // Выбор оптимального протокола с учетом ограничений
    static std::string selectOptimalProtocol(
        const std::string& preferred_protocol,
        const NetworkConstraints& constraints);
    
    // Выбор резервного протокола
    static std::string selectFallbackProtocol(
        const std::string& failed_protocol);
};

} // namespace comm
} // namespace mceas

#endif // MCEAS_COMM_PROTOCOL_BASE_H