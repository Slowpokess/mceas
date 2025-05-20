#include "internal/behavior/process.h"
#include "internal/logging.h"
#include "internal/utils/platform_utils.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <array>
#include <memory>

#if defined(_WIN32)
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <libproc.h>
#elif defined(__linux__)
#include <dirent.h>
#include <unistd.h>
#endif

namespace mceas {

ProcessMonitorImpl::ProcessMonitorImpl() 
    : last_update_(std::chrono::system_clock::now() - std::chrono::hours(24)) {
    // Инициализируем список известных вредоносных процессов
    known_malicious_processes_.insert("malware.exe");
    known_malicious_processes_.insert("keylogger");
    known_malicious_processes_.insert("trojan");
    known_malicious_processes_.insert("backdoor");
    known_malicious_processes_.insert("spyware");
    known_malicious_processes_.insert("miner");
    
    // Выполняем первоначальное обновление информации о процессах
    updateProcessInfo();
}

std::vector<ProcessInfo> ProcessMonitorImpl::getRunningProcesses() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Обновляем информацию о процессах, если прошло достаточно времени
    auto now = std::chrono::system_clock::now();
    if (now - last_update_ > std::chrono::seconds(30)) {
        updateProcessInfo();
    }
    
    return running_processes_;
}

ProcessInfo ProcessMonitorImpl::getProcessInfo(int pid) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = process_map_.find(pid);
    if (it != process_map_.end()) {
        return it->second;
    }
    
    // Если процесс не найден, возвращаем пустую структуру
    return ProcessInfo{};
}

std::vector<ProcessInfo> ProcessMonitorImpl::getChildProcesses(int pid) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<ProcessInfo> children;
    
    auto it = process_tree_.find(pid);
    if (it != process_tree_.end()) {
        for (int child_pid : it->second) {
            auto proc_it = process_map_.find(child_pid);
            if (proc_it != process_map_.end()) {
                children.push_back(proc_it->second);
            }
        }
    }
    
    return children;
}

std::vector<ProcessInfo> ProcessMonitorImpl::getProcessHistory(
    std::chrono::system_clock::time_point from,
    std::chrono::system_clock::time_point to) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<ProcessInfo> filtered_history;
    
    for (const auto& process : process_history_) {
        if (process.start_time >= from && process.start_time <= to) {
            filtered_history.push_back(process);
        }
    }
    
    return filtered_history;
}

std::vector<std::string> ProcessMonitorImpl::checkForSuspiciousProcesses() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> alerts;
    
    // Проверяем наличие известных вредоносных процессов
    auto malicious = checkMaliciousProcesses();
    alerts.insert(alerts.end(), malicious.begin(), malicious.end());
    
    // Проверяем процессы с высоким потреблением ресурсов
    auto high_resource = detectHighResourceUsage();
    alerts.insert(alerts.end(), high_resource.begin(), high_resource.end());
    
    // Проверяем необычные шаблоны выполнения
    auto unusual_patterns = detectUnusualExecutionPatterns();
    alerts.insert(alerts.end(), unusual_patterns.begin(), unusual_patterns.end());
    
    return alerts;
}

void ProcessMonitorImpl::updateProcessInfo() {
    try {
        // Сохраняем текущие процессы в историю
        for (const auto& process : running_processes_) {
            process_history_.push_back(process);
        }
        
        // Ограничиваем размер истории
        const size_t max_history_size = 1000;
        if (process_history_.size() > max_history_size) {
            process_history_.erase(process_history_.begin(), 
                                 process_history_.begin() + (process_history_.size() - max_history_size));
        }
        
        // Очищаем список текущих процессов
        running_processes_.clear();
        process_map_.clear();
        process_tree_.clear();
        
        // Читаем информацию о процессах из системы
        readSystemProcesses();
        
        // Обновляем дерево процессов
        updateProcessTree();
        
        // Обновляем время последнего обновления
        last_update_ = std::chrono::system_clock::now();
        
        LogInfo("Process information updated. Running processes: " + std::to_string(running_processes_.size()));
    }
    catch (const std::exception& e) {
        LogError("Error updating process information: " + std::string(e.what()));
    }
}

