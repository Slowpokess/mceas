#include "internal/logging.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <unistd.h>

namespace mceas {
namespace platform {
    // Простая реализация для macOS
    std::string getHostname() {
        char hostname[1024];
        gethostname(hostname, 1024);
        return std::string(hostname);
    }

    std::string getUserName() {
        const char* user = getenv("USER");
        return user ? std::string(user) : "unknown";
    }
}

// Функции-обертки для логирования уже реализованы в заголовочном файле,
// поскольку они объявлены как inline

} // namespace mceas