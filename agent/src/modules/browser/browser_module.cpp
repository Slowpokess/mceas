#include "browser/browser_module.h"
#include "internal/logging.h"
#include "internal/browser/chrome.h"
#include "internal/browser/firefox.h"
#include "internal/browser/edge.h"

namespace mceas {

BrowserModule::BrowserModule() 
    : ModuleBase("BrowserModule", ModuleType::BROWSER) {
}

BrowserModule::~BrowserModule() {
    if (running_) {
        stop();
    }
}

bool BrowserModule::onInitialize() {
    LogInfo("Initializing Browser Module");
    
    // Инициализируем анализаторы браузеров
    initAnalyzers();
    
    // Обнаруживаем установленные браузеры
    detectBrowsers();
    
    return true;
}

bool BrowserModule::onStart() {
    LogInfo("Starting Browser Module");
    return true;
}

bool BrowserModule::onStop() {
    LogInfo("Stopping Browser Module");
    return true;
}

ModuleResult BrowserModule::onExecute() {
    LogInfo("Executing Browser Module");
    
    ModuleResult result;
    result.module_name = getName();
    result.type = getType();
    result.success = true;
    
    try {
        // Собираем данные обо всех обнаруженных браузерах
        for (auto browser : detected_browsers_) {
            if (browser) {
                std::string browser_name = browser->getName();
                LogInfo("Collecting data from browser: " + browser_name);
                
                // Получаем данные от анализатора
                auto browser_data = browser->collectData();
                
                // Добавляем данные в результат
                for (const auto& data : browser_data) {
                    // Используем префикс с именем браузера, чтобы избежать коллизий
                    std::string key = browser_name + "." + data.first;
                    addResultData(key, data.second);
                }
            }
        }
        
        // Добавляем информацию о количестве обнаруженных браузеров
        addResultData("browsers.count", std::to_string(detected_browsers_.size()));
        
        return result;
    }
    catch (const std::exception& e) {
        LogError("Error executing browser module: " + std::string(e.what()));
        result.success = false;
        result.error_message = "Error executing browser module: " + std::string(e.what());
        return result;
    }
}

bool BrowserModule::onHandleCommand(const Command& command) {
    LogInfo("Handling command in Browser Module: " + command.action);
    
    // Обрабатываем команды
    if (command.action == "scan_browsers") {
        // Повторно сканируем браузеры
        detectBrowsers();
        return true;
    }
    else if (command.action == "get_browser_details") {
        // Получаем детали о конкретном браузере
        auto it = command.parameters.find("browser");
        if (it != command.parameters.end()) {
            std::string requested_browser = it->second;
            
            for (auto browser : detected_browsers_) {
                if (browser && browser->getName() == requested_browser) {
                    // Здесь можно выполнить дополнительные действия
                    LogInfo("Requested details for browser: " + requested_browser);
                    return true;
                }
            }
            
            LogWarning("Browser not found: " + requested_browser);
            return false;
        }
        
        LogWarning("Missing browser parameter in command");
        return false;
    }
    
    LogWarning("Unknown command action: " + command.action);
    return false;
}

void BrowserModule::initAnalyzers() {
    // Создаем анализаторы для всех поддерживаемых браузеров
    try {
        analyzers_.push_back(std::make_unique<ChromeAnalyzer>());
        LogInfo("Chrome analyzer initialized");
    }
    catch (const std::exception& e) {
        LogError("Failed to initialize Chrome analyzer: " + std::string(e.what()));
    }
    
    try {
        analyzers_.push_back(std::make_unique<FirefoxAnalyzer>());
        LogInfo("Firefox analyzer initialized");
    }
    catch (const std::exception& e) {
        LogError("Failed to initialize Firefox analyzer: " + std::string(e.what()));
    }
    
    try {
        analyzers_.push_back(std::make_unique<EdgeAnalyzer>());
        LogInfo("Edge analyzer initialized");
    }
    catch (const std::exception& e) {
        LogError("Failed to initialize Edge analyzer: " + std::string(e.what()));
    }
}

void BrowserModule::detectBrowsers() {
    LogInfo("Detecting installed browsers");
    
    // Очищаем список обнаруженных браузеров
    detected_browsers_.clear();
    
    // Проверяем установленные браузеры
    for (const auto& analyzer : analyzers_) {
        if (analyzer->isInstalled()) {
            LogInfo("Detected browser: " + analyzer->getName() + 
                    ", version: " + analyzer->getVersion());
            detected_browsers_.push_back(analyzer.get());
        }
    }
    
    LogInfo("Total browsers detected: " + std::to_string(detected_browsers_.size()));
}

} // namespace mceas