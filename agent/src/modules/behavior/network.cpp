#include "behavior/network.h"
#include "internal/logging.h"
#include "internal/utils/platform_utils.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <array>
#include <memory>
#include <unordered_map>

#if defined(_WIN32)
#include <windows.h>
#include <iphlpapi.h>
#include <tcpmib.h>
#pragma comment(lib, "iphlpapi.lib")
#elif defined(__APPLE__)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/sysctl.h>
#include <netinet/tcp_var.h>
#include <net/route.h>
#elif defined(__linux__)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

namespace mceas {

NetworkMonitorImpl::NetworkMonitorImpl() 
    : last_update_(std::chrono::system_clock::now() - std::chrono::hours(24)) {
    // Инициализируем список известных подозрительных IP-адресов
    suspicious_ips_.insert("1.2.3.4");  // Пример подозрительного IP
    
    // Инициализируем список известных легитимных портов
    legitimate_ports_.insert(80);    // HTTP
    legitimate_ports_.insert(443);   // HTTPS
    legitimate_ports_.insert(22);    // SSH
    legitimate_ports_.insert(21);    // FTP
    legitimate_ports_.insert(23);    // Telnet
    legitimate_ports_.insert(25);    // SMTP
    legitimate_ports_.insert(53);    // DNS
    legitimate_ports_.insert(110);   // POP3
    legitimate_ports_.insert(143);   // IMAP
    legitimate_ports_.insert(3306);  // MySQL
    legitimate_ports_.insert(5432);  // PostgreSQL
    legitimate_ports_.insert(27017); // MongoDB
    
    // Выполняем первоначальное обновление информации о сетевых соединениях
    updateConnectionInfo();
}

std::vector<NetworkConnectionInfo> NetworkMonitorImpl::getActiveConnections() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Обновляем информацию о соединениях, если прошло достаточно времени
    auto now = std::chrono::system_clock::now();
    if (now - last_update_ > std::chrono::seconds(30)) {
        updateConnectionInfo();
    }
    
    return active_connections_;
}

std::vector<NetworkConnectionInfo> NetworkMonitorImpl::getProcessConnections(int pid) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = process_connections_.find(pid);
    if (it != process_connections_.end()) {
        return it->second;
    }
    
    return {};
}

std::vector<NetworkConnectionInfo> NetworkMonitorImpl::getConnectionHistory(
    std::chrono::system_clock::time_point from,
    std::chrono::system_clock::time_point to) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<NetworkConnectionInfo> filtered_history;
    
    for (const auto& connection : connection_history_) {
        if (connection.established_time >= from && connection.established_time <= to) {
            filtered_history.push_back(connection);
        }
    }
    
    return filtered_history;
}

std::vector<std::string> NetworkMonitorImpl::checkForSuspiciousConnections() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> alerts;
    
    // Проверяем подозрительные IP-адреса
    auto suspicious_ips = checkSuspiciousIPs();
    alerts.insert(alerts.end(), suspicious_ips.begin(), suspicious_ips.end());
    
    // Проверяем необычные порты
    auto unusual_ports = detectUnusualPorts();
    alerts.insert(alerts.end(), unusual_ports.begin(), unusual_ports.end());
    
    // Проверяем аномальный сетевой трафик
    auto abnormal_traffic = detectAbnormalTraffic();
    alerts.insert(alerts.end(), abnormal_traffic.begin(), abnormal_traffic.end());
    
    return alerts;
}

void NetworkMonitorImpl::updateConnectionInfo() {
    try {
        // Сохраняем текущие соединения в историю
        for (const auto& connection : active_connections_) {
            connection_history_.push_back(connection);
        }
        
        // Ограничиваем размер истории
        const size_t max_history_size = 1000;
        if (connection_history_.size() > max_history_size) {
            connection_history_.erase(connection_history_.begin(), 
                                    connection_history_.begin() + (connection_history_.size() - max_history_size));
        }
        
        // Очищаем список текущих соединений
        active_connections_.clear();
        process_connections_.clear();
        
        // Читаем информацию о сетевых соединениях из системы
        readSystemConnections();
        
        // Обновляем время последнего обновления
        last_update_ = std::chrono::system_clock::now();
        
        LogInfo("Network connection information updated. Active connections: " + 
                 std::to_string(active_connections_.size()));
    }
    catch (const std::exception& e) {
        LogError("Error updating network connection information: " + std::string(e.what()));
    }
}

