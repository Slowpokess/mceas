#include "internal/behavior_module.h"
#include "internal/behavior/session.h"
#include "internal/behavior/process.h"
#include "internal/behavior/network.h"
#include "internal/logging.h"
#include "internal/utils/platform_utils.h"
#include <chrono>
#include <thread>
#include <sstream>
#include "external/json/json.hpp"

namespace mceas {

BehaviorModule::BehaviorModule() 
    : ModuleBase("BehaviorModule", ModuleType::BEHAVIOR),
      monitoring_active_(false),
      monitoring_interval_(60) {
}

BehaviorModule::~BehaviorModule() {
    if (running_) {
        stop();
    }
}

bool BehaviorModule::onInitialize() {
    LogInfo("Initializing Behavior Module");
    
    // Инициализация компонентов
    initComponents();
    
    return true;
}

bool BehaviorModule::onStart() {
    LogInfo("Starting Behavior Module");
    
    // Запускаем периодический мониторинг
    startMonitoring();
    
    return true;
}

bool BehaviorModule::onStop() {
    LogInfo("Stopping Behavior Module");
    
    // Останавливаем периодический мониторинг
    stopMonitoring();
    
    return true;
}

ModuleResult BehaviorModule::onExecute() {
    LogInfo("Executing Behavior Module");
    
    ModuleResult result;
    result.module_name = getName();
    result.type = getType();
    result.success = true;
    
    try {
        std::lock_guard<std::mutex> lock(data_mutex_);
        
        // Обновляем данные о сессиях, процессах и сетевых соединениях
        active_sessions_ = session_analyzer_->getActiveSessions();
        running_processes_ = process_monitor_->getRunningProcesses();
        active_connections_ = network_monitor_->getActiveConnections();
        
        // Обновляем время последнего обновления
        last_update_time_ = std::chrono::system_clock::now();
        
        // Проверяем наличие подозрительной активности
        std::vector<std::string> suspicious_sessions = session_analyzer_->checkForSuspiciousActivity();
        std::vector<std::string> suspicious_processes = process_monitor_->checkForSuspiciousProcesses();
        std::vector<std::string> suspicious_connections = network_monitor_->checkForSuspiciousConnections();
        
        // Записываем подозрительную активность в журнал событий
        for (const auto& alert : suspicious_sessions) {
            LogWarning("Suspicious session activity: " + alert);
            event_log_.push_back("SESSION_ALERT: " + alert);
        }
        
        for (const auto& alert : suspicious_processes) {
            LogWarning("Suspicious process activity: " + alert);
            event_log_.push_back("PROCESS_ALERT: " + alert);
        }
        
        for (const auto& alert : suspicious_connections) {
            LogWarning("Suspicious network activity: " + alert);
            event_log_.push_back("NETWORK_ALERT: " + alert);
        }
        
        // Добавляем результаты в выходные данные
        addResultData("behavior.sessions.count", std::to_string(active_sessions_.size()));
        addResultData("behavior.processes.count", std::to_string(running_processes_.size()));
        addResultData("behavior.connections.count", std::to_string(active_connections_.size()));
        addResultData("behavior.alerts.count", std::to_string(
            suspicious_sessions.size() + suspicious_processes.size() + suspicious_connections.size()));
        
        // Добавляем время последнего обновления
        auto time_t_now = std::chrono::system_clock::to_time_t(last_update_time_);
        addResultData("behavior.last_update", std::to_string(time_t_now));
        
        // Создаем JSON с информацией о сессиях
        if (!active_sessions_.empty()) {
            nlohmann::json sessions_json = nlohmann::json::array();
            
            for (const auto& session : active_sessions_) {
                nlohmann::json session_json;
                session_json["user_id"] = session.user_id;
                session_json["user_name"] = session.user_name;
                session_json["terminal"] = session.terminal;
                session_json["login_source"] = session.login_source;
                
                // Преобразуем временные метки в читаемый формат
                auto login_time_t = std::chrono::system_clock::to_time_t(session.login_time);
                auto last_activity_t = std::chrono::system_clock::to_time_t(session.last_activity);
                
                session_json["login_time"] = std::to_string(login_time_t);
                session_json["last_activity"] = std::to_string(last_activity_t);
                
                session_json["is_remote"] = session.is_remote;
                session_json["is_active"] = session.is_active;
                session_json["remote_host"] = session.remote_host;
                
                sessions_json.push_back(session_json);
            }
            
            addResultData("sessions_json", sessions_json.dump());
        }
        
        // Создаем JSON с информацией о процессах (ограничиваем количество)
        if (!running_processes_.empty()) {
            nlohmann::json processes_json = nlohmann::json::array();
            size_t count = 0;
            size_t max_processes = 50; // Ограничиваем количество процессов в JSON
            
            for (const auto& process : running_processes_) {
                if (count >= max_processes) break;
                
                nlohmann::json process_json;
                process_json["pid"] = process.pid;
                process_json["ppid"] = process.ppid;
                process_json["name"] = process.name;
                process_json["path"] = process.path;
                process_json["user"] = process.user;
                
                // Преобразуем временную метку в читаемый формат
                auto start_time_t = std::chrono::system_clock::to_time_t(process.start_time);
                process_json["start_time"] = std::to_string(start_time_t);
                
                process_json["cpu_usage"] = process.cpu_usage;
                process_json["memory_usage"] = process.memory_usage;
                
                processes_json.push_back(process_json);
                count++;
            }
            
            addResultData("processes_json", processes_json.dump());
        }
        
        // Создаем JSON с информацией о сетевых соединениях (ограничиваем количество)
        if (!active_connections_.empty()) {
            nlohmann::json connections_json = nlohmann::json::array();
            size_t count = 0;
            size_t max_connections = 50; // Ограничиваем количество соединений в JSON
            
            for (const auto& connection : active_connections_) {
                if (count >= max_connections) break;
                
                nlohmann::json connection_json;
                connection_json["pid"] = connection.pid;
                connection_json["process_name"] = connection.process_name;
                connection_json["protocol"] = connection.protocol;
                connection_json["local_address"] = connection.local_address;
                connection_json["local_port"] = connection.local_port;
                connection_json["remote_address"] = connection.remote_address;
                connection_json["remote_port"] = connection.remote_port;
                connection_json["state"] = connection.state;
                
                connections_json.push_back(connection_json);
                count++;
            }
            
            addResultData("connections_json", connections_json.dump());
        }
        
        // Создаем JSON с оповещениями
        if (!event_log_.empty()) {
            nlohmann::json alerts_json = nlohmann::json::array();
            
            // Добавляем последние 100 оповещений
            size_t start_idx = event_log_.size() > 100 ? event_log_.size() - 100 : 0;
            for (size_t i = start_idx; i < event_log_.size(); i++) {
                alerts_json.push_back(event_log_[i]);
            }
            
            addResultData("alerts_json", alerts_json.dump());
        }
        
        return result;
    }
    catch (const std::exception& e) {
        LogError("Error executing behavior module: " + std::string(e.what()));
        result.success = false;
        result.error_message = "Error executing behavior module: " + std::string(e.what());
        return result;
    }
}

bool BehaviorModule::onHandleCommand(const Command& command) {
    LogInfo("Handling command in Behavior Module: " + command.action);
    
    // Обрабатываем команды
    if (command.action == "get_active_sessions") {
        try {
            // Получаем информацию о активных сессиях
            auto sessions = session_analyzer_->getActiveSessions();
            
            LogInfo("Retrieved " + std::to_string(sessions.size()) + " active sessions");
            return true;
        }
        catch (const std::exception& e) {
            LogError("Error retrieving active sessions: " + std::string(e.what()));
            return false;
        }
    }
    else if (command.action == "get_running_processes") {
        auto it = command.parameters.find("user");
        std::string user_filter;
        
        if (it != command.parameters.end()) {
            user_filter = it->second;
        }
        
        try {
            // Получаем информацию о запущенных процессах
            auto processes = process_monitor_->getRunningProcesses();
            
            // Фильтруем по пользователю, если указан
            if (!user_filter.empty()) {
                size_t original_count = processes.size();
                std::vector<ProcessInfo> filtered_processes;
                
                for (const auto& process : processes) {
                    if (process.user == user_filter) {
                        filtered_processes.push_back(process);
                    }
                }
                
                LogInfo("Retrieved " + std::to_string(filtered_processes.size()) + 
                         " processes (filtered from " + std::to_string(original_count) + 
                         ") for user: " + user_filter);
            }
            else {
                LogInfo("Retrieved " + std::to_string(processes.size()) + " running processes");
            }
            
            return true;
        }
        catch (const std::exception& e) {
            LogError("Error retrieving running processes: " + std::string(e.what()));
            return false;
        }
    }
    else if (command.action == "get_network_connections") {
        auto it = command.parameters.find("pid");
        int pid_filter = -1;
        
        if (it != command.parameters.end()) {
            try {
                pid_filter = std::stoi(it->second);
            }
            catch (...) {
                // Игнорируем ошибки преобразования
            }
        }
        
        try {
            // Получаем информацию о сетевых соединениях
            std::vector<NetworkConnectionInfo> connections;
            
            if (pid_filter > 0) {
                connections = network_monitor_->getProcessConnections(pid_filter);
                LogInfo("Retrieved " + std::to_string(connections.size()) + 
                         " network connections for PID: " + std::to_string(pid_filter));
            }
            else {
                connections = network_monitor_->getActiveConnections();
                LogInfo("Retrieved " + std::to_string(connections.size()) + " active network connections");
            }
            
            return true;
        }
        catch (const std::exception& e) {
            LogError("Error retrieving network connections: " + std::string(e.what()));
            return false;
        }
    }
    else if (command.action == "check_suspicious_activity") {
        try {
            // Проверяем наличие подозрительной активности
            auto session_alerts = session_analyzer_->checkForSuspiciousActivity();
            auto process_alerts = process_monitor_->checkForSuspiciousProcesses();
            auto network_alerts = network_monitor_->checkForSuspiciousConnections();
            
            // Объединяем все оповещения
            std::vector<std::string> all_alerts;
            all_alerts.insert(all_alerts.end(), session_alerts.begin(), session_alerts.end());
            all_alerts.insert(all_alerts.end(), process_alerts.begin(), process_alerts.end());
            all_alerts.insert(all_alerts.end(), network_alerts.begin(), network_alerts.end());
            
            LogInfo("Found " + std::to_string(all_alerts.size()) + " suspicious activities");
            
            // Добавляем оповещения в журнал событий
            for (const auto& alert : all_alerts) {
                event_log_.push_back(alert);
            }
            
            return true;
        }
        catch (const std::exception& e) {
            LogError("Error checking suspicious activity: " + std::string(e.what()));
            return false;
        }
    }
    
    LogWarning("Unknown command action: " + command.action);
    return false;
}

void BehaviorModule::initComponents() {
    // Создаем экземпляры компонентов
    session_analyzer_ = std::make_unique<SessionAnalyzerImpl>();
    process_monitor_ = std::make_unique<ProcessMonitorImpl>();
    network_monitor_ = std::make_unique<NetworkMonitorImpl>();
    
    LogInfo("Behavior Module components initialized");
}

void BehaviorModule::startMonitoring() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (!monitoring_active_) {
        monitoring_active_ = true;
        
        // Запускаем поток мониторинга
        monitoring_thread_ = std::make_unique<std::thread>(&BehaviorModule::monitoringThread, this);
        
        LogInfo("Behavior monitoring started with interval: " + std::to_string(monitoring_interval_) + " seconds");
    }
    else {
        LogWarning("Behavior monitoring is already active");
    }
}

