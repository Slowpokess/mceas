#ifndef MCEAS_UPDATER_H
#define MCEAS_UPDATER_H

#include "internal/common.h"
#include "internal/types.h"
#include <string>
#include <mutex>
#include <chrono>

namespace mceas {

// Предварительное объявление
class Config;

class Updater {
public:
    // Конструктор
    Updater(const AgentConfig& config, Config& config_manager);
    
    // Деструктор
    ~Updater();
    
    // Проверка наличия обновлений
    bool checkForUpdates(bool force = false);
    
    // Выполнение обновления
    bool performUpdate();
    
    // Получение версии доступного обновления
    std::string getAvailableVersion() const;
    
    // Получение времени последней проверки
    std::chrono::system_clock::time_point getLastCheckTime() const;
    
private:
    // Загрузка файла обновления
    bool downloadUpdate(const std::string& url, const std::string& save_path);
    
    // Проверка целостности файла обновления
    bool verifyUpdate(const std::string& file_path, const std::string& checksum);
    
    // Установка обновления
    bool installUpdate(const std::string& file_path);
    
    // Конфигурация агента
    AgentConfig config_;
    
    // Ссылка на менеджер конфигурации
    Config& config_manager_;
    
    // Версия доступного обновления
    std::string available_version_;
    
    // URL для загрузки обновления
    std::string update_url_;
    
    // Время последней проверки обновлений
    std::chrono::system_clock::time_point last_check_time_;
    
    // Мьютекс для потокобезопасности
    mutable std::mutex mutex_;
};

} // namespace mceas

#endif // MCEAS_UPDATER_H