void NetworkMonitorImpl::readSystemConnections() {
#if defined(_WIN32)
    // Windows: используем GetExtendedTcpTable и GetExtendedUdpTable для получения соединений
    
    // Получаем TCP соединения
    DWORD size = 0;
    DWORD result = GetExtendedTcpTable(NULL, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    if (result == ERROR_INSUFFICIENT_BUFFER) {
        std::vector<char> buffer(size);
        PMIB_TCPTABLE_OWNER_PID tcpTable = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buffer.data());
        
        result = GetExtendedTcpTable(tcpTable, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
        if (result == NO_ERROR) {
            for (DWORD i = 0; i < tcpTable->dwNumEntries; i++) {
                NetworkConnectionInfo connection;
                connection.pid = tcpTable->table[i].dwOwningPid;
                connection.protocol = "TCP";
                
                // Преобразуем IP-адреса и порты
                IN_ADDR local_addr;
                local_addr.S_un.S_addr = tcpTable->table[i].dwLocalAddr;
                connection.local_address = inet_ntoa(local_addr);
                connection.local_port = ntohs(static_cast<USHORT>(tcpTable->table[i].dwLocalPort));
                
                IN_ADDR remote_addr;
                remote_addr.S_un.S_addr = tcpTable->table[i].dwRemoteAddr;
                connection.remote_address = inet_ntoa(remote_addr);
                connection.remote_port = ntohs(static_cast<USHORT>(tcpTable->table[i].dwRemotePort));
                
                // Определяем состояние соединения
                switch (tcpTable->table[i].dwState) {
                    case MIB_TCP_STATE_CLOSED:
                        connection.state = "CLOSED";
                        break;
                    case MIB_TCP_STATE_LISTEN:
                        connection.state = "LISTEN";
                        break;
                    case MIB_TCP_STATE_SYN_SENT:
                        connection.state = "SYN_SENT";
                        break;
                    case MIB_TCP_STATE_SYN_RCVD:
                        connection.state = "SYN_RCVD";
                        break;
                    case MIB_TCP_STATE_ESTAB:
                        connection.state = "ESTABLISHED";
                        break;
                    case MIB_TCP_STATE_FIN_WAIT1:
                        connection.state = "FIN_WAIT1";
                        break;
                    case MIB_TCP_STATE_FIN_WAIT2:
                        connection.state = "FIN_WAIT2";
                        break;
                    case MIB_TCP_STATE_CLOSE_WAIT:
                        connection.state = "CLOSE_WAIT";
                        break;
                    case MIB_TCP_STATE_CLOSING:
                        connection.state = "CLOSING";
                        break;
                    case MIB_TCP_STATE_LAST_ACK:
                        connection.state = "LAST_ACK";
                        break;
                    case MIB_TCP_STATE_TIME_WAIT:
                        connection.state = "TIME_WAIT";
                        break;
                    case MIB_TCP_STATE_DELETE_TCB:
                        connection.state = "DELETE_TCB";
                        break;
                    default:
                        connection.state = "UNKNOWN";
                        break;
                }
                
                // Устанавливаем время установления соединения
                connection.established_time = std::chrono::system_clock::now();
                
                // Получаем имя процесса
                HANDLE process_handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 
                                                 FALSE, connection.pid);
                if (process_handle) {
                    char process_name[MAX_PATH];
                    if (GetModuleBaseNameA(process_handle, NULL, process_name, MAX_PATH)) {
                        connection.process_name = process_name;
                    }
                    CloseHandle(process_handle);
                }
                
                active_connections_.push_back(connection);
                process_connections_[connection.pid].push_back(connection);
            }
        }
    }
    
    // Получаем UDP соединения
    size = 0;
    result = GetExtendedUdpTable(NULL, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
    if (result == ERROR_INSUFFICIENT_BUFFER) {
        std::vector<char> buffer(size);
        PMIB_UDPTABLE_OWNER_PID udpTable = reinterpret_cast<PMIB_UDPTABLE_OWNER_PID>(buffer.data());
        
        result = GetExtendedUdpTable(udpTable, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
        if (result == NO_ERROR) {
            for (DWORD i = 0; i < udpTable->dwNumEntries; i++) {
                NetworkConnectionInfo connection;
                connection.pid = udpTable->table[i].dwOwningPid;
                connection.protocol = "UDP";
                
                // Преобразуем IP-адреса и порты
                IN_ADDR local_addr;
                local_addr.S_un.S_addr = udpTable->table[i].dwLocalAddr;
                connection.local_address = inet_ntoa(local_addr);
                connection.local_port = ntohs(static_cast<USHORT>(udpTable->table[i].dwLocalPort));
                
                // UDP не имеет удаленного адреса
                connection.remote_address = "0.0.0.0";
                connection.remote_port = 0;
                
                // UDP не имеет состояния
                connection.state = "STATELESS";
                
                // Устанавливаем время установления соединения
                connection.established_time = std::chrono::system_clock::now();
                
                // Получаем имя процесса
                HANDLE process_handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 
                                                 FALSE, connection.pid);
                if (process_handle) {
                    char process_name[MAX_PATH];
                    if (GetModuleBaseNameA(process_handle, NULL, process_name, MAX_PATH)) {
                        connection.process_name = process_name;
                    }
                    CloseHandle(process_handle);
                }
                
                active_connections_.push_back(connection);
                process_connections_[connection.pid].push_back(connection);
            }
        }
    }
#elif defined(__APPLE__)
    // macOS: используем sysctlbyname для получения соединений
    
    // Получаем TCP соединения
    int mib[] = { CTL_NET, PF_INET, IPPROTO_TCP, TCPCTL_PCBLIST };
    size_t len;
    
    if (sysctl(mib, 4, NULL, &len, NULL, 0) < 0) {
        LogError("Failed to get TCP connection list size");
        return;
    }
    
    std::vector<char> buf(len);
    if (sysctl(mib, 4, buf.data(), &len, NULL, 0) < 0) {
        LogError("Failed to get TCP connection list");
        return;
    }
    
    // Обработка TCP соединений
    // Это сложная структура, требующая тщательного разбора
    // В данной реализации использована упрощенная логика
    
    // Получаем UDP соединения
    mib[2] = IPPROTO_UDP;
    
    if (sysctl(mib, 4, NULL, &len, NULL, 0) < 0) {
        LogError("Failed to get UDP connection list size");
        return;
    }
    
    buf.resize(len);
    if (sysctl(mib, 4, buf.data(), &len, NULL, 0) < 0) {
        LogError("Failed to get UDP connection list");
        return;
    }
    
    // Обработка UDP соединений
    // Упрощенная логика
#elif defined(__linux__)
    // Linux: читаем информацию из /proc/net/tcp и /proc/net/udp
    
    // Функция для парсинга файла соединений
    auto parseConnectionFile = [&](const std::string& file_path, const std::string& protocol) {
        std::ifstream file(file_path);
        if (!file) {
            LogError("Failed to open file: " + file_path);
            return;
        }
        
        std::string line;
        // Пропускаем заголовок
        std::getline(file, line);
        
        // Создаем отображение "inode -> pid" для определения процесса
        std::unordered_map<std::string, int> inode_to_pid;
        
        // Ищем через /proc/[pid]/fd директории
        DIR* proc_dir = opendir("/proc");
        if (proc_dir) {
            struct dirent* pid_entry;
            while ((pid_entry = readdir(proc_dir)) != nullptr) {
                // Проверяем, является ли запись числом (PID)
                if (pid_entry->d_type == DT_DIR) {
                    char* endptr;
                    long pid = strtol(pid_entry->d_name, &endptr, 10);
                    
                    if (*endptr == '\0') {
                        // Формируем путь к директории с файловыми дескрипторами
                        std::string fd_path = "/proc/" + std::string(pid_entry->d_name) + "/fd";
                        DIR* fd_dir = opendir(fd_path.c_str());
                        
                        if (fd_dir) {
                            struct dirent* fd_entry;
                            while ((fd_entry = readdir(fd_dir)) != nullptr) {
                                // Получаем путь к файлу дескриптора
                                std::string fd_link = fd_path + "/" + fd_entry->d_name;
                                
                                // Читаем символьную ссылку
                                char link_target[1024];
                                ssize_t len = readlink(fd_link.c_str(), link_target, sizeof(link_target) - 1);
                                
                                if (len != -1) {
                                    link_target[len] = '\0';
                                    std::string target(link_target);
                                    
                                    // Ищем "socket:[inode]"
                                    if (target.find("socket:[") == 0) {
                                        std::string inode = target.substr(8, target.length() - 9);
                                        inode_to_pid[inode] = static_cast<int>(pid);
                                    }
                                }
                            }
                            
                            closedir(fd_dir);
                        }
                    }
                }
            }
            
            closedir(proc_dir);
        }
        
        // Читаем соединения
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            
            std::string sl, local_addr, remote_addr, status, tx_queue_rx_queue, 
                      tr_tm_when, retrnsmt, uid_str, timeout, inode, others;
            
            iss >> sl >> local_addr >> remote_addr >> status >> tx_queue_rx_queue 
               >> tr_tm_when >> retrnsmt >> uid_str >> timeout >> inode >> others;
            
            NetworkConnectionInfo connection;
            connection.protocol = protocol;
            
            // Парсим локальный адрес
            size_t colon_pos = local_addr.find(':');
            if (colon_pos != std::string::npos) {
                std::string ip_hex = local_addr.substr(0, colon_pos);
                std::string port_hex = local_addr.substr(colon_pos + 1);
                
                // Преобразуем шестнадцатеричный IP-адрес
                unsigned int ip;
                std::stringstream ss_ip;
                ss_ip << std::hex << ip_hex;
                ss_ip >> ip;
                
                struct in_addr addr;
                addr.s_addr = ntohl(ip);
                connection.local_address = inet_ntoa(addr);
                
                // Преобразуем шестнадцатеричный порт
                unsigned int port;
                std::stringstream ss_port;
                ss_port << std::hex << port_hex;
                ss_port >> port;
                connection.local_port = port;
            }
            
            // Парсим удаленный адрес
            colon_pos = remote_addr.find(':');
            if (colon_pos != std::string::npos) {
                std::string ip_hex = remote_addr.substr(0, colon_pos);
                std::string port_hex = remote_addr.substr(colon_pos + 1);
                
                // Преобразуем шестнадцатеричный IP-адрес
                unsigned int ip;
                std::stringstream ss_ip;
                ss_ip << std::hex << ip_hex;
                ss_ip >> ip;
                
                struct in_addr addr;
                addr.s_addr = ntohl(ip);
                connection.remote_address = inet_ntoa(addr);
                
                // Преобразуем шестнадцатеричный порт
                unsigned int port;
                std::stringstream ss_port;
                ss_port << std::hex << port_hex;
                ss_port >> port;
                connection.remote_port = port;
            }
            
            // Парсим статус соединения
            unsigned int status_int;
            std::stringstream ss_status;
            ss_status << std::hex << status;
            ss_status >> status_int;
            
            // Определяем состояние
            switch (status_int) {
                case 1:  connection.state = "ESTABLISHED"; break;
                case 2:  connection.state = "SYN_SENT"; break;
                case 3:  connection.state = "SYN_RECV"; break;
                case 4:  connection.state = "FIN_WAIT1"; break;
                case 5:  connection.state = "FIN_WAIT2"; break;
                case 6:  connection.state = "TIME_WAIT"; break;
                case 7:  connection.state = "CLOSE"; break;
                case 8:  connection.state = "CLOSE_WAIT"; break;
                case 9:  connection.state = "LAST_ACK"; break;
                case 10: connection.state = "LISTEN"; break;
                case 11: connection.state = "CLOSING"; break;
                default: connection.state = "UNKNOWN"; break;
            }
            
            // Устанавливаем время установления соединения
            connection.established_time = std::chrono::system_clock::now();
            
            // Находим PID процесса по inode
            auto pid_it = inode_to_pid.find(inode);
            if (pid_it != inode_to_pid.end()) {
                connection.pid = pid_it->second;
                
                // Получаем имя процесса
                std::string proc_path = "/proc/" + std::to_string(connection.pid) + "/comm";
                std::ifstream proc_file(proc_path);
                if (proc_file) {
                    std::getline(proc_file, connection.process_name);
                }
            }
            
            active_connections_.push_back(connection);
            process_connections_[connection.pid].push_back(connection);
        }
    };
    
    // Получаем TCP соединения
    parseConnectionFile("/proc/net/tcp", "TCP");
    
    // Получаем TCP6 соединения
    parseConnectionFile("/proc/net/tcp6", "TCP6");
    
    // Получаем UDP соединения
    parseConnectionFile("/proc/net/udp", "UDP");
    
    // Получаем UDP6 соединения
    parseConnectionFile("/proc/net/udp6", "UDP6");
#endif
}

