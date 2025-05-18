#ifndef MCEAS_CRYPTO_EXODUS_H
#define MCEAS_CRYPTO_EXODUS_H

#include "internal/crypto_module.h"
#include "internal/crypto/common.h"

namespace mceas {

// Анализатор для кошелька Exodus (настольное приложение)
class ExodusAnalyzer : public WalletAnalyzer {
public:
    ExodusAnalyzer();
    
    // Реализация методов интерфейса WalletAnalyzer
    bool isInstalled() override;
    WalletType getType() override;
    std::string getName() override;
    std::string getVersion() override;
    std::string getWalletPath() override;
    std::map<std::string, std::string> collectData() override;
    
private:
    // Методы анализа данных Exodus
    std::vector<CryptoAddress> getAddresses();
    std::vector<Transaction> getTransactions(int limit = 10);
    std::map<std::string, std::string> getAssets();
    
    // Кэшированные данные
    std::string version_;
    std::string application_path_;
    std::string data_path_;
    bool is_installed_;
};

} // namespace mceas

#endif // MCEAS_CRYPTO_EXODUS_H