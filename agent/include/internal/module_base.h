#ifndef MCEAS_MODULE_BASE_H
#define MCEAS_MODULE_BASE_H

#include "common.h"
#include "types.h"
#include "internal/module_manager.h"
#include "logging.h"
#include <string>
#include <map>
#include <mutex>
#include <atomic>


namespace mceas {

// Абстрактный базовый класс для всех модулей
class ModuleBase : public Module {
public:
    ModuleBase(const std::string& name, ModuleType type);
    virtual ~ModuleBase() = default;

    // Реализация методов из интерфейса Module
    virtual bool initialize() override;
    virtual bool start() override;
    virtual bool stop() override;
    virtual ModuleResult execute() override;
    virtual bool handleCommand(const Command& command) override;
    
    // Геттеры
    virtual std::string getName() const override;
    virtual ModuleType getType() const override;

protected:
    // Методы для переопределения в конкретных модулях
    virtual bool onInitialize() = 0;
    virtual bool onStart() = 0;
    virtual bool onStop() = 0;
    virtual ModuleResult onExecute() = 0;
    virtual bool onHandleCommand(const Command& command) = 0;

    // Общие методы для всех модулей
    void setLastError(const std::string& error);
    std::string getLastError() const;
    
    // Добавление данных в результат
    void addResultData(const std::string& key, const std::string& value);
    
    // Очистка данных результата
    void clearResultData();

    // Имя и тип модуля
    std::string name_;
    ModuleType type_;
    
    // Флаг состояния модуля
    std::atomic<bool> running_;
    
    // Последняя ошибка
    std::string last_error_;
    
    // Собранные данные результата
    std::map<std::string, std::string> result_data_;
    
    // Мьютекс для синхронизации доступа
    mutable std::mutex mutex_;
};

} // namespace mceas

#endif // MCEAS_MODULE_BASE_H