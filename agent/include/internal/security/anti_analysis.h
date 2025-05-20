#ifndef MCEAS_SECURITY_ANTI_ANALYSIS_H
#define MCEAS_SECURITY_ANTI_ANALYSIS_H

#include <string>
#include <vector>
#include <functional>

namespace mceas {
namespace security {

// Типы обнаружения
enum class DetectionType {
    VIRTUAL_MACHINE,
    SANDBOX,
    DEBUGGER,
    ANALYSIS_TOOLS,
    FORENSIC_TOOLS,
    SUSPICIOUS_ENVIRONMENT
};

// Результат проверки
struct DetectionResult {
    bool detected;
    DetectionType type;
    std::string description;
    int confidence; // 0-100
};

// Класс для обнаружения аналитических сред
class AntiAnalysis {
public:
    // Конструктор
    AntiAnalysis();
    
    // Запуск всех проверок
    std::vector<DetectionResult> runAllChecks();
    
    // Проверка на виртуальную машину
    DetectionResult checkVirtualMachine();
    
    // Проверка на песочницу
    DetectionResult checkSandbox();
    
    // Проверка на отладчик
    DetectionResult checkDebugger();
    
    // Проверка на инструменты анализа
    DetectionResult checkAnalysisTools();
    
    // Проверка на инструменты форензики
    DetectionResult checkForensicTools();
    
    // Проверка на подозрительное окружение
    DetectionResult checkSuspiciousEnvironment();
    
    // Установка обработчика для результатов проверки
    void setDetectionHandler(std::function<void(const DetectionResult&)> handler);
    
    // Включение/выключение проверок
    void enableCheck(DetectionType type, bool enable);
    
    // Проверка, включена ли проверка
    bool isCheckEnabled(DetectionType type) const;
    
private:
    // VMware-специфичные проверки
    bool detectVMware();
    
    // VirtualBox-специфичные проверки
    bool detectVirtualBox();
    
    // KVM/QEMU-специфичные проверки
    bool detectKVM();
    
    // Hyper-V-специфичные проверки
    bool detectHyperV();
    
    // Общие проверки на виртуализацию
    bool detectGenericVM();
    
    // Проверка на инструменты анализа сети
    bool detectNetworkAnalysis();
    
    // Проверка на специфичные файлы и процессы
    bool detectSpecificFiles();
    bool detectSpecificProcesses();
    
    // Проверка на аномалии в системных параметрах
    bool detectSystemAnomalies();
    
    // Включенные проверки
    std::vector<DetectionType> enabled_checks_;
    
    // Обработчик для результатов проверки
    std::function<void(const DetectionResult&)> detection_handler_;
};

} // namespace security
} // namespace mceas

#endif // MCEAS_SECURITY_ANTI_ANALYSIS_H