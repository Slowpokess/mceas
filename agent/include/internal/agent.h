#ifndef MCEAS_AGENT_H
#define MCEAS_AGENT_H

#include "internal/common.h"
#include "internal/types.h"
#include "internal/errors.h"
#include "internal/security/anti_analysis.h"
#include "internal/security/self_protection.h"
#include <string>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

namespace mceas {

// Предварительные объявления
class ModuleManager;
class Config;
class Updater;

namespace comm {
    class Client;
}

// Режимы работы агента
enum class AgentMode {
    DEBUG,     // Режим отладки: подробное логирование, без скрытности
    STEALTH,   // Скрытный режим: маскировка, защита, минимум логов
    TEST,      // Тестовый режим: без отправки данных на сервер
    HYBRID     // Гибридный режим: начинает в режиме отладки, переключается в скрытный
};

class Agent {
public:
    // Конструктор, деструктор
    Agent();
    ~Agent();

    // Инициализация агента
    StatusCode initialize(const std::string& config_path = "");

    // Запуск агента
    StatusCode start();

    // Остановка агента
    StatusCode stop();

    // Выполнение команды
    CommandResult executeCommand(const Command& command);

    // Получение статуса агента
    bool isRunning() const;

    // Получение ID агента
    std::string getAgentId() const;

    // Получение версии агента
    std::string getVersion() const;
    
    // Установка режима работы
    void setMode(AgentMode mode);
    
    // Получение текущего режима работы
    AgentMode getMode() const;
    
    // Обработчик результатов проверок безопасности
    void handleSecurityDetection(const security::DetectionResult& result);
    
    // Обработчик событий защиты
    void handleProtectionEvent(security::ProtectionType type, bool enabled);

private:
    // Основной цикл работы агента
    void mainLoop();

    // Обработка результатов модулей
    void processModuleResults();

    // Отправка данных на сервер
    void sendDataToServer();
    
    // Проверка безопасности среды
    bool checkEnvironment();
    
    // Настройка режима работы
    void configureMode();
    
    // Функция для обнаружения отладчика
    bool isDebuggerPresent();
    
    // Функция для проверки виртуальной машины
    bool isVirtualMachine();
    
    // Добавление критических данных в результаты отправки
    void addCriticalData();

    // Уникальный идентификатор агента
    std::string agent_id_;

    // Флаг работы агента
    std::atomic<bool> running_;

    // Компоненты агента
    std::unique_ptr<Config> config_;
    std::unique_ptr<ModuleManager> module_manager_;
    std::unique_ptr<Updater> updater_;
    std::unique_ptr<comm::Client> comm_client_;
    std::unique_ptr<security::AntiAnalysis> anti_analysis_;
    std::unique_ptr<security::SelfProtection> self_protection_;

    // Поток для основного цикла
    std::unique_ptr<std::thread> main_thread_;
    
    // Результаты модулей
    std::vector<ModuleResult> module_results_;
    
    // Текущий режим работы
    AgentMode mode_;
    
    // Флаг возможной угрозы
    std::atomic<bool> threat_detected_;

    // Мьютекс для синхронизации доступа
    std::mutex mutex_;
};

} // namespace mceas

#endif // MCEAS_AGENT_H