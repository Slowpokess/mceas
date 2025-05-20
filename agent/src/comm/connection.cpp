#include "internal/comm/connection.h"
#include "internal/logging.h"
#include <thread>

namespace mceas {
namespace comm {

Connection::Connection(const ConnectionParams& params)
    : state_(ConnectionState::DISCONNECTED), params_(params) {
}

Connection::~Connection() {
    if (state_ != ConnectionState::DISCONNECTED) {
        disconnect();
    }
}

bool Connection::connect() {
    // Уже подключены
    if (state_ == ConnectionState::CONNECTED) {
        return true;
    }
    
    LogInfo("Connecting to " + params_.endpoint + ":" + std::to_string(params_.port));
    
    // Устанавливаем состояние "подключение"
    state_ = ConnectionState::CONNECTING;
    if (state_handler_) {
        state_handler_(state_);
    }
    
    // Здесь будет реальная логика подключения, которая зависит от конкретного протокола
    // Для примера просто переводим состояние в CONNECTED
    state_ = ConnectionState::CONNECTED;
    if (state_handler_) {
        state_handler_(state_);
    }
    
    updateLastActivity();
    return true;
}

bool Connection::disconnect() {
    // Уже отключены
    if (state_ == ConnectionState::DISCONNECTED) {
        return true;
    }
    
    LogInfo("Disconnecting from " + params_.endpoint);
    
    // Здесь будет реальная логика отключения
    
    // Обновляем состояние
    state_ = ConnectionState::DISCONNECTED;
    if (state_handler_) {
        state_handler_(state_);
    }
    
    return true;
}

ConnectionState Connection::getState() const {
    return state_;
}

ConnectionStats Connection::getStats() const {
    return stats_;
}

void Connection::updateLastActivity() {
    stats_.last_activity = std::chrono::system_clock::now();
}

void Connection::setStateChangeHandler(std::function<void(ConnectionState)> handler) {
    state_handler_ = handler;
}

void Connection::setParams(const ConnectionParams& params) {
    // Если изменились критические параметры, переподключаемся
    bool need_reconnect = (state_ == ConnectionState::CONNECTED) && 
                         (params_.endpoint != params.endpoint || params_.port != params.port);
    
    params_ = params;
    
    if (need_reconnect) {
        reconnect();
    }
}

ConnectionParams Connection::getParams() const {
    return params_;
}

void Connection::resetStats() {
    stats_ = ConnectionStats();
}

bool Connection::needsReconnect() const {
    // Если соединение не активно или в ошибке, и прошло достаточно времени 
    // с последней активности, нужно переподключение
    if (state_ != ConnectionState::CONNECTED && state_ != ConnectionState::CONNECTING) {
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - stats_.last_activity).count();
            
        return params_.auto_reconnect && elapsed > params_.reconnect_interval_ms;
    }
    
    return false;
}

bool Connection::reconnect() {
    if (state_ == ConnectionState::CONNECTED) {
        disconnect();
    }
    
    state_ = ConnectionState::RECONNECTING;
    if (state_handler_) {
        state_handler_(state_);
    }
    
    LogInfo("Attempting to reconnect to " + params_.endpoint);
    
    // Счетчик попыток
    stats_.reconnect_attempts++;
    
    // Попытки переподключения
    for (int attempt = 0; attempt < params_.reconnect_attempts; ++attempt) {
        if (connect()) {
            LogInfo("Reconnected successfully to " + params_.endpoint);
            return true;
        }
        
        LogWarning("Reconnection attempt " + std::to_string(attempt + 1) + 
                 " of " + std::to_string(params_.reconnect_attempts) + " failed");
                 
        // Пауза перед следующей попыткой
        std::this_thread::sleep_for(std::chrono::milliseconds(params_.reconnect_interval_ms));
    }
    
    // Если все попытки неудачны
    state_ = ConnectionState::FAILED;
    if (state_handler_) {
        state_handler_(state_);
    }
    
    LogError("All reconnection attempts to " + params_.endpoint + " failed");
    return false;
}

} // namespace comm
} // namespace mceas