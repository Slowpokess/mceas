#include "internal/crypto/exodus.h"
#include "internal/logging.h"
#include "internal/utils/platform_utils.h"
#include <filesystem>
#include <fstream>
#include "external/json/json.hpp"

namespace mceas {

ExodusAnalyzer::ExodusAnalyzer() : is_installed_(false) {
    try {
        // Проверяем, установлен ли Exodus
        is_installed_ = utils::IsAppInstalled("Exodus");
        
        if (is_installed_) {
            // Получаем путь к приложению
            application_path_ = utils::GetAppPath("Exodus");
            
            // Получаем версию
            version_ = utils::GetAppVersion("Exodus");
            
            // Получаем путь к данным Exodus
            data_path_ = utils::GetAppDataPath("Exodus");
        }
    }
    catch (const std::exception& e) {
        LogError("Error initializing Exodus analyzer: " + std::string(e.what()));
    }
}


bool ExodusAnalyzer::isInstalled() {
    return is_installed_;
}

WalletType ExodusAnalyzer::getType() {
    return WalletType::EXODUS;
}

std::string ExodusAnalyzer::getName() {
    return "Exodus";
}

std::string ExodusAnalyzer::getVersion() {
    return version_;
}

std::string ExodusAnalyzer::getWalletPath() {
    return data_path_;
}

std::map<std::string, std::string> ExodusAnalyzer::collectData() {
    std::map<std::string, std::string> data;
    
    if (!isInstalled()) {
        LogWarning("Exodus is not installed, cannot collect data");
        return data;
    }
    
    try {
        // Добавляем базовую информацию
        data["version"] = version_;
        data["application_path"] = application_path_;
        data["data_path"] = data_path_;
        
        // Получаем адреса
        auto addresses = getAddresses();
        data["addresses_count"] = std::to_string(addresses.size());
        
        // Получаем транзакции
        auto transactions = getTransactions(10); // Ограничиваем 10 транзакциями
        data["transactions_count"] = std::to_string(transactions.size());
        
        // Получаем активы
        auto assets = getAssets();
        data["assets_count"] = std::to_string(assets.size());
        
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
        LogError("Error collecting Exodus data: " + std::string(e.what()));
    }
    
    return data;
}

std::vector<CryptoAddress> ExodusAnalyzer::getAddresses() {
    std::vector<CryptoAddress> addresses;
    
    try {
        // Заглушка: здесь должен быть код для извлечения адресов из хранилища Exodus
        // В реальном приложении здесь будет работа с файлами в ~/Library/Application Support/Exodus
        
        LogInfo("Exodus address extraction is not fully implemented yet");
        
        // Для тестирования можно добавить примерный адрес
        CryptoAddress sample_address;
        sample_address.currency = "BTC";
        sample_address.address = "bc1...";  // Заглушка для адреса
        sample_address.balance = 0.0;
        sample_address.label = "Bitcoin Wallet";
        
        addresses.push_back(sample_address);
    }
    catch (const std::exception& e) {
        LogError("Error extracting Exodus addresses: " + std::string(e.what()));
    }
    
    return addresses;
}

std::vector<Transaction> ExodusAnalyzer::getTransactions(int limit) {
    std::vector<Transaction> transactions;
    
    try {
        // Заглушка: здесь должен быть код для извлечения транзакций из хранилища Exodus
        
        LogInfo("Exodus transaction extraction is not fully implemented yet");
    }
    catch (const std::exception& e) {
        LogError("Error extracting Exodus transactions: " + std::string(e.what()));
    }
    
    return transactions;
}

std::map<std::string, std::string> ExodusAnalyzer::getAssets() {
    std::map<std::string, std::string> assets;
    
    try {
        // Заглушка: здесь должен быть код для извлечения информации о активах из хранилища Exodus
        
        LogInfo("Exodus asset extraction is not fully implemented yet");
    }
    catch (const std::exception& e) {
        LogError("Error extracting Exodus assets: " + std::string(e.what()));
    }
    
    return assets;
}

} // namespace mceas