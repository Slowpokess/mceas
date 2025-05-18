#ifndef MCEAS_AGENT_H
#define MCEAS_AGENT_H

#include "internal/common.h"
#include "internal/types.h"
#include "internal/errors.h"
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

private:
    // Основной цикл работы агента
    void mainLoop();

    // Обработка результатов модулей
    void processModuleResults();

    // Отправка данных на сервер
    void sendDataToServer();

    // Уникальный идентификатор агента
    std::string agent_id_;

    // Флаг работы агента
    std::atomic<bool> running_;

    // Компоненты агента
    std::unique_ptr<Config> config_;
    std::unique_ptr<ModuleManager> module_manager_;
    std::unique_ptr<Updater> updater_;

    // Поток для основного цикла
    std::unique_ptr<std::thread> main_thread_;

    // Мьютекс для синхронизации доступа
    std::mutex mutex_;
};

} // namespace mceas

#endif // MCEAS_AGENT_H