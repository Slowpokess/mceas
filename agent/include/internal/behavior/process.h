#ifndef MCEAS_PROCESS_MONITOR_H
#define MCEAS_PROCESS_MONITOR_H

#include "internal/behavior_module.h"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <chrono>

namespace mceas {

// Реализация монитора процессов
class ProcessMonitorImpl : public ProcessMonitor {
public:
    ProcessMonitorImpl();
    ~ProcessMonitorImpl() override = default;
    
    // Реализация методов интерфейса ProcessMonitor
    std::vector<ProcessInfo> getRunningProcesses() override;
    
    ProcessInfo getProcessInfo(int pid) override;
    
    std::vector<ProcessInfo> getChildProcesses(int pid) override;
    
    std::vector<ProcessInfo> getProcessHistory(
        std::chrono::system_clock::time_point from,
        std::chrono::system_clock::time_point to) override;
    
    std::vector<std::string> checkForSuspiciousProcesses() override;
    
private:
    // Обновление информации о процессах
    void updateProcessInfo();
    
    // Чтение информации о процессах из системы
    void readSystemProcesses();
    
    // Обновление дерева процессов
    void updateProcessTree();
    
    // Проверка наличия известных вредоносных процессов
    std::vector<std::string> checkMaliciousProcesses();
    
    // Обнаружение процессов с высоким потреблением ресурсов
    std::vector<std::string> detectHighResourceUsage();
    
    // Обнаружение необычных шаблонов выполнения
    std::vector<std::string> detectUnusualExecutionPatterns();
    
    // Кэш активных процессов
    std::vector<ProcessInfo> running_processes_;
    
    // История процессов
    std::vector<ProcessInfo> process_history_;
    
    // Отображение для быстрого поиска процесса по PID
    std::map<int, ProcessInfo> process_map_;
    
    // Дерево родительских и дочерних процессов
    std::map<int, std::vector<int>> process_tree_;
    
    // Список известных вредоносных процессов
    std::set<std::string> known_malicious_processes_;
    
    // Статистика потребления ресурсов
    std::map<std::string, std::pair<double, double>> resource_usage_baseline_;
    
    // Время последнего обновления
    std::chrono::system_clock::time_point last_update_;
    
    // Мьютекс для потокобезопасности
    mutable std::mutex mutex_;
};

} // namespace mceas

#endif // MCEAS_PROCESS_MONITOR_H