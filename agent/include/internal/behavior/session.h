#ifndef MCEAS_SESSION_ANALYZER_H
#define MCEAS_SESSION_ANALYZER_H

#include "internal/behavior_module.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <chrono>

namespace mceas {

// Реализация анализатора сессий
class SessionAnalyzerImpl : public SessionAnalyzer {
public:
    SessionAnalyzerImpl();
    ~SessionAnalyzerImpl() override = default;
    
    // Реализация методов интерфейса SessionAnalyzer
    std::vector<SessionInfo> getActiveSessions() override;
    
    std::vector<SessionInfo> getSessionHistory(
        std::chrono::system_clock::time_point from,
        std::chrono::system_clock::time_point to) override;
    
    SessionInfo getSessionInfo(const std::string& session_id) override;
    
    std::vector<std::string> checkForSuspiciousActivity() override;
    
private:
    // Обновление информации о сессиях
    void updateSessionInfo();
    
    // Чтение информации о сессиях из системных логов
    void readSystemLogs();
    
    // Обнаружение удаленных сессий
    void detectRemoteSessions();
    
    // Определение аномалий во времени сессий
    std::vector<std::string> detectTimeAnomalies();
    
    // Поиск подозрительных источников входа
    std::vector<std::string> detectSuspiciousLoginSources();
    
    // Кэш активных сессий
    std::vector<SessionInfo> active_sessions_;
    
    // История сессий
    std::vector<SessionInfo> session_history_;
    
    // Отображение для быстрого поиска сессии по ID
    std::map<std::string, SessionInfo> session_map_;
    
    // Время последнего обновления
    std::chrono::system_clock::time_point last_update_;
    
    // Типичные времена входа для каждого пользователя
    std::map<std::string, std::vector<std::chrono::system_clock::time_point>> user_login_patterns_;
    
    // Мьютекс для потокобезопасности
    mutable std::mutex mutex_;
};

} // namespace mceas

#endif // MCEAS_SESSION_ANALYZER_H