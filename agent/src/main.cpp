#include "internal/agent.h"
#include "internal/logging.h"
#include <iostream>
#include <string>
#include <csignal>
#include <thread>
#include <chrono>

using namespace mceas;

// Глобальный указатель на агента для обработки сигналов
static Agent* g_agent = nullptr;

// Обработчик сигналов для корректного завершения работы
void signalHandler(int signal) {
    LogInfo("Received signal: " + std::to_string(signal));
    
    if (g_agent) {
        LogInfo("Stopping agent...");
        g_agent->stop();
    }
}

int main(int argc, char* argv[]) {
    try {
        LogInfo("Starting MCEAS Agent");
        
        // Инициализация агента
        Agent agent;
        g_agent = &agent;
        
        // Регистрация обработчиков сигналов
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        
        // Определение пути к конфигурационному файлу
        std::string config_path = "config.json";
        
        // Парсинг аргументов командной строки
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--config" && i + 1 < argc) {
                config_path = argv[++i];
            }
            else if (arg == "--help") {
                std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
                std::cout << "Options:" << std::endl;
                std::cout << "  --config PATH  Path to configuration file" << std::endl;
                std::cout << "  --help         Show this help message" << std::endl;
                return 0;
            }
        }
        
        // Инициализация агента с указанным конфигурационным файлом
        StatusCode init_status = agent.initialize(config_path);
        if (init_status != StatusCode::SUCCESS) {
            LogError("Failed to initialize agent. Status code: " + std::to_string(static_cast<int>(init_status)));
            return 1;
        }
        
        // Запуск агента
        StatusCode start_status = agent.start();
        if (start_status != StatusCode::SUCCESS) {
            LogError("Failed to start agent. Status code: " + std::to_string(static_cast<int>(start_status)));
            return 1;
        }
        
        LogInfo("Agent started successfully with ID: " + agent.getAgentId());
        
        // Основной цикл приложения
        // В реальном приложении здесь может быть логика обработки команд
        // либо просто ожидание завершения работы агента
        while (agent.isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        LogInfo("Agent stopped. Exiting...");
        return 0;
    }
    catch (const std::exception& e) {
        LogCritical("Unhandled exception: " + std::string(e.what()));
        return 1;
    }
}