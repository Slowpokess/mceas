#ifndef MCEAS_PLATFORM_H
#define MCEAS_PLATFORM_H

#include <string> 

// Определения для разных платформ
#if defined(_WIN32) || defined(_WIN64)
    #define OS_WINDOWS
#elif defined(__APPLE__) && defined(__MACH__)
    #define OS_MACOS
#elif defined(__linux__)
    #define OS_LINUX
#else
    #error "Unsupported platform"
#endif

namespace mceas {
namespace platform {

// Функции для работы с платформозависимыми особенностями
bool isWindows();
bool isMacOS();
bool isLinux();
std::string getPlatformName();
std::string getHostname();
std::string getUserName();
std::string getOSVersion();

} // namespace platform
} // namespace mceas

#endif // MCEAS_PLATFORM_H