void ProcessMonitorImpl::readSystemProcesses() {
#if defined(_WIN32)
    // Windows: используем ToolHelp API для получения списка процессов
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        LogError("Failed to create process snapshot");
        return;
    }
    
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);
    
    if (Process32First(snapshot, &entry)) {
        do {
            ProcessInfo process;
            process.pid = entry.th32ProcessID;
            process.ppid = entry.th32ParentProcessID;
            process.name = entry.szExeFile;
            
            // Получаем путь к исполняемому файлу
            HANDLE process_handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, process.pid);
            if (process_handle) {
                char path[MAX_PATH];
                if (GetModuleFileNameEx(process_handle, NULL, path, MAX_PATH)) {
                    process.path = path;
                }
                
                // Получаем время запуска процесса
                FILETIME creation_time, exit_time, kernel_time, user_time;
                if (GetProcessTimes(process_handle, &creation_time, &exit_time, &kernel_time, &user_time)) {
                    ULARGE_INTEGER ul;
                    ul.LowPart = creation_time.dwLowDateTime;
                    ul.HighPart = creation_time.dwHighDateTime;
                    
                    auto time_point = std::chrono::system_clock::from_time_t(
                        (ul.QuadPart - 116444736000000000ULL) / 10000000ULL);
                    
                    process.start_time = time_point;
                }
                
                // Получаем использование памяти
                PROCESS_MEMORY_COUNTERS pmc;
                if (GetProcessMemoryInfo(process_handle, &pmc, sizeof(pmc))) {
                    process.memory_usage = pmc.WorkingSetSize;
                }
                
                CloseHandle(process_handle);
            }
            
            // Получаем дополнительную информацию о процессе
            // ...
            
            running_processes_.push_back(process);
            process_map_[process.pid] = process;
            
        } while (Process32Next(snapshot, &entry));
    }
    
    CloseHandle(snapshot);
#elif defined(__APPLE__)
    // macOS: используем sysctl для получения списка процессов
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
    size_t size;
    
    if (sysctl(mib, 4, NULL, &size, NULL, 0) < 0) {
        LogError("Failed to get process list size");
        return;
    }
    
    struct kinfo_proc* processes = (struct kinfo_proc*)malloc(size);
    if (!processes) {
        LogError("Failed to allocate memory for process list");
        return;
    }
    
    if (sysctl(mib, 4, processes, &size, NULL, 0) < 0) {
        LogError("Failed to get process list");
        free(processes);
        return;
    }
    
    int process_count = size / sizeof(struct kinfo_proc);
    
    for (int i = 0; i < process_count; i++) {
        ProcessInfo process;
        process.pid = processes[i].kp_proc.p_pid;
        process.ppid = processes[i].kp_eproc.e_ppid;
        process.name = processes[i].kp_proc.p_comm;
        
        // Получаем путь к исполняемому файлу
        char path[PROC_PIDPATHINFO_MAXSIZE];
        if (proc_pidpath(process.pid, path, sizeof(path)) > 0) {
            process.path = path;
        }
        
        // Получаем имя пользователя
        process.user = std::to_string(processes[i].kp_eproc.e_ucred.cr_uid);
        
        // Получаем время запуска процесса
        process.start_time = std::chrono::system_clock::from_time_t(
            processes[i].kp_proc.p_starttime.tv_sec);
        
        // Получаем использование ресурсов
        struct proc_taskinfo task_info;
        if (proc_pidinfo(process.pid, PROC_PIDTASKINFO, 0, &task_info, sizeof(task_info)) > 0) {
            process.memory_usage = task_info.pti_resident_size;
            
            // Вычисляем использование CPU
            process.cpu_usage = (task_info.pti_total_user + task_info.pti_total_system) / 1000000000.0;
        }
        
        // Получаем командную строку
        int args_mib[3] = { CTL_KERN, KERN_PROCARGS2, process.pid };
        char args[4096];
        size_t args_size = sizeof(args);
        
        if (sysctl(args_mib, 3, args, &args_size, NULL, 0) == 0) {
            // Пропускаем размер аргументов (int) и имя программы
            int args_count = *((int*)args);
            char* args_ptr = args + sizeof(int);
            
            // Пропускаем имя программы
            args_ptr += strlen(args_ptr) + 1;
            
            // Собираем командную строку
            std::string cmd_line;
            while (args_ptr < args + args_size) {
                if (*args_ptr != '\0') {
                    if (!cmd_line.empty()) {
                        cmd_line += " ";
                    }
                    cmd_line += args_ptr;
                }
                args_ptr += strlen(args_ptr) + 1;
            }
            
            process.command_line = cmd_line;
        }
        
        // Получаем TTY
        process.tty = processes[i].kp_eproc.e_tdev;
        
        // Определяем, является ли процесс фоновым
        process.is_background = (processes[i].kp_proc.p_flag & P_BACKGROUND) != 0;
        
        running_processes_.push_back(process);
        process_map_[process.pid] = process;
    }
    
    free(processes);
