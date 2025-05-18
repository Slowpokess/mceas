#ifndef MCEAS_MODULE_MANAGER_H
#define MCEAS_MODULE_MANAGER_H

#include "internal/common.h"
#include "internal/types.h"
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <mutex>


namespace mceas {

// Интерфейс модуля
class Module {
public:
    virtual ~Module() = default;
    virtual bool initialize() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual ModuleResult execute() = 0;
    virtual bool handleCommand(const Command& command) = 0;
    virtual std::string getName() const = 0;
    virtual ModuleType getType() const = 0;
};

class ModuleManager {
public:
    // Конструктор
    explicit ModuleManager(const AgentConfig& config);
    
    // Деструктор
    ~ModuleManager();
    
    // Инициализация модулей
    bool initializeModules();
    
    // Запуск модулей
    bool startModules();
    
    // Остановка модулей
    bool stopModules();
    
    // Сбор результатов от всех модулей
    std::vector<ModuleResult> collectResults();
    
    // Получение модуля по имени
    std::shared_ptr<Module> getModule(const std::string& name);
    
    // Получение модуля по типу
    std::shared_ptr<Module> getModule(ModuleType type);
    
    // Выполнение команды для конкретного модуля
    bool executeModuleCommand(const std::string& module_name, const Command& command);
    
private:
    // Загрузка модуля заданного типа
    std::shared_ptr<Module> loadModule(ModuleType type);
    
    // Конфигурация агента
    AgentConfig config_;
    
    // Хранилище модулей (имя -> модуль)
    std::map<std::string, std::shared_ptr<Module>> modules_by_name_;
    
    // Хранилище модулей (тип -> модуль)
    std::map<ModuleType, std::shared_ptr<Module>> modules_by_type_;
    
    // Мьютекс для потокобезопасности
    std::mutex mutex_;
};

} // namespace mceas

#endif // MCEAS_MODULE_MANAGER_H