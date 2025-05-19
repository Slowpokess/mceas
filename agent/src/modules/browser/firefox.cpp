#include "browser/firefox.h"
#include "internal/logging.h"
#include "browser/common.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include "external/json/json.hpp" // Используем nlohmann/json

namespace mceas {

FirefoxAnalyzer::FirefoxAnalyzer() {
    // Инициализация
    is_installed_ = false;
    
    try {
        // Проверяем, установлен ли Firefox
        is_installed_ = browser::isBrowserInstalled("Firefox");
        
        if (is_installed_) {
            // Получаем путь к исполняемому файлу
            executable_path_ = browser::getExecutablePath("Firefox");
            
            // Получаем версию
            version_ = browser::getBrowserVersion("Firefox", "CFBundleShortVersionString");
            
            // Получаем путь к профилю
            // Firefox использует случайно сгенерированное имя для директории профиля
            // Здесь мы используем упрощение, предполагая, что есть только одна директория профиля
            std::string mozilla_dir = browser::getProfilePath("Library/Application Support/Mozilla");
            if (!mozilla_dir.empty()) {
                std::string firefox_dir = mozilla_dir + "/Firefox/Profiles";
                
                // Ищем первую директорию, оканчивающуюся на ".default"
                if (std::filesystem::exists(firefox_dir) && std::filesystem::is_directory(firefox_dir)) {
                    for (const auto& entry : std::filesystem::directory_iterator(firefox_dir)) {
                        if (entry.is_directory() && entry.path().string().find(".default") != std::string::npos) {
                            profile_path_ = entry.path().string();
                            break;
                        }
                    }
                }
            }
        }
    }
    catch (const std::exception& e) {
        LogError("Error initializing Firefox analyzer: " + std::string(e.what()));
    }
}

bool FirefoxAnalyzer::isInstalled() {
    return is_installed_;
}

BrowserType FirefoxAnalyzer::getType() {
    return BrowserType::FIREFOX;
}

std::string FirefoxAnalyzer::getName() {
    return "Firefox";
}

std::string FirefoxAnalyzer::getVersion() {
    return version_;
}

std::string FirefoxAnalyzer::getExecutablePath() {
    return executable_path_;
}

std::string FirefoxAnalyzer::getProfilePath() {
    return profile_path_;
}

std::map<std::string, std::string> FirefoxAnalyzer::collectData() {
    std::map<std::string, std::string> data;
    
    if (!isInstalled()) {
        LogWarning("Firefox is not installed, cannot collect data");
        return data;
    }
    
    try {
        // Добавляем базовую информацию
        data["version"] = version_;
        data["executable_path"] = executable_path_;
        data["profile_path"] = profile_path_;
        
        // Получаем закладки
        auto bookmarks = getBookmarks();
        data["bookmarks_count"] = std::to_string(bookmarks.size());
        
        // Получаем историю
        auto history = getHistory(100); // Ограничиваем 100 записями
        data["history_entries_count"] = std::to_string(history.size());
        
        // Получаем расширения
        auto extensions = getExtensions();
        data["extensions_count"] = std::to_string(extensions.size());
        
        // Создаем JSON с информацией о расширениях
        if (!extensions.empty()) {
            nlohmann::json extensions_json = nlohmann::json::array();
            
            for (const auto& ext : extensions) {
                nlohmann::json ext_json;
                ext_json["id"] = ext.id;
                ext_json["name"] = ext.name;
                ext_json["version"] = ext.version;
                ext_json["enabled"] = ext.enabled;
                
                extensions_json.push_back(ext_json);
            }
            
            data["extensions_json"] = extensions_json.dump();
        }
    }
    catch (const std::exception& e) {
        LogError("Error collecting Firefox data: " + std::string(e.what()));
    }
    
    return data;
}

std::vector<Bookmark> FirefoxAnalyzer::getBookmarks() {
    std::vector<Bookmark> bookmarks;
    
    try {
        // Firefox хранит закладки в places.sqlite
        // Здесь должен быть код для доступа к SQLite базе данных
        
        // Заглушка: здесь должен быть код для чтения закладок Firefox из SQLite
        // В реальном приложении здесь будет подключение к базе SQLite и выполнение запроса
        
        LogInfo("Firefox bookmarks analysis is not fully implemented yet");
    }
    catch (const std::exception& e) {
        LogError("Error parsing Firefox bookmarks: " + std::string(e.what()));
    }
    
    return bookmarks;
}

std::vector<HistoryEntry> FirefoxAnalyzer::getHistory(int limit) {
    std::vector<HistoryEntry> history;
    
    try {
        // Firefox хранит историю в places.sqlite
        // Здесь должен быть код для доступа к SQLite базе данных
        
        // Заглушка: здесь должен быть код для чтения истории Firefox из SQLite
        // В реальном приложении здесь будет подключение к базе SQLite и выполнение запроса
        
        LogInfo("Firefox history analysis is not fully implemented yet");
    }
    catch (const std::exception& e) {
        LogError("Error parsing Firefox history: " + std::string(e.what()));
    }
    
    return history;
}

std::vector<Extension> FirefoxAnalyzer::getExtensions() {
    std::vector<Extension> extensions;
    
    try {
        // Firefox хранит расширения в extensions.json
        std::string extensions_file = profile_path_ + "/extensions.json";
        
        if (!std::filesystem::exists(extensions_file)) {
            LogWarning("Firefox extensions file not found: " + extensions_file);
            return extensions;
        }
        
        // Заглушка: здесь должен быть код для чтения расширений Firefox из JSON
        // В реальном приложении здесь будет парсинг extensions.json
        
        LogInfo("Firefox extensions analysis is not fully implemented yet");
    }
    catch (const std::exception& e) {
        LogError("Error parsing Firefox extensions: " + std::string(e.what()));
    }
    
    return extensions;
}

} // namespace mceas