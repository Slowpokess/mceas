#ifndef MCEAS_BEHAVIOR_MODULE_H
#define MCEAS_BEHAVIOR_MODULE_H

#include "internal/common.h"
#include "internal/types.h"
#include "internal/module_base.h"
#include <vector>
#include <memory>
#include <string>
#include <map>
#include <mutex>
#include <chrono>
#include <atomic>

namespace mceas {

// Структура для информации о сессии пользователя
struct SessionInfo {
    std::string user_id;
    std::string user_name;
    std::string terminal;
    std::string login_source;
    std::chrono::system_clock::time_point login_time;
    std::chrono::system_clock::time_point last_activity;
    bool is_remote;
    bool is_active;
    std::string remote_host;
    std::map<std::string, std::string> env_variables;
};

// Структура для информации о процессе
struct ProcessInfo {
    int pid;
    int ppid;
    std::string name;
    std::string path;
    std::string user;
    std::chrono::system_clock::time_point start_time;
    double cpu_usage;
    uint64_t memory_usage;
    std::string command_line;
    std::map<std::string, std::string> environment;
    int tty;
    bool is_background;
};

// Структура для информации о сетевом соединении
struct NetworkConnectionInfo {
    int pid;
    std::string process_name;
    std::string protocol;
    std::string local_address;
    int local_port;
    std::string remote_address;
    int remote_port;
    std::string state;
    std::chrono::system_clock::time_point established_time;
    uint64_t sent_bytes;
    uint64_t received_bytes;
};

// Интерфейс для анализатора сессий
class SessionAnalyzer {
public:
    virtual ~SessionAnalyzer() = default;
    
    // Получение списка активных сессий
    virtual std::vector<SessionInfo> getActiveSessions() = 0;
    
    // Получение истории сессий за указанный период
    virtual std::vector<SessionInfo> getSessionHistory(
        std::chrono::system_clock::time_point from,
        std::chrono::system_clock::time_point to) = 0;
    
    // Получение информации о конкретной сессии
    virtual SessionInfo getSessionInfo(const std::string& session_id) = 0;
    
    // Проверка подозрительной активности в сессиях
    virtual std::vector<std::string> checkForSuspiciousActivity() = 0;
};

// Интерфейс для мониторинга процессов
class ProcessMonitor {
public:
    virtual ~ProcessMonitor() = default;
    
    // Получение списка активных процессов
    virtual std::vector<ProcessInfo> getRunningProcesses() = 0;
    
    // Получение информации о конкретном процессе
    virtual ProcessInfo getProcessInfo(int pid) = 0;
    
    // Получение списка дочерних процессов
    virtual std::vector<ProcessInfo> getChildProcesses(int pid) = 0;
    
    // Получение истории запуска процессов
    virtual std::vector<ProcessInfo> getProcessHistory(
        std::chrono::system_clock::time_point from,
        std::chrono::system_clock::time_point to) = 0;
    
    // Проверка наличия подозрительных процессов
    virtual std::vector<std::string> checkForSuspiciousProcesses() = 0;
};

// Интерфейс для мониторинга сетевой активности
class NetworkMonitor {
public:
    virtual ~NetworkMonitor() = default;
    
    // Получение списка активных сетевых соединений
    virtual std::vector<NetworkConnectionInfo> getActiveConnections() = 0;
    
    // Получение соединений для конкретного процесса
    virtual std::vector<NetworkConnectionInfo> getProcessConnections(int pid) = 0;
    
    // Получение истории сетевых соединений
    virtual std::vector<NetworkConnectionInfo> getConnectionHistory(
        std::chrono::system_clock::time_point from,
        std::chrono::system_clock::time_point to) = 0;
    
    // Проверка наличия подозрительных сетевых соединений
    virtual std::vector<std::string> checkForSuspiciousConnections() = 0;
};

// Основной модуль поведенческого аудита
class BehaviorModule : public ModuleBase {
public:
    BehaviorModule();
    ~BehaviorModule() override;

protected:
    // Реализация методов базового класса
    bool onInitialize() override;
    bool onStart() override;
    bool onStop() override;
    ModuleResult onExecute() override;
    bool onHandleCommand(const Command& command) override;

private:
    // Инициализация компонентов
    void initComponents();
    
    // Запуск периодического мониторинга
    void startMonitoring();
    
    // Остановка периодического мониторинга
    void stopMonitoring();
    
    // Функция периодического мониторинга
    void monitoringThread();
    
    // Анализ поведенческих шаблонов
    void analyzeBehaviorPatterns();
    
    // Сохранение истории для последующего анализа
    void saveHistory();
    
    // Компоненты модуля
    std::unique_ptr<SessionAnalyzer> session_analyzer_;
    std::unique_ptr<ProcessMonitor> process_monitor_;
    std::unique_ptr<NetworkMonitor> network_monitor_;
    
    // Данные мониторинга
    std::vector<SessionInfo> active_sessions_;
    std::vector<ProcessInfo> running_processes_;
    std::vector<NetworkConnectionInfo> active_connections_;
    
    // Журнал событий
    std::vector<std::string> event_log_;
    
    // Флаг работы мониторинга
    std::atomic<bool> monitoring_active_;
    
    // Период сбора данных (в секундах)
    int monitoring_interval_;
    
    // Поток для мониторинга
    std::unique_ptr<std::thread> monitoring_thread_;
    
    // Время последнего обновления данных
    std::chrono::system_clock::time_point last_update_time_;
    
    // Мьютекс для потокобезопасности
    std::mutex data_mutex_;
};

} // namespace mceas

#endif // MCEAS_BEHAVIOR_MODULE_H