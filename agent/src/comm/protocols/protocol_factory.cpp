#include "internal/comm/protocols/protocol_base.h"
#include "internal/logging.h"
#include <unordered_map>
#include <algorithm>
#include <random>

namespace mceas {
namespace comm {

// Получение реестра протоколов
std::map<std::string, ProtocolCreator>& ProtocolFactory::getProtocolRegistry() {
    static std::map<std::string, ProtocolCreator> registry;
    return registry;
}

// Регистрация протокола
bool ProtocolFactory::registerProtocol(const std::string& name, ProtocolCreator creator) {
    auto& registry = getProtocolRegistry();
    registry[name] = creator;
    LogInfo("Protocol registered: " + name);
    return true;
}

// Создание протокола по имени
std::unique_ptr<IProtocol> ProtocolFactory::createProtocol(const std::string& protocol_name) {
    auto& registry = getProtocolRegistry();
    auto it = registry.find(protocol_name);
    
    if (it == registry.end()) {
        LogError("Unknown protocol: " + protocol_name);
        return nullptr;
    }
    
    return it->second();
}

// Создание протокола с учетом ограничений сети
std::unique_ptr<IProtocol> ProtocolFactory::createProtocol(
    const std::string& preferred_protocol,
    const NetworkConstraints& constraints) {
    
    // Выбираем оптимальный протокол с учетом ограничений
    std::string optimal_protocol = selectOptimalProtocol(preferred_protocol, constraints);
    
    if (optimal_protocol.empty()) {
        LogError("No suitable protocol found for the given constraints");
        return nullptr;
    }
    
    return createProtocol(optimal_protocol);
}

// Создание резервного протокола
std::unique_ptr<IProtocol> ProtocolFactory::createFallbackProtocol(
    const std::string& failed_protocol) {
    
    // Выбираем резервный протокол
    std::string fallback_protocol = selectFallbackProtocol(failed_protocol);
    
    if (fallback_protocol.empty()) {
        LogError("No fallback protocol available for: " + failed_protocol);
        return nullptr;
    }
    
    return createProtocol(fallback_protocol);
}

// Доступные протоколы
std::vector<std::string> ProtocolFactory::getAvailableProtocols() {
    auto& registry = getProtocolRegistry();
    std::vector<std::string> protocols;
    
    for (const auto& entry : registry) {
        protocols.push_back(entry.first);
    }
    
    return protocols;
}

// Выбор оптимального протокола с учетом ограничений
std::string ProtocolFactory::selectOptimalProtocol(
    const std::string& preferred_protocol,
    const NetworkConstraints& constraints) {
    
    auto& registry = getProtocolRegistry();
    
    // Если предпочтительный протокол доступен и подходит под ограничения,
    // используем его
    if (registry.find(preferred_protocol) != registry.end()) {
        // Для HTTPS порт 443 должен быть разрешен
        if (preferred_protocol == "https") {
            if (!constraints.restricted_ports || 
                std::find(constraints.allowed_ports.begin(), 
                         constraints.allowed_ports.end(), 443) != 
                constraints.allowed_ports.end()) {
                return preferred_protocol;
            }
        }
        // Для HTTP порт 80 должен быть разрешен
        else if (preferred_protocol == "http") {
            // Если глубокий анализ пакетов, лучше не использовать HTTP
            if (!constraints.deep_packet_inspection) {
                if (!constraints.restricted_ports || 
                    std::find(constraints.allowed_ports.begin(), 
                             constraints.allowed_ports.end(), 80) != 
                    constraints.allowed_ports.end()) {
                    return preferred_protocol;
                }
            }
        }
        // Для WebSockets порт 443 должен быть разрешен (WSS)
        else if (preferred_protocol == "ws" || preferred_protocol == "wss") {
            if (!constraints.restricted_ports || 
                std::find(constraints.allowed_ports.begin(), 
                         constraints.allowed_ports.end(), 443) != 
                constraints.allowed_ports.end()) {
                return preferred_protocol;
            }
        }
        // Для DoH порт 443 должен быть разрешен
        else if (preferred_protocol == "doh") {
            if (!constraints.restricted_ports || 
                std::find(constraints.allowed_ports.begin(), 
                         constraints.allowed_ports.end(), 443) != 
                constraints.allowed_ports.end()) {
                return preferred_protocol;
            }
        }
    }
    
    // Если нет подходящего протокола, выбираем по приоритету
    std::vector<std::string> priority_list;
    
    // Если глубокий анализ пакетов, предпочитаем DoH
    if (constraints.deep_packet_inspection) {
        priority_list = {"doh", "https", "wss", "http", "ws"};
    }
    // Иначе предпочитаем стандартные веб-протоколы
    else {
        priority_list = {"https", "wss", "doh", "http", "ws"};
    }
    
    // Перебираем по приоритету
    for (const auto& protocol : priority_list) {
        if (registry.find(protocol) != registry.end()) {
            // Проверяем соответствие ограничениям
            if (protocol == "https" || protocol == "wss" || protocol == "doh") {
                if (!constraints.restricted_ports || 
                    std::find(constraints.allowed_ports.begin(), 
                             constraints.allowed_ports.end(), 443) != 
                    constraints.allowed_ports.end()) {
                    return protocol;
                }
            }
            else if (protocol == "http" || protocol == "ws") {
                if (!constraints.restricted_ports || 
                    std::find(constraints.allowed_ports.begin(), 
                             constraints.allowed_ports.end(), 80) != 
                    constraints.allowed_ports.end()) {
                    return protocol;
                }
            }
        }
    }
    
    // Если ничего не подходит, возвращаем HTTP как самый базовый
    if (registry.find("http") != registry.end()) {
        return "http";
    }
    
    // Если и HTTP нет, возвращаем первый доступный
    if (!registry.empty()) {
        return registry.begin()->first;
    }
    
    return "";
}

// Выбор резервного протокола
std::string ProtocolFactory::selectFallbackProtocol(
    const std::string& failed_protocol) {
    
    auto& registry = getProtocolRegistry();
    
    // Карта резервных протоколов
    static const std::map<std::string, std::vector<std::string>> fallbacks = {
        {"https", {"http", "doh", "wss", "ws"}},
        {"http", {"https", "doh", "wss", "ws"}},
        {"wss", {"https", "http", "doh", "ws"}},
        {"ws", {"wss", "https", "http", "doh"}},
        {"doh", {"https", "http", "wss", "ws"}}
    };
    
    // Ищем в карте резервных протоколов
    auto it = fallbacks.find(failed_protocol);
    if (it != fallbacks.end()) {
        for (const auto& protocol : it->second) {
            if (registry.find(protocol) != registry.end()) {
                return protocol;
            }
        }
    }
    
    // Если нет в карте или все резервные недоступны,
    // возвращаем случайный доступный, кроме неудавшегося
    std::vector<std::string> available;
    for (const auto& entry : registry) {
        if (entry.first != failed_protocol) {
            available.push_back(entry.first);
        }
    }
    
    if (!available.empty()) {
        // Выбираем случайный из доступных
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, available.size() - 1);
        return available[dis(gen)];
    }
    
    return "";
}

} // namespace comm
} // namespace mceas