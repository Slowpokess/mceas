#ifndef MCEAS_CRYPTO_TON_H
#define MCEAS_CRYPTO_TON_H

#include "internal/crypto_module.h"
#include "browser/browser_module.h"
#include "internal/crypto/common.h"

namespace mceas {

// Анализатор для TON Wallet (браузерное расширение и десктопное приложение)
class TONWalletAnalyzer : public WalletAnalyzer {
public:
    // Конструктор
    TONWalletAnalyzer();
    
    // Реализация методов интерфейса WalletAnalyzer
    bool isInstalled() override;
    WalletType getType() override;
    std::string getName() override;
    std::string getVersion() override;
    std::string getWalletPath() override;
    std::map<std::string, std::string> collectData() override;
    
private:
    // Методы анализа данных TON Wallet
    std::vector<CryptoAddress> getAddresses();
    std::vector<Transaction> getTransactions(int limit = 10);
    bool isExtensionInstalled();
    bool isDesktopAppInstalled();
    
    // Кэшированные данные
    std::string version_;
    std::string extension_path_;
    std::string application_path_;
    bool is_extension_installed_;
    bool is_app_installed_;
    
    // ID расширения TON Wallet для Chrome
    const std::string extension_id_ = "nphplpgoakhhjchkkhmiggakijnkhfnd";
};

} // namespace mceas

#endif // MCEAS_CRYPTO_TON_H