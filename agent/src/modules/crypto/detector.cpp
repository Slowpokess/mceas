#include "internal/crypto/detector.h"
#include "internal/logging.h"
#include "internal/crypto/metamask.h"
#include "internal/crypto/exodus.h"
#include "internal/crypto/trust.h"
#include "internal/crypto/ton.h"
#include "internal/crypto/solana.h"
#include "internal/crypto/common.h"
#include "internal/utils/platform_utils.h"
#include <filesystem>

namespace mceas {

WalletDetector::WalletDetector() {
    // Инициализация
}

std::vector<std::unique_ptr<WalletAnalyzer>> WalletDetector::detectInstalledWallets() {
    std::vector<std::unique_ptr<WalletAnalyzer>> detected_wallets;
    
    LogInfo("Starting cryptocurrency wallet detection");
    
    // Проверяем настольные приложения
    if (checkDesktopWallets()) {
        LogInfo("Checking desktop wallet applications");
        
        // Проверяем Exodus
        if (utils::IsAppInstalled("Exodus")) {
            LogInfo("Exodus wallet detected");
            detected_wallets.push_back(std::make_unique<ExodusAnalyzer>());
        }
        
        // Проверяем Trust Wallet
        if (utils::IsAppInstalled("Trust Wallet")) {
            LogInfo("Trust Wallet detected");
            detected_wallets.push_back(std::make_unique<TrustWalletAnalyzer>());
        }
        
        // Проверяем TON Wallet десктопное приложение
        if (utils::IsAppInstalled("TON Wallet")) {
            LogInfo("TON Wallet desktop application detected");
            detected_wallets.push_back(std::make_unique<TONWalletAnalyzer>());
        }
    }
    
    // Проверяем расширения браузеров
    if (checkBrowserWallets()) {
        LogInfo("Checking browser wallet extensions");
        
        // Проверяем MetaMask в Chrome
        if (utils::IsBrowserExtensionInstalled("Chrome", "nkbihfbeogaeaoehlefnkodbefgpgknn")) {
            LogInfo("MetaMask extension detected in Chrome");
            detected_wallets.push_back(std::make_unique<MetamaskAnalyzer>(BrowserType::CHROME));
        }
        
        // Проверяем MetaMask в Firefox
        if (crypto::isMetamaskInstalledFirefox()) {
            LogInfo("MetaMask extension detected in Firefox");
            detected_wallets.push_back(std::make_unique<MetamaskAnalyzer>(BrowserType::FIREFOX));
        }
        
        // Проверяем Phantom в Chrome (Solana)
        if (utils::IsBrowserExtensionInstalled("Chrome", "bfnaelmomeimhlpmgjnjophhpkkoljpa")) {
            LogInfo("Phantom wallet extension detected in Chrome");
            detected_wallets.push_back(std::make_unique<PhantomWalletAnalyzer>(BrowserType::CHROME));
        }
        
        // Проверяем TON Wallet в Chrome
        if (utils::IsBrowserExtensionInstalled("Chrome", "nphplpgoakhhjchkkhmiggakijnkhfnd")) {
            LogInfo("TON Wallet extension detected in Chrome");
            detected_wallets.push_back(std::make_unique<TONWalletAnalyzer>());
        }
    }
    
    // Проверяем файлы кошельков на диске
    if (checkWalletFiles()) {
        LogInfo("Checking wallet files on disk");
        
        // Здесь будет код проверки наличия файлов кошельков
        // Например, Bitcoin Core, Electrum и других
    }
    
    LogInfo("Wallet detection completed. Found " + std::to_string(detected_wallets.size()) + " wallets");
    
    return detected_wallets;
}

bool WalletDetector::checkDesktopWallets() {
    // Проверяем наличие популярных настольных кошельков
    bool found_any = false;
    
    // Проверяем Exodus
    if (utils::IsAppInstalled("Exodus")) {
        LogInfo("Found Exodus wallet application");
        found_any = true;
    }
    
    // Проверяем Electrum
    if (utils::IsAppInstalled("Electrum")) {
        LogInfo("Found Electrum wallet application");
        found_any = true;
    }
    
    // Проверяем Trust Wallet
    if (utils::IsAppInstalled("Trust Wallet")) {
        LogInfo("Found Trust Wallet application");
        found_any = true;
    }
    
    // Проверяем TON Wallet
    if (utils::IsAppInstalled("TON Wallet")) {
        LogInfo("Found TON Wallet application");
        found_any = true;
    }
    
    return found_any;
}

bool WalletDetector::checkBrowserWallets() {
    // Проверяем наличие расширений-кошельков в браузерах
    bool found_any = false;
    
    // Проверяем MetaMask в Chrome
    if (utils::IsBrowserExtensionInstalled("Chrome", "nkbihfbeogaeaoehlefnkodbefgpgknn")) {
        LogInfo("Found MetaMask extension in Chrome");
        found_any = true;
    }
    
    // Проверяем MetaMask в Firefox
    if (crypto::isMetamaskInstalledFirefox()) {
        LogInfo("Found MetaMask extension in Firefox");
        found_any = true;
    }
    
    // Проверяем Phantom в Chrome
    if (utils::IsBrowserExtensionInstalled("Chrome", "bfnaelmomeimhlpmgjnjophhpkkoljpa")) {
        LogInfo("Found Phantom extension in Chrome");
        found_any = true;
    }
    
    // Проверяем TON Wallet в Chrome
    if (utils::IsBrowserExtensionInstalled("Chrome", "nphplpgoakhhjchkkhmiggakijnkhfnd")) {
        LogInfo("Found TON Wallet extension in Chrome");
        found_any = true;
    }
    
    return found_any;
}

bool WalletDetector::checkWalletFiles() {
    // Проверяем наличие файлов кошельков на диске
    bool found_any = false;
    std::string home = utils::GetHomeDir();
    
    // Проверяем наличие Bitcoin Core
    #if defined(_WIN32)
        std::string bitcoin_dir = home + "\\AppData\\Roaming\\Bitcoin";
    #elif defined(__APPLE__)
        std::string bitcoin_dir = home + "/Library/Application Support/Bitcoin";
    #else
        std::string bitcoin_dir = home + "/.bitcoin";
    #endif
    
    if (utils::FileExists(bitcoin_dir)) {
        LogInfo("Found Bitcoin Core wallet files");
        found_any = true;
    }
    
    // Проверяем наличие Electrum
    #if defined(_WIN32)
        std::string electrum_dir = home + "\\AppData\\Roaming\\Electrum";
    #elif defined(__APPLE__)
        std::string electrum_dir = home + "/Library/Application Support/Electrum";
    #else
        std::string electrum_dir = home + "/.electrum";
    #endif
    
    if (utils::FileExists(electrum_dir)) {
        LogInfo("Found Electrum wallet files");
        found_any = true;
    }
    
    return found_any;
}

} // namespace mceas