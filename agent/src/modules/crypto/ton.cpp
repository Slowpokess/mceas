#include "internal/crypto/ton.h"
#include "internal/logging.h"
#include <filesystem>
#include <fstream>
#include "external/json/json.hpp"

namespace mceas {

TONWalletAnalyzer::TONWalletAnalyzer() 
    : is_extension_installed_(false), is_app_installed_(false) {
    
    try {
        // Проверяем, установлено ли расширение TON Wallet
        is_extension_installed_ = isExtensionInstalled();
        
        // Проверяем, установлено ли приложение TON Wallet
        is_app_installed_ = isDesktopAppInstalled();
        
        if (is_extension_installed_) {
            const char* home_dir = std::getenv("HOME");
            if (home_dir) {
                // Путь к расширению Chrome
                std::string chrome_extensions_dir = std::string(home_dir) + 
                    "/Library/Application Support/Google/Chrome/Default/Extensions/";
                extension_path_ = chrome_extensions_dir + extension_id_;
                
                // Пытаемся получить версию из манифеста
                std::string manifest_path = extension_path_;
                
                // Ищем последнюю версию расширения
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
                            
                            // Получаем версию из манифеста
                            if (manifest.contains("version")) {
                                version_ = manifest["version"].get<std::string>();
                            }
                        }
                    }
                }
            }
        }
        else if (is_app_installed_) {
            // Получаем путь к приложению
            application_path_ = crypto::getAppPathMacOS("TON Wallet");
            
            // Получаем версию
            version_ = crypto::getAppVersionMacOS("TON Wallet", "CFBundleShortVersionString");
        }
    }
    catch (const std::exception& e) {
        LogError("Error initializing TON Wallet analyzer: " + std::string(e.what()));
    }
}

bool TONWalletAnalyzer::isInstalled() {
    return is_extension_installed_ || is_app_installed_;
}

WalletType TONWalletAnalyzer::getType() {
    return WalletType::TON;
}

std::string TONWalletAnalyzer::getName() {
    return "TON Wallet";
}

std::string TONWalletAnalyzer::getVersion() {
    return version_;
}

std::string TONWalletAnalyzer::getWalletPath() {
    if (is_extension_installed_) {
        return extension_path_;
    }
    else if (is_app_installed_) {
        return application_path_;
    }
    return "";
}

std::map<std::string, std::string> TONWalletAnalyzer::collectData() {
    std::map<std::string, std::string> data;
    
    if (!isInstalled()) {
        LogWarning("TON Wallet is not installed, cannot collect data");
        return data;
    }
    
    try {
        // Добавляем базовую информацию
        data["version"] = version_;
        data["is_extension_installed"] = is_extension_installed_ ? "true" : "false";
        data["is_app_installed"] = is_app_installed_ ? "true" : "false";
        
        if (is_extension_installed_) {
            data["extension_path"] = extension_path_;
        }
        
        if (is_app_installed_) {
            data["application_path"] = application_path_;
        }
        
        // Получаем адреса
        auto addresses = getAddresses();
        data["addresses_count"] = std::to_string(addresses.size());
        
        // Получаем транзакции
        auto transactions = getTransactions(10);
        data["transactions_count"] = std::to_string(transactions.size());
        
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
        LogError("Error collecting TON Wallet data: " + std::string(e.what()));
    }
    
    return data;
}

std::vector<CryptoAddress> TONWalletAnalyzer::getAddresses() {
    std::vector<CryptoAddress> addresses;
    
    try {
        // Заглушка: здесь должен быть код для извлечения адресов из TON Wallet
        LogInfo("TON Wallet address extraction is not fully implemented yet");
        
        // Для тестирования добавляем примерный адрес
        CryptoAddress sample_address;
        sample_address.currency = "TON";
        sample_address.address = "EQA..."; // Заглушка для TON адреса
        sample_address.balance = 0.0;
        sample_address.label = "Main Wallet";
        
        addresses.push_back(sample_address);
    }
    catch (const std::exception& e) {
        LogError("Error extracting TON Wallet addresses: " + std::string(e.what()));
    }
    
    return addresses;
}

std::vector<Transaction> TONWalletAnalyzer::getTransactions(int limit) {
    std::vector<Transaction> transactions;
    
    try {
        // Заглушка: здесь должен быть код для извлечения транзакций
        LogInfo("TON Wallet transaction extraction is not fully implemented yet");
    }
    catch (const std::exception& e) {
        LogError("Error extracting TON Wallet transactions: " + std::string(e.what()));
    }
    
    return transactions;
}

bool TONWalletAnalyzer::isExtensionInstalled() {
    // Проверяем наличие расширения в Chrome
    const char* home_dir = std::getenv("HOME");
    if (!home_dir) {
        LogError("Failed to get HOME environment variable");
        return false;
    }
    
    std::string extension_path = std::string(home_dir) + 
        "/Library/Application Support/Google/Chrome/Default/Extensions/" + extension_id_;
    
    return std::filesystem::exists(extension_path);
}

bool TONWalletAnalyzer::isDesktopAppInstalled() {
    // Проверяем наличие десктопного приложения
    return crypto::isWalletInstalledMacOS("TON Wallet");
}

} // namespace mceas