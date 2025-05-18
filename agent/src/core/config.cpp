#include "internal/config.h"
#include "internal/logging.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <json.hpp> // Используем nlohmann/json

namespace mceas {

Config::Config(const std::string& config_path) : config_path_(config_path) {
    // Если указан путь, загружаем из файла
    if (!config_path.empty()) {
        if (!loadFromFile(config_path)) {
            LogWarning("Failed to load config from: " + config_path + ", using defaults");
            loadDefaultConfig();
        }
    }
    else {
        // Иначе загружаем настройки по умолчанию
        loadDefaultConfig();
    }
}

Config::~Config() {
    // Если путь к конфигурации задан, сохраняем при уничтожении объекта
    if (!config_path_.empty()) {
        saveToFile(config_path_);
    }
}

bool Config::loadFromFile(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            LogError("Unable to open config file: " + path);
            return false;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();
        
        return parseJson(buffer.str());
    }
    catch (const std::exception& e) {
        LogError("Error loading config: " + std::string(e.what()));
        return false;
    }
}

bool Config::saveToFile(const std::string& path) {
    std::string file_path = path.empty() ? config_path_ : path;
    
    if (file_path.empty()) {
        LogError("Config file path is not specified");
        return false;
    }
    
    try {
        std::ofstream file(file_path);
        if (!file.is_open()) {
            LogError("Unable to open config file for writing: " + file_path);
            return false;
        }
        
        file << serializeToJson();
        file.close();
        
        return true;
    }
    catch (const std::exception& e) {
        LogError("Error saving config: " + std::string(e.what()));
        return false;
    }
}

std::string Config::getValue(const std::string& key, const std::string& default_value) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = config_values_.find(key);
    if (it != config_values_.end()) {
        return it->second;
    }
    
    return default_value;
}

void Config::setValue(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_values_[key] = value;
}

AgentConfig Config::getAgentConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    AgentConfig config;
    
    // Заполняем структуру конфигурации значениями из хранилища
    config.agent_id = getValue("agent_id", "");
    config.server_url = getValue("server_url", "https://mceas-server.example.com");
    config.encryption_enabled = getValue("encryption_enabled", "true") == "true";
    config.update_interval = std::stoi(getValue("update_interval", "3600"));
    
    // Настраиваем включенные модули
    config.enabled_modules[ModuleType::BROWSER] = getValue("module_browser_enabled", "true") == "true";
    config.enabled_modules[ModuleType::CRYPTO] = getValue("module_crypto_enabled", "true") == "true";
    config.enabled_modules[ModuleType::FILE] = getValue("module_file_enabled", "true") == "true";
    config.enabled_modules[ModuleType::BEHAVIOR] = getValue("module_behavior_enabled", "true") == "true";
    
    return config;
}

void Config::updateAgentConfig(const AgentConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Обновляем хранилище значениями из структуры конфигурации
    setValue("agent_id", config.agent_id);
    setValue("server_url", config.server_url);
    setValue("encryption_enabled", config.encryption_enabled ? "true" : "false");
    setValue("update_interval", std::to_string(config.update_interval));
    
    // Обновляем состояние модулей
    for (const auto& module_pair : config.enabled_modules) {
        switch (module_pair.first) {
            case ModuleType::BROWSER:
                setValue("module_browser_enabled", module_pair.second ? "true" : "false");
                break;
            case ModuleType::CRYPTO:
                setValue("module_crypto_enabled", module_pair.second ? "true" : "false");
                break;
            case ModuleType::FILE:
                setValue("module_file_enabled", module_pair.second ? "true" : "false");
                break;
            case ModuleType::BEHAVIOR:
                setValue("module_behavior_enabled", module_pair.second ? "true" : "false");
                break;
        }
    }
}

bool Config::hasKey(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_values_.find(key) != config_values_.end();
}

void Config::loadDefaultConfig() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Загружаем конфигурацию по умолчанию
    config_values_["agent_id"] = "";  // Пустое значение, будет генерироваться динамически
    config_values_["server_url"] = "https://mceas-server.example.com";
    config_values_["encryption_enabled"] = "true";
    config_values_["update_interval"] = "3600";  // 1 час в секундах
    
    // Настройки модулей по умолчанию
    config_values_["module_browser_enabled"] = "true";
    config_values_["module_crypto_enabled"] = "true";
    config_values_["module_file_enabled"] = "true";
    config_values_["module_behavior_enabled"] = "true";
}

bool Config::parseJson(const std::string& json_str) {
    try {
        nlohmann::json j = nlohmann::json::parse(json_str);
        
        std::lock_guard<std::mutex> lock(mutex_);
        config_values_.clear();
        
        // Разбираем JSON в плоскую структуру ключ-значение
        for (auto it = j.begin(); it != j.end(); ++it) {
            if (it.value().is_string()) {
                config_values_[it.key()] = it.value().get<std::string>();
            }
            else if (it.value().is_boolean()) {
                config_values_[it.key()] = it.value().get<bool>() ? "true" : "false";
            }
            else if (it.value().is_number()) {
                config_values_[it.key()] = std::to_string(it.value().get<int>());
            }
            // Пропускаем сложные типы данных
        }
        
        return true;
    }
    catch (const std::exception& e) {
        LogError("Error parsing JSON config: " + std::string(e.what()));
        return false;
    }
}

std::string Config::serializeToJson() const {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        
        nlohmann::json j;
        
        // Преобразуем плоскую структуру ключ-значение в JSON
        for (const auto& pair : config_values_) {
            const std::string& key = pair.first;
            const std::string& value = pair.second;
            
            // Пытаемся угадать тип на основе значения
            if (value == "true" || value == "false") {
                j[key] = (value == "true");
            }
            else {
                // Пытаемся преобразовать в число
                try {
                    size_t pos;
                    int num = std::stoi(value, &pos);
                    if (pos == value.length()) {
                        j[key] = num;
                    }
                    else {
                        j[key] = value;
                    }
                }
                catch (...) {
                    j[key] = value;
                }
            }
        }
        
        return j.dump(4);  // Отформатированный JSON с отступами
    }
    catch (const std::exception& e) {
        LogError("Error serializing config to JSON: " + std::string(e.what()));
        return "{}";
    }
}

} // namespace mceas