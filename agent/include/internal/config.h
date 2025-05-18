#ifndef MCEAS_CONFIG_H
#define MCEAS_CONFIG_H

#include "internal/common.h"
#include "internal/types.h"
#include <string>
#include <map>
#include <mutex>

namespace mceas {

class Config {
public:
    // Конструктор
    explicit Config(const std::string& config_path = "");
    
    // Деструктор
    ~Config();
    
    // Загрузка конфигурации из файла
    bool loadFromFile(const std::string& path);
    
    // Сохранение конфигурации в файл
    bool saveToFile(const std::string& path = "");
    
    // Получение значения параметра по ключу
    std::string getValue(const std::string& key, const std::string& default_value = "") const;
    
    // Установка значения параметра
    void setValue(const std::string& key, const std::string& value);
    
    // Получение конфигурации агента
    AgentConfig getAgentConfig() const;
    
    // Обновление конфигурации агента
    void updateAgentConfig(const AgentConfig& config);
    
    // Проверка наличия ключа в конфигурации
    bool hasKey(const std::string& key) const;
    
private:
    // Загрузка конфигурации по умолчанию
    void loadDefaultConfig();
    
    // Парсинг JSON-конфигурации
    bool parseJson(const std::string& json_str);
    
    // Сериализация в JSON
    std::string serializeToJson() const;
    
    // Хранилище параметров (ключ-значение)
    std::map<std::string, std::string> config_values_;
    
    // Путь к файлу конфигурации
    std::string config_path_;
    
    // Мьютекс для потокобезопасности
    mutable std::mutex mutex_;
};

} // namespace mceas

#endif // MCEAS_CONFIG_H