std::vector<std::string> NetworkMonitorImpl::checkSuspiciousIPs() {
    std::vector<std::string> suspicious;
    
    for (const auto& connection : active_connections_) {
        // Проверяем, входит ли IP-адрес в список подозрительных
        if (suspicious_ips_.find(connection.remote_address) != suspicious_ips_.end()) {
            std::stringstream ss;
            ss << "Connection to suspicious IP detected: " << connection.process_name 
               << " (PID: " << connection.pid << ") - " 
               << connection.protocol << " " 
               << connection.local_address << ":" << connection.local_port 
               << " -> " << connection.remote_address << ":" << connection.remote_port;
            
            suspicious.push_back(ss.str());
        }
    }
    
    return suspicious;
}

std::vector<std::string> NetworkMonitorImpl::detectUnusualPorts() {
    std::vector<std::string> unusual_ports;
    
    // Список диапазонов портов для проверки
    const std::vector<std::pair<int, int>> suspicious_port_ranges = {
        {0, 1023},           // Системные порты (требуют привилегий)
        {4444, 4444},        // Метасплойт
        {6666, 6667},        // IRC-боты
        {1080, 1080},        // SOCKS прокси
        {8080, 8080},        // Прокси/RAT
        {3389, 3389}         // RDP (легитимный, но может быть подозрительным)
    };
    
    for (const auto& connection : active_connections_) {
        // Проверяем, не является ли порт необычным
        
        // Сначала проверяем, входит ли порт в список легитимных
        if (legitimate_ports_.find(connection.remote_port) != legitimate_ports_.end()) {
            continue;
        }
        
        // Затем проверяем, входит ли порт в один из подозрительных диапазонов
        for (const auto& range : suspicious_port_ranges) {
            if (connection.remote_port >= range.first && connection.remote_port <= range.second) {
                std::stringstream ss;
                ss << "Connection to unusual port detected: " << connection.process_name 
                   << " (PID: " << connection.pid << ") - " 
                   << connection.protocol << " " 
                   << connection.local_address << ":" << connection.local_port 
                   << " -> " << connection.remote_address << ":" << connection.remote_port;
                
                unusual_ports.push_back(ss.str());
                break;
            }
        }
    }
    
    return unusual_ports;
}

