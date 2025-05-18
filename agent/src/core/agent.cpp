#include "internal/agent.h"
#include "internal/config.h"
#include "internal/module_manager.h"
#include "internal/updater.h"
#include "internal/logging.h"
#include <chrono>
#include <thread>

namespace mceas {

Agent::Agent() : running_(false) {
    // Генерируем уникальный ID агента
    agent_id_ = "MCEAS-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
}

Agent::~Agent() {
    if (running_) {
        stop();
    }
}

StatusCode Agent::initialize(const std::string& config_path) {
    try {
        // Инициализация компонентов
        config_ = std::make_unique<Config>(config_path);
        
        // Если передан путь к конфигурации, загружаем из него
        if (!config_path.empty()) {
            if (!config_->loadFromFile(config_path)) {
                LogError("Failed to load configuration from: " + config_path);
                return StatusCode::ERROR_CONFIGURATION;
            }
        }
        
        // Получаем конфигурацию агента
        AgentConfig agent_config = config_->getAgentConfig();
        
        // Если в конфигурации есть ID агента, используем его
        if (!agent_config.agent_id.empty()) {
            agent_id_ = agent_config.agent_id;
        }
        
        // Инициализируем менеджер модулей
        module_manager_ = std::make_unique<ModuleManager>(agent_config);
        
        // Инициализируем механизм обновления
        updater_ = std::make_unique<Updater>(agent_config, *config_);
        
        LogInfo("Agent initialized with ID: " + agent_id_);
        return StatusCode::SUCCESS;
    }
    catch (const ConfigurationException& e) {
        LogError("Configuration error during initialization: " + std::string(e.what()));
        return StatusCode::ERROR_CONFIGURATION;
    }
    catch (const ModuleException& e) {
        LogError("Module error during initialization: " + std::string(e.what()));
        return StatusCode::ERROR_MODULE_LOAD;
    }
    catch (const std::exception& e) {
        LogError("Unexpected error during initialization: " + std::string(e.what()));
        return StatusCode::ERROR_SYSTEM;
    }
}

StatusCode Agent::start() {
    if (running_) {
        LogWarning("Agent is already running");
        return StatusCode::SUCCESS;
    }
    
    try {
        // Запускаем модули
        if (!module_manager_->startModules()) {
            LogError("Failed to start modules");
            return StatusCode::ERROR_MODULE_LOAD;
        }
        
        // Обновляем флаг состояния
        running_ = true;
        
        // Запускаем основной поток
        main_thread_ = std::make_unique<std::thread>(&Agent::mainLoop, this);
        
        LogInfo("Agent started successfully");
        return StatusCode::SUCCESS;
    }
    catch (const std::exception& e) {
        LogError("Error starting agent: " + std::string(e.what()));
        return StatusCode::ERROR_SYSTEM;
    }
}

StatusCode Agent::stop() {
    if (!running_) {
        LogWarning("Agent is not running");
        return StatusCode::SUCCESS;
    }
    
    try {
        // Обновляем флаг состояния
        running_ = false;
        
        // Дожидаемся завершения основного потока
        if (main_thread_ && main_thread_->joinable()) {
            main_thread_->join();
        }
        
        // Останавливаем модули
        module_manager_->stopModules();
        
        LogInfo("Agent stopped successfully");
        return StatusCode::SUCCESS;
    }
    catch (const std::exception& e) {
        LogError("Error stopping agent: " + std::string(e.what()));
        return StatusCode::ERROR_SYSTEM;
    }
}

CommandResult Agent::executeCommand(const Command& command) {
    CommandResult result;
    result.command_id = command.command_id;
    
    try {
        // Обработка команды в зависимости от действия
        if (command.action == "update") {
            // Команда обновления
            bool update_success = updater_->checkForUpdates(true);
            result.success = update_success;
            result.result = update_success ? "Update successful" : "No updates available";
        }
        else if (command.action == "restart") {
            // Команда перезапуска
            stop();
            result.success = true;
            result.result = "Agent restarted";
            start();
        }
        else if (command.action == "module_command") {
            // Команда для конкретного модуля
            auto it = command.parameters.find("module");
            if (it != command.parameters.end()) {
                std::string module_name = it->second;
                result.success = module_manager_->executeModuleCommand(module_name, command);
                result.result = "Module command executed";
            }
            else {
                result.success = false;
                result.error_message = "Missing module parameter";
            }
        }
        else {
            result.success = false;
            result.error_message = "Unknown command action: " + command.action;
        }
    }
    catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Error executing command: ") + e.what();
    }
    
    return result;
}

bool Agent::isRunning() const {
    return running_;
}

std::string Agent::getAgentId() const {
    return agent_id_;
}

std::string Agent::getVersion() const {
    return MCEAS_AGENT_VERSION;
}

void Agent::mainLoop() {
    try {
        while (running_) {
            // Обрабатываем результаты от модулей
            processModuleResults();
            
            // Отправляем данные на сервер
            sendDataToServer();
            
            // Проверяем обновления (по расписанию)
            updater_->checkForUpdates(false);
            
            // Пауза для снижения нагрузки
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    catch (const std::exception& e) {
        LogError("Error in main loop: " + std::string(e.what()));
        running_ = false;
    }
}

void Agent::processModuleResults() {
    // Получаем результаты от всех модулей
    auto results = module_manager_->collectResults();
    
    // Здесь можно обрабатывать результаты, например,
    // агрегировать их или выполнять дополнительную логику
    
    // Для отладки выводим информацию о полученных результатах
    for (const auto& result : results) {
        if (result.success) {
            LogDebug("Collected data from module: " + result.module_name);
        }
        else {
            LogWarning("Failed to collect data from module: " + result.module_name +
                      " Error: " + result.error_message);
        }
    }
}

void Agent::sendDataToServer() {
    // Здесь будет логика отправки данных на сервер
    // Пока оставляем заглушку
    LogDebug("Data would be sent to server here");
}

} // namespace mceas