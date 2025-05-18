#include "internal/crypto/metamask.h"
#include "internal/logging.h"
#include "internal/utils/platform_utils.h"
#include <filesystem>
#include <fstream>
#include "external/json/json.hpp"

namespace mceas {

MetamaskAnalyzer::MetamaskAnalyzer(BrowserType browser_type)
    : browser_type_(browser_type), is_installed_(false) {
    
    try {
        std::string browser_name;
        
        switch (browser_type) {
            case BrowserType::CHROME:
                browser_name = "Chrome";
                break;
            case BrowserType::FIREFOX:
                browser_name = "Firefox";
                break;
            case BrowserType::EDGE:
                browser_name = "Edge";
                break;
            default:
                LogError("Unsupported browser type for MetaMask");
                return;
        }
        
        // Проверяем наличие расширения MetaMask
        is_installed_ = utils::IsBrowserExtensionInstalled(browser_name, extension_id_);
        
        if (is_installed_) {
            // Получаем путь к расширению
            extension_path_ = utils::GetExtensionPath(browser_name, extension_id_);
            
            // Получаем путь к локальному хранилищу
            storage_path_ = utils::GetBrowserStoragePath(browser_name);
            
            // Получаем версию расширения
            version_ = utils::GetExtensionVersion(browser_name, extension_id_);
        }
    }
    catch (const std::exception& e) {
        LogError("Error initializing MetaMask analyzer: " + std::string(e.what()));
    }
}

bool MetamaskAnalyzer::isInstalled() {
    return is_installed_;
}

WalletType MetamaskAnalyzer::getType() {
    return WalletType::METAMASK;
}

std::string MetamaskAnalyzer::getName() {
    return "MetaMask";
}

std::string MetamaskAnalyzer::getVersion() {
    return version_;
}

std::string MetamaskAnalyzer::getWalletPath() {
    return extension_path_;
}

std::map<std::string, std::string> MetamaskAnalyzer::collectData() {
    std::map<std::string, std::string> data;
    
    if (!isInstalled()) {
        LogWarning("MetaMask is not installed, cannot collect data");
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
        auto transactions = getTransactions(10); // Ограничиваем 10 транзакциями
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
        LogError("Error collecting MetaMask data: " + std::string(e.what()));
    }
    
    return data;
}

std::vector<CryptoAddress> MetamaskAnalyzer::getAddresses() {
    std::vector<CryptoAddress> addresses;
    
    try {
        // Заглушка: здесь должен быть код для извлечения адресов из хранилища MetaMask
        // В реальном приложении здесь будет работа с локальным хранилищем браузера
        // и/или анализ файлов state.json
        
        LogInfo("MetaMask address extraction is not fully implemented yet");
        
        // Для тестирования можно добавить примерный адрес
        CryptoAddress sample_address;
        sample_address.currency = "ETH";
        sample_address.address = "0x...";  // Заглушка для адреса
        sample_address.balance = 0.0;
        sample_address.label = "Account 1";
        
        addresses.push_back(sample_address);
    }
    catch (const std::exception& e) {
        LogError("Error extracting MetaMask addresses: " + std::string(e.what()));
    }
    
    return addresses;
}

std::vector<Transaction> MetamaskAnalyzer::getTransactions(int limit) {
    std::vector<Transaction> transactions;
    
    try {
        // Заглушка: здесь должен быть код для извлечения транзакций из хранилища MetaMask
        
        LogInfo("MetaMask transaction extraction is not fully implemented yet");
    }
    catch (const std::exception& e) {
        LogError("Error extracting MetaMask transactions: " + std::string(e.what()));
    }
    
    return transactions;
}

std::map<std::string, std::string> MetamaskAnalyzer::getTokens() {
    std::map<std::string, std::string> tokens;
    
    try {
        // Заглушка: здесь должен быть код для извлечения информации о токенах из хранилища MetaMask
        
        LogInfo("MetaMask token extraction is not fully implemented yet");
    }
    catch (const std::exception& e) {
        LogError("Error extracting MetaMask tokens: " + std::string(e.what()));
    }
    
    return tokens;
}

} // namespace mceas