std::vector<std::string> NetworkMonitorImpl::detectAbnormalTraffic() {
    std::vector<std::string> abnormal_traffic;
    
    // Для демонстрации: заглушка для обнаружения аномального трафика
    // В реальном приложении здесь может быть более сложная логика,
    // например, анализ объема передаваемых данных, частоты соединений,
    // времени активности и т.д.
    
    for (const auto& connection : active_connections_) {
        // Пример: проверяем подозрительные комбинации протокола и порта
        if (connection.protocol == "UDP" && connection.remote_port == 53 && 
            connection.remote_address != "8.8.8.8" && connection.remote_address != "8.8.4.4") {
            // Подозрительный DNS-запрос (не к Google DNS)
            std::stringstream ss;
            ss << "Suspicious DNS query detected: " << connection.process_name 
               << " (PID: " << connection.pid << ") - " 
               << connection.remote_address << ":" << connection.remote_port;
            
            abnormal_traffic.push_back(ss.str());
        }
        
        // Пример: проверяем необычные состояния соединений
        if (connection.protocol == "TCP" && 
            connection.state != "ESTABLISHED" && 
            connection.state != "LISTEN" && 
            connection.state != "TIME_WAIT") {
            
            // Необычное состояние TCP-соединения
            std::stringstream ss;
            ss << "Unusual TCP connection state detected: " << connection.process_name 
               << " (PID: " << connection.pid << ") - " 
               << connection.state << " " 
               << connection.local_address << ":" << connection.local_port 
               << " -> " << connection.remote_address << ":" << connection.remote_port;
            
            abnormal_traffic.push_back(ss.str());
        }
    }
    
    return abnormal_traffic;
}

} // namespace mceas