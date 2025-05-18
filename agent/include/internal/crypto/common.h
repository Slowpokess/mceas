#ifndef MCEAS_CRYPTO_COMMON_H
#define MCEAS_CRYPTO_COMMON_H

#include <string>
#include <vector>
#include <map>
#include <set>

namespace mceas {

// Структура для данных о криптовалютном адресе
struct CryptoAddress {
    std::string currency;    // Название криптовалюты (BTC, ETH, и т.д.)
    std::string address;     // Адрес кошелька
    double balance;          // Баланс (если доступен)
    std::string label;       // Метка/название (если есть)
};

// Структура для данных о транзакции
struct Transaction {
    std::string tx_id;          // ID транзакции
    std::string from_address;   // Адрес отправителя
    std::string to_address;     // Адрес получателя
    double amount;              // Сумма
    std::string currency;       // Валюта
    std::string timestamp;      // Временная метка
    std::string status;         // Статус транзакции
};

namespace crypto {

// Структура для данных о потенциальной seed-фразе
struct SeedPhraseInfo {
    std::string file_path;    // Путь к файлу, где найдена фраза
    std::string content;      // Найденная фраза (может быть замаскирована)
    std::string type;         // Тип (BIP-39, TON, Solana и т.д.)
    int word_count;           // Количество слов
    double confidence;        // Уверенность, что это seed-фраза (0.0-1.0)
};

// Проверка, установлен ли Metamask в Chrome
bool isMetamaskInstalledChrome();

// Проверка, установлен ли Metamask в Firefox
bool isMetamaskInstalledFirefox();

// Функция для поиска текста, похожего на seed-фразы
std::vector<SeedPhraseInfo> findPotentialSeedPhrases(const std::string& directory_path, bool mask_phrases = true);

// Функция для проверки, является ли текст seed-фразой
bool isPotentialSeedPhrase(const std::string& text, std::string& type, double& confidence);

// Функция для маскирования seed-фразы для безопасного хранения в логах
std::string maskSeedPhrase(const std::string& phrase);

// Функция для получения словаря BIP-39
std::set<std::string> getBIP39WordList();

} // namespace crypto

} // namespace mceas

#endif // MCEAS_CRYPTO_COMMON_H