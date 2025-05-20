#include "internal/behavior/session.h"
#include "internal/logging.h"
#include "internal/utils/platform_utils.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <cstring>
#include <array>
#include <memory>

#if defined(_WIN32)
#include <windows.h>
#include <wtsapi32.h>
#pragma comment(lib, "wtsapi32.lib")
#elif defined(__APPLE__) || defined(__linux__)
#include <unistd.h>
#include <utmp.h>
#include <pwd.h>
#endif

namespace mceas {

SessionAnalyzerImpl::SessionAnalyzerImpl() 
    : last_update_(std::chrono::system_clock::now() - std::chrono::hours(24)) {
    // Выполняем первоначальное обновление информации о сессиях
    updateSessionInfo();
}

std::vector<SessionInfo> SessionAnalyzerImpl::getActiveSessions() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Обновляем информацию о сессиях, если прошло достаточно времени
    auto now = std::chrono::system_clock::now();
    if (now - last_update_ > std::chrono::seconds(60)) {
        updateSessionInfo();
    }
    
    return active_sessions_;
}

std::vector<SessionInfo> SessionAnalyzerImpl::getSessionHistory(
    std::chrono::system_clock::time_point from,
    std::chrono::system_clock::time_point to) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<SessionInfo> filtered_history;
    
    for (const auto& session : session_history_) {
        if (session.login_time >= from && session.login_time <= to) {
            filtered_history.push_back(session);
        }
    }
    
    return filtered_history;
}

SessionInfo SessionAnalyzerImpl::getSessionInfo(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = session_map_.find(session_id);
    if (it != session_map_.end()) {
        return it->second;
    }
    
    // Если сессия не найдена, возвращаем пустую структуру
    return SessionInfo{};
}

std::vector<std::string> SessionAnalyzerImpl::checkForSuspiciousActivity() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> alerts;
    
    // Проверяем аномалии времени сессий
    auto time_anomalies = detectTimeAnomalies();
    alerts.insert(alerts.end(), time_anomalies.begin(), time_anomalies.end());
    
    // Проверяем подозрительные источники входа
    auto suspicious_sources = detectSuspiciousLoginSources();
    alerts.insert(alerts.end(), suspicious_sources.begin(), suspicious_sources.end());
    
    return alerts;
}

void SessionAnalyzerImpl::updateSessionInfo() {
    try {
        // Очищаем список активных сессий
        active_sessions_.clear();
        
        // Читаем информацию о сессиях из системных логов
        readSystemLogs();
        
        // Обнаруживаем удаленные сессии
        detectRemoteSessions();
        
        // Обновляем отображение сессий
        session_map_.clear();
        for (const auto& session : active_sessions_) {
            // Используем комбинацию имени пользователя и терминала как идентификатор сессии
            std::string session_id = session.user_name + "@" + session.terminal;
            session_map_[session_id] = session;
        }
        
        // Добавляем активные сессии в историю
        for (const auto& session : active_sessions_) {
            session_history_.push_back(session);
        }
        
        // Ограничиваем размер истории
        const size_t max_history_size = 1000;
        if (session_history_.size() > max_history_size) {
            session_history_.erase(session_history_.begin(), 
                                 session_history_.begin() + (session_history_.size() - max_history_size));
        }
        
        // Обновляем время последнего обновления
        last_update_ = std::chrono::system_clock::now();
        
        LogInfo("Session information updated. Active sessions: " + std::to_string(active_sessions_.size()));
    }
    catch (const std::exception& e) {
        LogError("Error updating session information: " + std::string(e.what()));
    }
}

void SessionAnalyzerImpl::readSystemLogs() {
#if defined(_WIN32)
    // Windows: используем WTS API для получения сессий
    WTS_SESSION_INFO* sessions = nullptr;
    DWORD count = 0;
    
    if (WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &sessions, &count)) {
        for (DWORD i = 0; i < count; i++) {
            WTS_SESSION_INFO& session = sessions[i];
            
            // Пропускаем системные сессии
            if (session.SessionId == 0) {
                continue;
            }
            
            LPTSTR user_name = nullptr;
            DWORD user_name_size = 0;
            
            if (WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, session.SessionId, 
                                         WTSUserName, &user_name, &user_name_size)) {
                
                SessionInfo session_info;
                session_info.user_name = user_name;
                session_info.terminal = "RDP-" + std::to_string(session.SessionId);
                session_info.is_active = (session.State == WTSActive);
                
                // Получаем дополнительную информацию о сессии
                // ...
                
                active_sessions_.push_back(session_info);
                
                WTSFreeMemory(user_name);
            }
        }
        
        WTSFreeMemory(sessions);
    }
#elif defined(__APPLE__) || defined(__linux__)
    // Unix-like: используем utmp для получения сессий
    struct utmp* entry;
    
    setutent();
    
    while ((entry = getutent()) != nullptr) {
        if (entry->ut_type == USER_PROCESS) {
            SessionInfo session_info;
            
            // Копируем имя пользователя и терминал
            char user_name[UT_NAMESIZE + 1] = {0};
            char terminal[UT_LINESIZE + 1] = {0};
            
            strncpy(user_name, entry->ut_user, UT_NAMESIZE);
            strncpy(terminal, entry->ut_line, UT_LINESIZE);
            
            session_info.user_name = user_name;
            session_info.terminal = terminal;
            
            // Получаем ID пользователя
            struct passwd* pw = getpwnam(session_info.user_name.c_str());
            if (pw != nullptr) {
                session_info.user_id = std::to_string(pw->pw_uid);
            }
            
            // Устанавливаем хост входа
            char host[UT_HOSTSIZE + 1] = {0};
            strncpy(host, entry->ut_host, UT_HOSTSIZE);
            session_info.login_source = host;
            
            // Устанавливаем время входа
            session_info.login_time = std::chrono::system_clock::from_time_t(entry->ut_time);
            
            // Устанавливаем время последней активности (в данном случае то же, что и время входа)
            session_info.last_activity = session_info.login_time;
            
            // Определяем, является ли сессия удаленной
            session_info.is_remote = !session_info.login_source.empty() && 
                                  session_info.login_source != "localhost" && 
                                  session_info.login_source != "127.0.0.1" && 
                                  session_info.login_source != "::1";
            
            // Устанавливаем удаленный хост
            if (session_info.is_remote) {
                session_info.remote_host = session_info.login_source;
            }
            
            // Предполагаем, что сессия активна
            session_info.is_active = true;
            
            active_sessions_.push_back(session_info);
        }
    }
    
    endutent();
