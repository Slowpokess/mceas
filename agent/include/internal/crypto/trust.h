#ifndef MCEAS_CRYPTO_TRUST_H
#define MCEAS_CRYPTO_TRUST_H

#include "internal/crypto_module.h"
#include "internal/crypto/common.h"

namespace mceas {

// Анализатор для Trust Wallet (мобильное приложение с десктопной синхронизацией)
class TrustWalletAnalyzer : public WalletAnalyzer {
public:
    TrustWalletAnalyzer();
    
    // Реализация методов интерфейса WalletAnalyzer
    bool isInstalled() override;
    WalletType getType() override;
    std::string getName() override;
    std::string getVersion() override;
    std::string getWalletPath() override;
    std::map<std::string, std::string> collectData() override;
    
private:
    // Методы анализа данных Trust Wallet
    std::vector<CryptoAddress> getAddresses();
    std::vector<Transaction> getTransactions(int limit = 10);
    std::map<std::string, std::string> getSupportedChains();
    
    // Кэшированные данные
    std::string version_;
    std::string application_path_;
    std::string data_path_;
    bool is_installed_;
};

} // namespace mceas

#endif // MCEAS_CRYPTO_TRUST_H