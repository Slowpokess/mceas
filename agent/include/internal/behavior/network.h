#ifndef MCEAS_NETWORK_MONITOR_H
#define MCEAS_NETWORK_MONITOR_H

#include "internal/behavior_module.h"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <chrono>

namespace mceas {

// Реализация монитора сетевой активности
class NetworkMonitorImpl : public NetworkMonitor {
public:
    NetworkMonitorImpl();
    ~NetworkMonitorImpl() override = default;
    
    // Реализация методов интерфейса NetworkMonitor
    std::vector<NetworkConnectionInfo> getActiveConnections() override;
    
    std::vector<NetworkConnectionInfo> getProcessConnections(int pid) override;
    
    std::vector<NetworkConnectionInfo> getConnectionHistory(
        std::chrono::system_clock::time_point from,
        std::chrono::system_clock::time_point to) override;
    
    std::vector<std::string> checkForSuspiciousConnections() override;
    
private:
    // Обновление информации о сетевых соединениях
    void updateConnectionInfo();
    
    // Чтение информации о сетевых соединениях из системы
    void readSystemConnections();
    
    // Проверка подозрительных IP-адресов
    std::vector<std::string> checkSuspiciousIPs();
    
    // Обнаружение необычных портов
    std::vector<std::string> detectUnusualPorts();
    
    // Обнаружение аномальной сетевой активности
    std::vector<std::string> detectAbnormalTraffic();
    
    // Кэш активных соединений
    std::vector<NetworkConnectionInfo> active_connections_;
    
    // История соединений
    std::vector<NetworkConnectionInfo> connection_history_;
    
    // Отображение соединений по процессам
    std::map<int, std::vector<NetworkConnectionInfo>> process_connections_;
    
    // Список известных подозрительных IP-адресов
    std::set<std::string> suspicious_ips_;
    
    // Список известных легитимных портов
    std::set<int> legitimate_ports_;
    
    // Базовые показатели сетевого трафика
    std::map<std::string, std::pair<uint64_t, uint64_t>> traffic_baseline_;
    
    // Время последнего обновления
    std::chrono::system_clock::time_point last_update_;
    
    // Мьютекс для потокобезопасности
    mutable std::mutex mutex_;
};

} // namespace mceas

#endif // MCEAS_NETWORK_MONITOR_H