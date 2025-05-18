#include "internal/crypto/solana.h"
#include "internal/logging.h"
#include <filesystem>
#include <fstream>
#include "external/json/json.hpp"

namespace mceas {

PhantomWalletAnalyzer::PhantomWalletAnalyzer(BrowserType browser_type)
    : browser_type_(browser_type), is_installed_(false) {
    
    try {
        const char* home_dir = std::getenv("HOME");
        if (!home_dir) {
            LogError("Failed to get HOME environment variable");
            return;
        }
        
        // Определяем путь к расширению в зависимости от браузера
        if (browser_type == BrowserType::CHROME) {
            std::string chrome_extensions_dir = std::string(home_dir) + 
                "/Library/Application Support/Google/Chrome/Default/Extensions/";
            extension_path_ = chrome_extensions_dir + extension_id_;
            storage_path_ = std::string(home_dir) + 
                "/Library/Application Support/Google/Chrome/Default/Local Storage/leveldb/";
        }
        else if (browser_type == BrowserType::FIREFOX) {
            // Firefox хранит расширения иначе
            LogInfo("Firefox path for Phantom is not fully implemented yet");
            extension_path_ = "";
            storage_path_ = "";
        }
        else if (browser_type == BrowserType::EDGE) {
            std::string edge_extensions_dir = std::string(home_dir) + 
                "/Library/Application Support/Microsoft Edge/Default/Extensions/";
            extension_path_ = edge_extensions_dir + extension_id_;
            storage_path_ = std::string(home_dir) + 
                "/Library/Application Support/Microsoft Edge/Default/Local Storage/leveldb/";
        }
        
        // Проверяем, установлено ли расширение
        is_installed_ = std::filesystem::exists(extension_path_);
        
        if (is_installed_) {
            // Получаем версию из манифеста
            std::string manifest_path = extension_path_;
            std::string latest_version = "";
            
            if (std::filesystem::exists(extension_path_) && std::filesystem::is_directory(extension_path_)) {
                for (const auto& entry : std::filesystem::directory_iterator(extension_path_)) {
                    if (entry.is_directory()) {
                        latest_version = entry.path().filename().string();
                    }
                }
            }
            
            if (!latest_version.empty()) {
                manifest_path = extension_path_ + "/" + latest_version + "/manifest.json";
                
                if (std::filesystem::exists(manifest_path)) {
                    std::ifstream file(manifest_path);
                    if (file.is_open()) {
                        nlohmann::json manifest;
                        file >> manifest;
                        file.close();
                        
                        if (manifest.contains("version")) {
                            version_ = manifest["version"].get<std::string>();
                        }
                    }
                }
            }
        }
    }
    catch (const std::exception& e) {
        LogError("Error initializing Phantom analyzer: " + std::string(e.what()));
    }
}

bool PhantomWalletAnalyzer::isInstalled() {
    return is_installed_;
}

WalletType PhantomWalletAnalyzer::getType() {
    return WalletType::SOLANA;
}

std::string PhantomWalletAnalyzer::getName() {
    return "Phantom";
}

std::string PhantomWalletAnalyzer::getVersion() {
    return version_;
}

std::string PhantomWalletAnalyzer::getWalletPath() {
    return extension_path_;
}

std::map<std::string, std::string> PhantomWalletAnalyzer::collectData() {
    std::map<std::string, std::string> data;
    
    if (!isInstalled()) {
        LogWarning("Phantom is not installed, cannot collect data");
        return data;
    }
    
    try {
        // Добавляем базовую информацию
        data["version"] = version_;
        data["extension_path"] = extension_path_;
        data["storage_path"] = storage_path_;
        
        // Добавляем информацию о браузере
        switch (browser_type_) {
            case BrowserType::CHROME:
                data["browser"] = "Chrome";
                break;
            case BrowserType::FIREFOX:
                data["browser"] = "Firefox";
                break;
            case BrowserType::EDGE:
                data["browser"] = "Edge";
                break;
            default:
                data["browser"] = "Unknown";
                break;
        }
        
        // Получаем адреса
        auto addresses = getAddresses();
        data["addresses_count"] = std::to_string(addresses.size());
        
        // Получаем транзакции
        auto transactions = getTransactions(10);
        data["transactions_count"] = std::to_string(transactions.size());
        
        // Получаем токены
        auto tokens = getTokens();
        data["tokens_count"] = std::to_string(tokens.size());
        
        // Создаем JSON с информацией об адресах
        if (!addresses.empty()) {
            nlohmann::json addresses_json = nlohmann::json::array();
            
            for (const auto& addr : addresses) {
                nlohmann::json addr_json;
                addr_json["currency"] = addr.currency;
                addr_json["address"] = addr.address;
                addr_json["balance"] = addr.balance;
                addr_json["label"] = addr.label;
                
                addresses_json.push_back(addr_json);
            }
            
            data["addresses_json"] = addresses_json.dump();
        }
    }
    catch (const std::exception& e) {
        LogError("Error collecting Phantom data: " + std::string(e.what()));
    }
    
    return data;
}

std::vector<CryptoAddress> PhantomWalletAnalyzer::getAddresses() {
    std::vector<CryptoAddress> addresses;
    
    try {
        // Заглушка: здесь должен быть код для извлечения адресов из Phantom
        LogInfo("Phantom address extraction is not fully implemented yet");
        
        // Для тестирования добавляем примерный адрес Solana
        CryptoAddress sample_address;
        sample_address.currency = "SOL";
        sample_address.address = "9..."; // Заглушка для Solana адреса
        sample_address.balance = 0.0;
        sample_address.label = "Main Wallet";
        
        addresses.push_back(sample_address);
    }
    catch (const std::exception& e) {
        LogError("Error extracting Phantom addresses: " + std::string(e.what()));
    }
    
    return addresses;
}

std::vector<Transaction> PhantomWalletAnalyzer::getTransactions(int limit) {
    std::vector<Transaction> transactions;
    
    try {
        // Заглушка: здесь должен быть код для извлечения транзакций
        LogInfo("Phantom transaction extraction is not fully implemented yet");
    }
    catch (const std::exception& e) {
        LogError("Error extracting Phantom transactions: " + std::string(e.what()));
    }
    
    return transactions;
}

std::map<std::string, std::string> PhantomWalletAnalyzer::getTokens() {
    std::map<std::string, std::string> tokens;
    
    try {
        // Заглушка: здесь должен быть код для извлечения информации о токенах
        LogInfo("Phantom token extraction is not fully implemented yet");
    }
    catch (const std::exception& e) {
        LogError("Error extracting Phantom tokens: " + std::string(e.what()));
    }
    
    return tokens;
}

} // namespace mceas