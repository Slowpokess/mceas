#include "browser/browser_module.h"
#include "internal/crypto_module.h"
#include "telemetry/file_telemetry_module.h"
#include "internal/behavior_module.h"
#include "internal/logging.h"
#include <algorithm>

namespace mceas {

ModuleManager::ModuleManager(const AgentConfig& config) : config_(config) {
    // Инициализация настроек модулей из конфигурации
}

ModuleManager::~ModuleManager() {
    // Останавливаем все модули при уничтожении менеджера
    stopModules();
}

bool ModuleManager::initializeModules() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    bool success = true;
    
    // Инициализируем модули в соответствии с конфигурацией
    for (const auto& module_pair : config_.enabled_modules) {
        ModuleType type = module_pair.first;
        bool enabled = module_pair.second;
        
        if (enabled) {
            try {
                // Загружаем модуль
                auto module = loadModule(type);
                
                if (module) {
                    // Инициализируем модуль
                    if (module->initialize()) {
                        // Добавляем в хранилища
                        modules_by_name_[module->getName()] = module;
                        modules_by_type_[module->getType()] = module;
                        
                        LogInfo("Module initialized: " + module->getName());
                    }
                    else {
                        LogError("Failed to initialize module: " + module->getName());
                        success = false;
                    }
                }
                else {
                    LogError("Failed to load module of type: " + std::to_string(static_cast<int>(type)));
                    success = false;
                }
            }
            catch (const std::exception& e) {
                LogError("Error initializing module: " + std::string(e.what()));
                success = false;
            }
        }
    }
    
    return success;
}

bool ModuleManager::startModules() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    bool success = true;
    
    // Запускаем все инициализированные модули
    for (auto& module_pair : modules_by_name_) {
        auto& module = module_pair.second;
        
        try {
            if (module->start()) {
                LogInfo("Module started: " + module->getName());
            }
            else {
                LogError("Failed to start module: " + module->getName());
                success = false;
            }
        }
        catch (const std::exception& e) {
            LogError("Error starting module " + module->getName() + ": " + std::string(e.what()));
            success = false;
        }
    }
    
    return success;
}

bool ModuleManager::stopModules() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    bool success = true;
    
    // Останавливаем все модули
    for (auto& module_pair : modules_by_name_) {
        auto& module = module_pair.second;
        
        try {
            if (module->stop()) {
                LogInfo("Module stopped: " + module->getName());
            }
            else {
                LogError("Failed to stop module: " + module->getName());
                success = false;
            }
        }
        catch (const std::exception& e) {
            LogError("Error stopping module " + module->getName() + ": " + std::string(e.what()));
            success = false;
        }
    }
    
    return success;
}

std::vector<ModuleResult> ModuleManager::collectResults() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<ModuleResult> results;
    
    // Собираем результаты от всех модулей
    for (auto& module_pair : modules_by_name_) {
        auto& module = module_pair.second;
        
        try {
            ModuleResult result = module->execute();
            results.push_back(result);
        }
        catch (const std::exception& e) {
            LogError("Error collecting results from module " + module->getName() + 
                     ": " + std::string(e.what()));
            
            // Добавляем результат с ошибкой
            ModuleResult error_result;
            error_result.module_name = module->getName();
            error_result.type = module->getType();
            error_result.success = false;
            error_result.error_message = std::string("Exception: ") + e.what();
            
            results.push_back(error_result);
        }
    }
    
    return results;
}

std::shared_ptr<Module> ModuleManager::getModule(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = modules_by_name_.find(name);
    if (it != modules_by_name_.end()) {
        return it->second;
    }
    
    return nullptr;
}

std::shared_ptr<Module> ModuleManager::getModule(ModuleType type) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = modules_by_type_.find(type);
    if (it != modules_by_type_.end()) {
        return it->second;
    }
    
    return nullptr;
}

bool ModuleManager::executeModuleCommand(const std::string& module_name, const Command& command) {
    auto module = getModule(module_name);
    if (!module) {
        LogError("Module not found: " + module_name);
        return false;
    }
    
    try {
        return module->handleCommand(command);
    }
    catch (const std::exception& e) {
        LogError("Error executing command on module " + module_name + ": " + std::string(e.what()));
        return false;
    }
}

// В функции loadModule добавляем поддержку модуля FILE_TELEMETRY
std::shared_ptr<Module> ModuleManager::loadModule(ModuleType type) {
    // Создаем экземпляр конкретного модуля в зависимости от типа
    switch (type) {
        case ModuleType::BROWSER: {
            auto module = std::make_shared<BrowserModule>();
            LogInfo("Created Browser Module");
            return module;
        }
        case ModuleType::CRYPTO: {
            auto module = std::make_shared<CryptoModule>();
            LogInfo("Created Crypto Module");
            return module;
        }
        case ModuleType::FILE: {
            auto module = std::make_shared<FileTelemetryModule>();
            LogInfo("Created File Telemetry Module");
            return module;
        }
        case ModuleType::BEHAVIOR: {
            auto module = std::make_shared<BehaviorModule>();
            LogInfo("Created Behaivor Module");
            return module;
        }
        default:
            LogWarning("Unknown module type: " + std::to_string(static_cast<int>(type)));
            return nullptr;
    }
}

} // namespace mceas