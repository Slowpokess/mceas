#ifndef MCEAS_VERSION_H
#define MCEAS_VERSION_H

namespace mceas {

// Информация о версии
struct Version {
    static constexpr int MAJOR = 0;
    static constexpr int MINOR = 1;
    static constexpr int PATCH = 0;
    static constexpr const char* BUILD_DATE = __DATE__;
    static constexpr const char* BUILD_TIME = __TIME__;
    
    // Полная строка версии
    static const char* getVersionString() {
        return "MCEAS Agent v0.1.0";
    }
};

} // namespace mceas

#endif // MCEAS_VERSION_H