#elif defined(__linux__)
    // Linux: читаем информацию из /proc
    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) {
        LogError("Failed to open /proc directory");
        return;
    }
    
    struct dirent* entry;
    while ((entry = readdir(proc_dir)) != nullptr) {
        // Проверяем, является ли запись числом (PID)
        if (entry->d_type == DT_DIR) {
            char* endptr;
            long pid = strtol(entry->d_name, &endptr, 10);
            
            if (*endptr == '\0') {
                ProcessInfo process;
                process.pid = static_cast<int>(pid);
                
                // Формируем путь к директории процесса
                std::string proc_path = "/proc/" + std::string(entry->d_name);
                
                // Читаем статус процесса
                std::ifstream status_file(proc_path + "/status");
                if (status_file) {
                    std::string line;
                    while (std::getline(status_file, line)) {
                        if (line.find("Name:") == 0) {
                            process.name = line.substr(5);
                            // Удаляем начальные пробелы
                            process.name.erase(0, process.name.find_first_not_of(" \t"));
                        }
                        else if (line.find("PPid:") == 0) {
                            process.ppid = std::stoi(line.substr(5));
                        }
                        else if (line.find("Uid:") == 0) {
                            std::stringstream ss(line.substr(5));
                            int uid;
                            ss >> uid;
                            process.user = std::to_string(uid);
                        }
                    }
                }
                
                // Читаем командную строку
                std::ifstream cmdline_file(proc_path + "/cmdline");
                if (cmdline_file) {
                    std::string cmdline;
                    std::getline(cmdline_file, cmdline, '\0');
                    process.command_line = cmdline;
                }
                
                // Получаем путь к исполняемому файлу
                try {
                    process.path = std::filesystem::read_symlink(proc_path + "/exe").string();
                }
                catch (...) {
                    // Игнорируем ошибки
                }
                
                // Получаем время запуска процесса
                std::ifstream stat_file(proc_path + "/stat");
                if (stat_file) {
                    std::string stat_line;
                    std::getline(stat_file, stat_line);
                    
                    // Парсим строку статистики
                    std::stringstream ss(stat_line);
                    std::string item;
                    
                    // Пропускаем pid и name
                    ss >> item >> item;
                    
                    // Пропускаем state
                    ss >> item;
                    
                    // Получаем ppid
                    ss >> process.ppid;
                    
                    // Пропускаем несколько полей
                    for (int i = 0; i < 17; i++) {
                        ss >> item;
                    }
                    
                    // Получаем время запуска (в тиках с момента загрузки системы)
                    unsigned long long start_time;
                    ss >> start_time;
                    
                    // Получаем время загрузки системы
                    std::ifstream uptime_file("/proc/uptime");
                    double uptime = 0;
                    if (uptime_file) {
                        uptime_file >> uptime;
                    }
                    
                    // Получаем текущее время
                    auto now = std::chrono::system_clock::now();
                    
                    // Вычисляем время запуска процесса
                    auto boot_time = now - std::chrono::seconds(static_cast<long>(uptime));
                    auto start_ticks = std::chrono::seconds(start_time / sysconf(_SC_CLK_TCK));
                    
                    process.start_time = boot_time + start_ticks;
                }
                
                // Получаем использование памяти
                std::ifstream statm_file(proc_path + "/statm");
                if (statm_file) {
                    unsigned long size, resident;
                    statm_file >> size >> resident;
                    
                    // Размер страницы памяти
                    long page_size = sysconf(_SC_PAGESIZE);
                    
                    // Вычисляем использование памяти в байтах
                    process.memory_usage = resident * page_size;
                }
                
                // Получаем использование CPU
                // В Linux это требует более сложных вычислений и отслеживания во времени
                process.cpu_usage = 0.0;
                
                // Получаем TTY
                process.tty = 0;
                
                // Определяем, является ли процесс фоновым
                process.is_background = false;
                
                running_processes_.push_back(process);
                process_map_[process.pid] = process;
            }
        }
    }
    
    closedir(proc_dir);
