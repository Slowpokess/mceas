#ifndef MCEAS_CRYPTO_DETECTOR_H
#define MCEAS_CRYPTO_DETECTOR_H

#include "internal/crypto_module.h"
#include <vector>
#include <string>

namespace mceas {

// Класс для обнаружения различных криптокошельков
class WalletDetector {
public:
    // Конструктор
    WalletDetector();
    
    // Обнаружение всех поддерживаемых кошельков
    std::vector<std::unique_ptr<WalletAnalyzer>> detectInstalledWallets();
    
private:
    // Проверка наличия настольных приложений-кошельков
    bool checkDesktopWallets();
    
    // Проверка наличия расширений-кошельков в браузерах
    bool checkBrowserWallets();
    
    // Проверка наличия файлов-кошельков на диске
    bool checkWalletFiles();
};

} // namespace mceas

#endif // MCEAS_CRYPTO_DETECTOR_H