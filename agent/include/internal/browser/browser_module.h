#ifndef MCEAS_BROWSER_MODULE_H
#define MCEAS_BROWSER_MODULE_H

#include "module_base.h"
#include <vector>
#include <memory>
#include <map>
#include <string>

namespace mceas {

// Предварительные объявления конкретных анализаторов
class ChromeAnalyzer;
class FirefoxAnalyzer;
class EdgeAnalyzer;

// Перечисление поддерживаемых браузеров
enum class BrowserType {
    CHROME,
    FIREFOX,
    EDGE,
    UNKNOWN
};

// Интерфейс для анализаторов браузеров
class BrowserAnalyzer {
public:
    virtual ~BrowserAnalyzer() = default;
    
    // Проверяет, установлен ли браузер
    virtual bool isInstalled() = 0;
    
    // Возвращает тип браузера
    virtual BrowserType getType() = 0;
    
    // Возвращает имя браузера
    virtual std::string getName() = 0;
    
    // Возвращает версию браузера
    virtual std::string getVersion() = 0;
    
    // Возвращает путь к исполняемому файлу браузера
    virtual std::string getExecutablePath() = 0;
    
    // Возвращает путь к профилю браузера
    virtual std::string getProfilePath() = 0;
    
    // Собирает данные о браузере
    virtual std::map<std::string, std::string> collectData() = 0;
};

// Модуль анализа браузеров
class BrowserModule : public ModuleBase {
public:
    BrowserModule();
    ~BrowserModule() override;

protected:
    // Реализация методов базового класса
    bool onInitialize() override;
    bool onStart() override;
    bool onStop() override;
    ModuleResult onExecute() override;
    bool onHandleCommand(const Command& command) override;

private:
    // Инициализация анализаторов браузеров
    void initAnalyzers();
    
    // Обнаружение установленных браузеров
    void detectBrowsers();
    
    // Список анализаторов браузеров
    std::vector<std::unique_ptr<BrowserAnalyzer>> analyzers_;
    
    // Список обнаруженных браузеров
    std::vector<BrowserAnalyzer*> detected_browsers_;
};

} // namespace mceas

#endif // MCEAS_BROWSER_MODULE_H