#endif
}

void ProcessMonitorImpl::updateProcessTree() {
    // Очищаем дерево процессов
    process_tree_.clear();
    
    // Строим дерево процессов
    for (const auto& process : running_processes_) {
        // Добавляем процесс в список дочерних процессов его родителя
        if (process.ppid > 0) {
            process_tree_[process.ppid].push_back(process.pid);
        }
    }
}

std::vector<std::string> ProcessMonitorImpl::checkMaliciousProcesses() {
    std::vector<std::string> malicious;
    
    for (const auto& process : running_processes_) {
        // Приводим имя процесса к нижнему регистру для сравнения
        std::string process_name_lower = process.name;
        std::transform(process_name_lower.begin(), process_name_lower.end(), 
                      process_name_lower.begin(), ::tolower);
        
        // Проверяем, содержит ли имя процесса известные вредоносные имена
        for (const auto& malicious_name : known_malicious_processes_) {
            if (process_name_lower.find(malicious_name) != std::string::npos) {
                std::stringstream ss;
                ss << "Potentially malicious process detected: " << process.name 
                   << " (PID: " << process.pid << ")";
                
                malicious.push_back(ss.str());
                break;
            }
        }
    }
    
    return malicious;
}

std::vector<std::string> ProcessMonitorImpl::detectHighResourceUsage() {
    std::vector<std::string> high_usage;
    
    // Пороговые значения для высокого потребления ресурсов
    const double CPU_THRESHOLD = 80.0;         // 80% использования CPU
    const uint64_t MEMORY_THRESHOLD = 1024 * 1024 * 1024;  // 1 ГБ памяти
    
    for (const auto& process : running_processes_) {
        // Проверяем использование CPU
        if (process.cpu_usage > CPU_THRESHOLD) {
            std::stringstream ss;
            ss << "High CPU usage detected: " << process.name 
               << " (PID: " << process.pid << ") - " 
               << process.cpu_usage << "%";
            
            high_usage.push_back(ss.str());
        }
        
        // Проверяем использование памяти
        if (process.memory_usage > MEMORY_THRESHOLD) {
            std::stringstream ss;
            ss << "High memory usage detected: " << process.name 
               << " (PID: " << process.pid << ") - " 
               << (process.memory_usage / (1024 * 1024)) << " MB";
            
            high_usage.push_back(ss.str());
        }
    }
    
    return high_usage;
}

std::vector<std::string> ProcessMonitorImpl::detectUnusualExecutionPatterns() {
    std::vector<std::string> unusual_patterns;
    
    // Проверяем необычные шаблоны выполнения
    
    // Пример: обнаружение процессов с подозрительными путями
    const std::vector<std::string> suspicious_paths = {
        "/tmp/",
        "/dev/shm/",
        "/var/tmp/",
        "C:\\Windows\\Temp\\",
        "C:\\Temp\\"
    };
    
    for (const auto& process : running_processes_) {
        for (const auto& path : suspicious_paths) {
            if (process.path.find(path) == 0) {
                std::stringstream ss;
                ss << "Process running from suspicious location: " << process.name 
                   << " (PID: " << process.pid << ") - " << process.path;
                
                unusual_patterns.push_back(ss.str());
                break;
            }
        }
    }
    
    // Пример: обнаружение необычных командных строк
    const std::vector<std::string> suspicious_commands = {
        "powershell -e",  // Encoded PowerShell
        "cmd.exe /c",     // Command prompt
        "bash -c",        // Bash command
        "nc -e",          // Netcat execution
        "wget",           // File download
        "curl"            // File download
    };
    
    for (const auto& process : running_processes_) {
        for (const auto& cmd : suspicious_commands) {
            if (process.command_line.find(cmd) != std::string::npos) {
                std::stringstream ss;
                ss << "Suspicious command line detected: " << process.name 
                   << " (PID: " << process.pid << ") - " << process.command_line;
                
                unusual_patterns.push_back(ss.str());
                break;
            }
        }
    }
    
    return unusual_patterns;
}

} // namespace mceas