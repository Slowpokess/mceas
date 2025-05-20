#ifndef MCEAS_SECURITY_SELF_PROTECTION_H
#define MCEAS_SECURITY_SELF_PROTECTION_H

#include <string>
#include <vector>
#include <functional>

namespace mceas {
namespace security {

// Типы механизмов защиты
enum class ProtectionType {
    WATCHDOG,
    PERSISTENCE,
    ANTI_KILL,
    SELF_REPAIR
};

// Настройки защиты
struct ProtectionConfig {
    bool enable_watchdog;
    bool enable_persistence;
    bool enable_anti_kill;
    bool enable_self_repair;
    std::string watchdog_path;
    std::string install_path;
    int heartbeat_interval_ms;
    
    // Конструктор с настройками по умолчанию
    ProtectionConfig()
        : enable_watchdog(true), enable_persistence(true),
          enable_anti_kill(true), enable_self_repair(true),
          watchdog_path(""), install_path(""),
          heartbeat_interval_ms(5000) {}
};

// Класс для самозащиты агента
class SelfProtection {
public:
    // Конструктор
    SelfProtection(const ProtectionConfig& config = ProtectionConfig());
    
    // Деструктор
    ~SelfProtection();
    
    // Инициализация механизмов защиты
    bool initialize();
    
    // Запуск защиты
    bool start();
    
    // Остановка защиты
    bool stop();
    
    // Установка агента для постоянного запуска
    bool installPersistence();
    
    // Удаление агента из автозапуска
    bool removePersistence();
    
    // Запуск сторожевого процесса
    bool startWatchdog();
    
    // Остановка сторожевого процесса
    bool stopWatchdog();
    
    // Проверка целостности
    bool checkIntegrity();
    
    // Самовосстановление
    bool selfRepair();
    
    // Установка обработчика для событий защиты
    void setProtectionHandler(std::function<void(ProtectionType, bool)> handler);
    
    // Проверка, включена ли защита
    bool isProtectionEnabled(ProtectionType type) const;
    
    // Обновление настроек защиты
    void updateConfig(const ProtectionConfig& config);
    
    // Получение настроек защиты
    ProtectionConfig getConfig() const;
    
private:
    // Механизмы реализации защиты зависят от платформы
    // Методы для Windows
#if defined(_WIN32)
    bool installPersistenceWindows();
    bool removePersistenceWindows();
    bool startWatchdogWindows();
    bool stopWatchdogWindows();
#endif

    // Методы для macOS
#if defined(__APPLE__)
    bool installPersistenceMacOS();
    bool removePersistenceMacOS();
    bool startWatchdogMacOS();
    bool stopWatchdogMacOS();
#endif

    // Методы для Linux
#if defined(__linux__)
    bool installPersistenceLinux();
    bool removePersistenceLinux();
    bool startWatchdogLinux();
    bool stopWatchdogLinux();
#endif

    // Общие методы
    bool createWatchdogProcess();
    bool obfuscateProcessName();
    bool setupSignalHandlers();
    void heartbeatLoop();
    
    // Конфигурация
    ProtectionConfig config_;
    
    // Флаг работы
    bool running_;
    
    // Идентификатор собственного процесса
    int process_id_;
    
    // Идентификатор сторожевого процесса
    int watchdog_process_id_;
    
    // Путь к собственному исполняемому файлу
    std::string executable_path_;
    
    // Обработчик событий защиты
    std::function<void(ProtectionType, bool)> protection_handler_;
};

} // namespace security
} // namespace mceas

#endif // MCEAS_SECURITY_SELF_PROTECTION_H