#endif
}

void SessionAnalyzerImpl::detectRemoteSessions() {
    // Обнаружение удаленных сессий на основе типа терминала или других признаков
    for (auto& session : active_sessions_) {
        // В Unix-like системах удаленные сессии обычно имеют префикс pts/
        if (session.terminal.find("pts/") == 0) {
            session.is_remote = true;
            
            // Если удаленный хост еще не установлен, пытаемся его определить
            if (session.remote_host.empty() && !session.login_source.empty()) {
                session.remote_host = session.login_source;
            }
        }
        
        // В Windows удаленные сессии обычно имеют префикс RDP-
        if (session.terminal.find("RDP-") == 0) {
            session.is_remote = true;
        }
    }
}

std::vector<std::string> SessionAnalyzerImpl::detectTimeAnomalies() {
    std::vector<std::string> anomalies;
    
    // Получаем текущее время
    auto now = std::chrono::system_clock::now();
    std::time_t now_t = std::chrono::system_clock::to_time_t(now);
    std::tm* now_tm = std::localtime(&now_t);
    
    // Проверяем время активных сессий
    for (const auto& session : active_sessions_) {
        // Получаем время входа в сессию
        std::time_t login_t = std::chrono::system_clock::to_time_t(session.login_time);
        std::tm* login_tm = std::localtime(&login_t);
        
        // Проверяем, входил ли пользователь ночью (между 0:00 и 5:00)
        if (login_tm->tm_hour >= 0 && login_tm->tm_hour < 5) {
            std::stringstream ss;
            ss << "Unusual login time for user " << session.user_name 
               << " at " << login_tm->tm_hour << ":" << login_tm->tm_min 
               << " on terminal " << session.terminal;
            
            anomalies.push_back(ss.str());
        }
        
        // Проверяем, входил ли пользователь в выходные дни
        if (login_tm->tm_wday == 0 || login_tm->tm_wday == 6) {
            std::stringstream ss;
            ss << "Weekend login for user " << session.user_name 
               << " on " << (login_tm->tm_wday == 0 ? "Sunday" : "Saturday")
               << " terminal " << session.terminal;
            
            anomalies.push_back(ss.str());
        }
        
        // Проверяем аномалии на основе шаблонов входа пользователя
        auto it = user_login_patterns_.find(session.user_name);
        if (it != user_login_patterns_.end() && !it->second.empty()) {
            // Если у нас есть история входов для этого пользователя,
            // проверяем, соответствует ли текущий вход типичному шаблону
            
            // Вычисляем среднее время входа
            int total_hours = 0;
            for (const auto& time : it->second) {
                std::time_t t = std::chrono::system_clock::to_time_t(time);
                std::tm* tm = std::localtime(&t);
                total_hours += tm->tm_hour;
            }
            
            int avg_hour = total_hours / it->second.size();
            
            // Если текущее время входа отличается от среднего более чем на 3 часа,
            // считаем это аномалией
            if (std::abs(login_tm->tm_hour - avg_hour) > 3) {
                std::stringstream ss;
                ss << "Unusual login time pattern for user " << session.user_name 
                   << ". Typical login hour: " << avg_hour 
                   << ", Current login hour: " << login_tm->tm_hour;
                
                anomalies.push_back(ss.str());
            }
        }
    }
    
    // Для каждого пользователя обновляем шаблоны входа
    for (const auto& session : active_sessions_) {
        user_login_patterns_[session.user_name].push_back(session.login_time);
        
        // Ограничиваем историю шаблонов
        const size_t max_pattern_size = 30;
        if (user_login_patterns_[session.user_name].size() > max_pattern_size) {
            user_login_patterns_[session.user_name].erase(
                user_login_patterns_[session.user_name].begin());
        }
    }
    
    return anomalies;
}

std::vector<std::string> SessionAnalyzerImpl::detectSuspiciousLoginSources() {
    std::vector<std::string> suspicious_sources;
    
    // Список известных безопасных источников входа
    std::set<std::string> safe_sources = {
        "localhost",
        "127.0.0.1",
        "::1",
        // Можно добавить другие безопасные источники
    };
    
    for (const auto& session : active_sessions_) {
        // Проверяем источник входа
        if (!session.login_source.empty() && 
            safe_sources.find(session.login_source) == safe_sources.end()) {
            
            // Если источник не входит в список безопасных, проверяем его
            
            // В реальном приложении здесь может быть более сложная логика проверки,
            // например, сравнение с белым списком, проверка по геолокации и т.д.
            
            // Для демонстрации просто фиксируем все удаленные входы
            if (session.is_remote) {
                std::stringstream ss;
                ss << "Remote login for user " << session.user_name 
                   << " from " << session.login_source 
                   << " on terminal " << session.terminal;
                
                suspicious_sources.push_back(ss.str());
            }
        }
    }
    
    return suspicious_sources;
}

} // namespace mceas