#include "internal/module_base.h"

namespace mceas {

ModuleBase::ModuleBase(const std::string& name, ModuleType type)
    : name_(name), type_(type), running_(false) {
}

bool ModuleBase::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        LogInfo("Initializing module: " + name_);
        return onInitialize();
    }
    catch (const std::exception& e) {
        setLastError("Initialization failed: " + std::string(e.what()));
        LogError("Module " + name_ + " initialization failed: " + e.what());
        return false;
    }
}

bool ModuleBase::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (running_) {
        LogWarning("Module " + name_ + " is already running");
        return true;
    }
    
    try {
        LogInfo("Starting module: " + name_);
        if (onStart()) {
            running_ = true;
            return true;
        }
        return false;
    }
    catch (const std::exception& e) {
        setLastError("Start failed: " + std::string(e.what()));
        LogError("Module " + name_ + " start failed: " + e.what());
        return false;
    }
}

bool ModuleBase::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!running_) {
        LogWarning("Module " + name_ + " is not running");
        return true;
    }
    
    try {
        LogInfo("Stopping module: " + name_);
        if (onStop()) {
            running_ = false;
            return true;
        }
        return false;
    }
    catch (const std::exception& e) {
        setLastError("Stop failed: " + std::string(e.what()));
        LogError("Module " + name_ + " stop failed: " + e.what());
        return false;
    }
}

ModuleResult ModuleBase::execute() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    ModuleResult result;
    result.module_name = name_;
    result.type = type_;
    
    if (!running_) {
        result.success = false;
        result.error_message = "Module is not running";
        return result;
    }
    
    try {
        // Очищаем данные результата перед выполнением
        clearResultData();
        
        // Выполняем основную логику модуля
        ModuleResult exec_result = onExecute();
        
        // Копируем данные из result_data_ в результат
        exec_result.data = result_data_;
        
        return exec_result;
    }
    catch (const std::exception& e) {
        setLastError("Execution failed: " + std::string(e.what()));
        LogError("Module " + name_ + " execution failed: " + e.what());
        
        result.success = false;
        result.error_message = last_error_;
        return result;
    }
}

bool ModuleBase::handleCommand(const Command& command) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        LogInfo("Handling command for module: " + name_ + ", command ID: " + command.command_id);
        return onHandleCommand(command);
    }
    catch (const std::exception& e) {
        setLastError("Command handling failed: " + std::string(e.what()));
        LogError("Module " + name_ + " command handling failed: " + e.what());
        return false;
    }
}

std::string ModuleBase::getName() const {
    return name_;
}

ModuleType ModuleBase::getType() const {
    return type_;
}

void ModuleBase::setLastError(const std::string& error) {
    last_error_ = error;
}

std::string ModuleBase::getLastError() const {
    return last_error_;
}

void ModuleBase::addResultData(const std::string& key, const std::string& value) {
    result_data_[key] = value;
}

void ModuleBase::clearResultData() {
    result_data_.clear();
}

} // namespace mceas