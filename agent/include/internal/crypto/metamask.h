#ifndef MCEAS_CRYPTO_METAMASK_H
#define MCEAS_CRYPTO_METAMASK_H

#include "internal/crypto_module.h"
#include "browser/browser_module.h"
#include "internal/crypto/common.h"

namespace mceas {

// Анализатор для кошелька MetaMask (браузерное расширение)
class MetamaskAnalyzer : public WalletAnalyzer {
public:
    // Конструктор с указанием браузера, в котором установлено расширение
    explicit MetamaskAnalyzer(BrowserType browser_type);
    
    // Реализация методов интерфейса WalletAnalyzer
    bool isInstalled() override;
    WalletType getType() override;
    std::string getName() override;
    std::string getVersion() override;
    std::string getWalletPath() override;
    std::map<std::string, std::string> collectData() override;
    
private:
    // Методы анализа данных MetaMask
    std::vector<CryptoAddress> getAddresses();
    std::vector<Transaction> getTransactions(int limit = 10);
    std::map<std::string, std::string> getTokens();
    
    // Тип браузера, в котором установлено расширение
    BrowserType browser_type_;
    
    // Кэшированные данные
    std::string version_;
    std::string extension_path_;
    std::string storage_path_;
    bool is_installed_;
    
    // ID расширения MetaMask
    const std::string extension_id_ = "nkbihfbeogaeaoehlefnkodbefgpgknn";
};

} // namespace mceas

#endif // MCEAS_CRYPTO_METAMASK_H