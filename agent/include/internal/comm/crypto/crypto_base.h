#ifndef MCEAS_COMM_CRYPTO_BASE_H
#define MCEAS_COMM_CRYPTO_BASE_H

#include <vector>
#include <string>
#include <optional>
#include <memory>

namespace mceas {
namespace comm {
namespace crypto {

// Конфигурация шифрования
struct CryptoConfig {
    std::string algorithm;      // Алгоритм шифрования
    std::string key;            // Ключ или путь к ключу
    std::string iv;             // Вектор инициализации
    std::string cert_path;      // Путь к сертификату (для SSL/TLS)
    bool verify_peer;           // Проверять ли сертификат сервера
    
    // Конструктор с настройками по умолчанию
    CryptoConfig() : algorithm("AES-256-CBC"), verify_peer(true) {}
};

// Базовый интерфейс шифрования
class ICrypto {
public:
    virtual ~ICrypto() = default;
    
    // Инициализация с заданной конфигурацией
    virtual bool initialize(const CryptoConfig& config) = 0;
    
    // Шифрование данных
    virtual std::vector<uint8_t> encrypt(const std::vector<uint8_t>& data) = 0;
    
    // Расшифровка данных
    virtual std::optional<std::vector<uint8_t>> decrypt(const std::vector<uint8_t>& data) = 0;
    
    // Создание хеша данных
    virtual std::string hash(const std::vector<uint8_t>& data) = 0;
    
    // Проверка хеша
    virtual bool verifyHash(const std::vector<uint8_t>& data, const std::string& hash) = 0;
};

// Фабрика для создания объектов шифрования
class CryptoFactory {
public:
    // Создание объекта шифрования по алгоритму
    static std::unique_ptr<ICrypto> createCrypto(const std::string& algorithm);
    
    // Список поддерживаемых алгоритмов
    static std::vector<std::string> getAvailableAlgorithms();
};

} // namespace crypto
} // namespace comm
} // namespace mceas

#endif // MCEAS_COMM_CRYPTO_BASE_H