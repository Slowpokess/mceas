#include "internal/file_telemetry_module.h"
#include "internal/file_telemetry/scanner.h"
#include "internal/file_telemetry/analyzer.h"
#include "internal/file_telemetry/classifier.h"
#include "internal/logging.h"
#include "internal/utils/platform_utils.h"
#include <chrono>
#include <filesystem>
#include <sstream>
#include "external/json/json.hpp"

namespace mceas {

FileTelemetryModule::FileTelemetryModule() 
    : ModuleBase("FileTelemetryModule", ModuleType::FILE),
      last_scan_time_(0),
      min_file_size_(0),
      max_file_size_(std::numeric_limits<uint64_t>::max()),
      scan_hidden_files_(false),
      max_scan_depth_(3),
      max_files_to_scan_(1000) {
}

FileTelemetryModule::~FileTelemetryModule() {
    if (running_) {
        stop();
    }
}

bool FileTelemetryModule::onInitialize() {
    LogInfo("Initializing File Telemetry Module");
    
    // Инициализация компонентов
    initComponents();
    
    // Настройка фильтров по умолчанию
    // Добавляем расширения файлов, которые нас интересуют
    target_extensions_.insert(".doc");
    target_extensions_.insert(".docx");
    target_extensions_.insert(".xls");
    target_extensions_.insert(".xlsx");
    target_extensions_.insert(".ppt");
    target_extensions_.insert(".pptx");
    target_extensions_.insert(".pdf");
    target_extensions_.insert(".txt");
    target_extensions_.insert(".csv");
    target_extensions_.insert(".json");
    target_extensions_.insert(".xml");
    target_extensions_.insert(".ini");
    target_extensions_.insert(".cfg");
    target_extensions_.insert(".conf");
    
    // Устанавливаем директории, которые исключаем из сканирования
    excluded_directories_.insert("/System");
    excluded_directories_.insert("/Library/Caches");
    excluded_directories_.insert("/var/tmp");
    excluded_directories_.insert("/tmp");
    
    // Устанавливаем фильтры сканера
    file_scanner_->setExtensionFilter(target_extensions_);
    file_scanner_->setSizeFilter(min_file_size_, max_file_size_);
    
    return true;
}

bool FileTelemetryModule::onStart() {
    LogInfo("Starting File Telemetry Module");
    return true;
}

bool FileTelemetryModule::onStop() {
    LogInfo("Stopping File Telemetry Module");
    return true;
}

ModuleResult FileTelemetryModule::onExecute() {
    LogInfo("Executing File Telemetry Module");
    
    ModuleResult result;
    result.module_name = getName();
    result.type = getType();
    result.success = true;
    
    try {
        std::lock_guard<std::mutex> lock(scan_mutex_);
        
        // Очищаем результаты предыдущего сканирования
        scan_results_.clear();
        file_stats_.clear();
        
        // Сканируем только домашнюю директорию пользователя для тестов
        // В реальном приложении здесь может быть более сложная логика
        std::string home_dir = utils::GetHomeDir();
        if (!home_dir.empty()) {
            LogInfo("Scanning user home directory: " + home_dir);
            
            // Сканируем документы пользователя
            std::string documents_dir = home_dir + "/Documents";
            if (std::filesystem::exists(documents_dir)) {
                auto docs_results = file_scanner_->scanDirectory(documents_dir, true, max_scan_depth_);
                scan_results_.insert(scan_results_.end(), docs_results.begin(), docs_results.end());
                LogInfo("Found " + std::to_string(docs_results.size()) + " files in Documents directory");
            }
            
            // Сканируем загрузки пользователя
            std::string downloads_dir = home_dir + "/Downloads";
            if (std::filesystem::exists(downloads_dir)) {
                auto downloads_results = file_scanner_->scanDirectory(downloads_dir, true, max_scan_depth_);
                scan_results_.insert(scan_results_.end(), downloads_results.begin(), downloads_results.end());
                LogInfo("Found " + std::to_string(downloads_results.size()) + " files in Downloads directory");
            }
            
            // Обновляем время последнего сканирования
            last_scan_time_ = std::time(nullptr);
            
            // Собираем статистику по типам файлов
            for (const auto& file : scan_results_) {
                std::string ext = file.extension;
                file_stats_[ext]++;
                
                // Классифицируем файл
                FileClassification classification = file_classifier_->classifyByMetadata(file);
                file_stats_[file_classifier_->getClassificationName(classification)]++;
            }
        }
        else {
            LogError("Failed to get user home directory");
            result.success = false;
            result.error_message = "Failed to get user home directory";
            return result;
        }
        
        // Добавляем результаты в выходные данные
        addResultData("files.total_count", std::to_string(scan_results_.size()));
        addResultData("files.last_scan_time", std::to_string(last_scan_time_));
        
        // Добавляем статистику по типам файлов
        for (const auto& stat : file_stats_) {
            addResultData("files.stats." + stat.first, std::to_string(stat.second));
        }
        
        // Создаем JSON с информацией о найденных файлах (ограничиваем количество)
        if (!scan_results_.empty()) {
            nlohmann::json files_json = nlohmann::json::array();
            size_t count = 0;
            size_t max_files = 100; // Ограничиваем количество файлов в JSON
            
            for (const auto& file : scan_results_) {
                if (count >= max_files) break;
                
                nlohmann::json file_json;
                file_json["path"] = file.path;
                file_json["name"] = file.name;
                file_json["extension"] = file.extension;
                file_json["size"] = file.size;
                file_json["modification_time"] = file.modification_time;
                file_json["classification"] = file_classifier_->getClassificationName(
                    file_classifier_->classifyByMetadata(file));
                
                files_json.push_back(file_json);
                count++;
            }
            
            addResultData("files_json", files_json.dump());
        }
        
        return result;
    }
    catch (const std::exception& e) {
        LogError("Error executing file telemetry module: " + std::string(e.what()));
        result.success = false;
        result.error_message = "Error executing file telemetry module: " + std::string(e.what());
        return result;
    }
}

bool FileTelemetryModule::onHandleCommand(const Command& command) {
    LogInfo("Handling command in File Telemetry Module: " + command.action);
    
    // Обрабатываем команды
    if (command.action == "scan_directory") {
        // Сканирование указанной директории
        auto it = command.parameters.find("path");
        if (it != command.parameters.end()) {
            std::string path = it->second;
            
            if (std::filesystem::exists(path)) {
                // Устанавливаем параметры сканирования из команды
                bool recursive = true;
                int max_depth = max_scan_depth_;
                
                auto recursive_it = command.parameters.find("recursive");
                if (recursive_it != command.parameters.end()) {
                    recursive = (recursive_it->second == "true");
                }
                
                auto depth_it = command.parameters.find("max_depth");
                if (depth_it != command.parameters.end()) {
                    try {
                        max_depth = std::stoi(depth_it->second);
                    }
                    catch (...) {
                        // Используем значение по умолчанию
                    }
                }
                
                LogInfo("Scanning directory: " + path + " (recursive: " + 
                         (recursive ? "true" : "false") + ", max depth: " + 
                         std::to_string(max_depth) + ")");
                
                // Выполняем сканирование
                auto results = file_scanner_->scanDirectory(path, recursive, max_depth);
                
                // Обновляем результаты
                std::lock_guard<std::mutex> lock(scan_mutex_);
                scan_results_ = results;
                
                // Обновляем статистику
                file_stats_.clear();
                for (const auto& file : scan_results_) {
                    std::string ext = file.extension;
                    file_stats_[ext]++;
                }
                
                LogInfo("Scan completed. Found " + std::to_string(results.size()) + " files");
                return true;
            }
            else {
                LogWarning("Directory does not exist: " + path);
                return false;
            }
        }
        
        LogWarning("Missing path parameter in scan_directory command");
        return false;
    }
    else if (command.action == "analyze_file") {
        // Анализ указанного файла
        auto it = command.parameters.find("path");
        if (it != command.parameters.end()) {
            std::string path = it->second;
            
            if (std::filesystem::exists(path)) {
                LogInfo("Analyzing file: " + path);
                
                // Сканируем файл для получения метаданных
                FileMetadata metadata = file_scanner_->scanFile(path);
                
                // Анализируем содержимое файла
                std::map<std::string, std::string> analysis_results;
                if (content_analyzer_->analyzeFile(path, analysis_results)) {
                    LogInfo("File analysis completed");
                    
                    // Проверяем на наличие чувствительных данных
                    std::vector<std::string> findings;
                    if (content_analyzer_->checkForSensitiveData(path, findings)) {
                        LogInfo("Found sensitive data in file: " + std::to_string(findings.size()) + " items");
                        
                        // Можно добавить логику для обработки результатов
                    }
                    
                    return true;
                }
                else {
                    LogWarning("Failed to analyze file: " + path);
                    return false;
                }
            }
            else {
                LogWarning("File does not exist: " + path);
                return false;
            }
        }
        
        LogWarning("Missing path parameter in analyze_file command");
        return false;
    }
    else if (command.action == "search_files") {
        // Поиск файлов по шаблону
        auto it = command.parameters.find("pattern");
        if (it != command.parameters.end()) {
            std::string pattern = it->second;
            LogInfo("Searching files by pattern: " + pattern);
            
            // Выполняем поиск файлов
            auto results = findFilesByPattern(pattern);
            
            LogInfo("Search completed. Found " + std::to_string(results.size()) + " files");
            return true;
        }
        
        LogWarning("Missing pattern parameter in search_files command");
        return false;
    }
    
    LogWarning("Unknown command action: " + command.action);
    return false;
}

void FileTelemetryModule::initComponents() {
    // Создаем экземпляры компонентов
    file_scanner_ = std::make_unique<FileScannerImpl>();
    content_analyzer_ = std::make_unique<ContentAnalyzerImpl>();
    file_classifier_ = std::make_unique<FileClassifierImpl>();
    
    LogInfo("File Telemetry components initialized");
}

void FileTelemetryModule::scanSystemDirectories() {
    // Сканирование системных директорий
    // Не реализовано для первой версии, так как требует прав администратора
    LogInfo("System directories scanning is not implemented yet");
}

void FileTelemetryModule::scanUserDirectories() {
    // Получаем домашнюю директорию пользователя
    std::string home_dir = utils::GetHomeDir();
    if (!home_dir.empty()) {
        LogInfo("Scanning user directories in: " + home_dir);
        
        // Список директорий для сканирования
        std::vector<std::string> dirs_to_scan = {
            home_dir + "/Documents",
            home_dir + "/Downloads",
            home_dir + "/Desktop"
        };
        
        // Сканируем каждую директорию
        for (const auto& dir : dirs_to_scan) {
            if (std::filesystem::exists(dir)) {
                LogInfo("Scanning directory: " + dir);
                auto results = file_scanner_->scanDirectory(dir, true, max_scan_depth_);
                scan_results_.insert(scan_results_.end(), results.begin(), results.end());
                LogInfo("Found " + std::to_string(results.size()) + " files in " + dir);
            }
        }
    }
    else {
        LogError("Failed to get user home directory");
    }
}

std::vector<FileMetadata> FileTelemetryModule::findFilesByPattern(const std::string& pattern) {
    std::vector<FileMetadata> results;
    
    // Здесь должна быть реализация поиска файлов по шаблону
    // Например, с использованием регулярных выражений или wildcard-матчинга
    LogInfo("File pattern search is not fully implemented yet");
    
    return results;
}

void FileTelemetryModule::saveResults() {
    // Сохранение результатов сканирования
    // В текущей версии не требуется, так как результаты возвращаются в ModuleResult
    LogInfo("Results saving is not implemented yet");
}

} // namespace mceas