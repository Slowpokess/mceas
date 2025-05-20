#ifndef MCEAS_COMM_CONNECTION_H
#define MCEAS_COMM_CONNECTION_H

#include <string>
#include <chrono>
#include <functional>

namespace mceas {
namespace comm {

// Состояние соединения (расширенная версия из protocol_base.h)
enum class ConnectionState {
    DISCONNECTED,  // Не подключено
    CONNECTING,    // В процессе подключения
    CONNECTED,     // Подключено
    RECONNECTING,  // В процессе переподключения
    FAILED,        // Соединение не удалось установить после всех попыток
    ERROR          // Ошибка соединения
};

// Статистика соединения
struct ConnectionStats {
    int sent_messages;          // Количество отправленных сообщений
    int received_messages;      // Количество полученных сообщений
    int failed_sends;           // Количество неудачных отправок
    int reconnect_attempts;     // Количество попыток переподключения
    std::chrono::system_clock::time_point last_activity; // Время последней активности
    
    // Конструктор
    ConnectionStats() 
        : sent_messages(0), received_messages(0), 
          failed_sends(0), reconnect_attempts(0),
          last_activity(std::chrono::system_clock::now()) {}
};

// Параметры соединения
struct ConnectionParams {
    std::string endpoint;            // Адрес для подключения
    int port;                        // Порт
    int connect_timeout_ms;          // Таймаут подключения
    int operation_timeout_ms;        // Таймаут операций
    int reconnect_attempts;          // Количество попыток переподключения
    int reconnect_interval_ms;       // Интервал между попытками
    bool auto_reconnect;             // Автоматическое переподключение
    
    // Конструктор с значениями по умолчанию
    ConnectionParams()
        : port(0), connect_timeout_ms(5000), operation_timeout_ms(10000),
          reconnect_attempts(3), reconnect_interval_ms(1000),
          auto_reconnect(true) {}
};

// Класс для управления соединением
class Connection {
public:
    // Конструктор
    explicit Connection(const ConnectionParams& params);
    
    // Деструктор
    ~Connection();
    
    // Установка соединения
    bool connect();
    
    // Разрыв соединения
    bool disconnect();
    
    // Проверка состояния
    ConnectionState getState() const;
    
    // Получение статистики
    ConnectionStats getStats() const;
    
    // Обновление времени последней активности
    void updateLastActivity();
    
    // Установка обработчика изменения состояния
    void setStateChangeHandler(std::function<void(ConnectionState)> handler);
    
    // Установка параметров
    void setParams(const ConnectionParams& params);
    
    // Получение параметров
    ConnectionParams getParams() const;
    
    // Сброс статистики
    void resetStats();
    
    // Проверка, нужно ли переподключение
    bool needsReconnect() const;
    
    // Переподключение
    bool reconnect();

private:
    // Текущее состояние
    ConnectionState state_;
    
    // Параметры соединения
    ConnectionParams params_;
    
    // Статистика
    ConnectionStats stats_;
    
    // Обработчик изменения состояния
    std::function<void(ConnectionState)> state_handler_;
};

} // namespace comm
} // namespace mceas

#endif // MCEAS_COMM_CONNECTION_H