void BehaviorModule::stopMonitoring() {
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        
        if (monitoring_active_) {
            monitoring_active_ = false;
        }
        else {
            LogWarning("Behavior monitoring is not active");
            return;
        }
    }
    
    // Дожидаемся завершения потока мониторинга
    if (monitoring_thread_ && monitoring_thread_->joinable()) {
        monitoring_thread_->join();
    }
    
    LogInfo("Behavior monitoring stopped");
}

void BehaviorModule::monitoringThread() {
    LogInfo("Monitoring thread started");
    
    while (monitoring_active_) {
        try {
            // Обновляем данные о сессиях, процессах и сетевых соединениях
            auto sessions = session_analyzer_->getActiveSessions();
            auto processes = process_monitor_->getRunningProcesses();
            auto connections = network_monitor_->getActiveConnections();
            
            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                
                // Обновляем кэшированные данные
                active_sessions_ = sessions;
                running_processes_ = processes;
                active_connections_ = connections;
                
                // Обновляем время последнего обновления
                last_update_time_ = std::chrono::system_clock::now();
            }
            
            // Проверяем наличие подозрительной активности
            std::vector<std::string> suspicious_sessions = session_analyzer_->checkForSuspiciousActivity();
            std::vector<std::string> suspicious_processes = process_monitor_->checkForSuspiciousProcesses();
            std::vector<std::string> suspicious_connections = network_monitor_->checkForSuspiciousConnections();
            
            // Записываем подозрительную активность в журнал событий
            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                
                for (const auto& alert : suspicious_sessions) {
                    LogWarning("Suspicious session activity: " + alert);
                    event_log_.push_back("SESSION_ALERT: " + alert);
                }
                
                for (const auto& alert : suspicious_processes) {
                    LogWarning("Suspicious process activity: " + alert);
                    event_log_.push_back("PROCESS_ALERT: " + alert);
                }
                
                for (const auto& alert : suspicious_connections) {
                    LogWarning("Suspicious network activity: " + alert);
                    event_log_.push_back("NETWORK_ALERT: " + alert);
                }
            }
            
            // Анализируем поведенческие шаблоны
            analyzeBehaviorPatterns();
            
            // Сохраняем историю
            saveHistory();
        }
        catch (const std::exception& e) {
            LogError("Error in monitoring thread: " + std::string(e.what()));
        }
        
        // Ждем указанный интервал перед следующим обновлением
        std::this_thread::sleep_for(std::chrono::seconds(monitoring_interval_));
    }
    
    LogInfo("Monitoring thread stopped");
}

void BehaviorModule::analyzeBehaviorPatterns() {
    // Анализ поведенческих шаблонов
    // В текущей версии используем заглушку
    LogDebug("Behavior pattern analysis not fully implemented yet");
}

void BehaviorModule::saveHistory() {
    // Сохранение истории для последующего анализа
    // В текущей версии используем заглушку
    LogDebug("History saving not fully implemented yet");
}

} // namespace mceas