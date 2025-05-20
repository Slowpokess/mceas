#include "internal/file_telemetry/analyzer.h"
#include "internal/logging.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace mceas {

ContentAnalyzerImpl::ContentAnalyzerImpl() {
    // Инициализация регулярных выражений для поиска чувствительной информации
    
    // Шаблоны для кредитных карт основных типов
    sensitive_patterns_.push_back(std::make_pair(
        "Credit card (Visa)", 
        std::regex("4[0-9]{3}[\\s-]?[0-9]{4}[\\s-]?[0-9]{4}[\\s-]?[0-9]{4}")
    ));
    
    sensitive_patterns_.push_back(std::make_pair(
        "Credit card (MasterCard)", 
        std::regex("5[1-5][0-9]{2}[\\s-]?[0-9]{4}[\\s-]?[0-9]{4}[\\s-]?[0-9]{4}")
    ));
    
    sensitive_patterns_.push_back(std::make_pair(
        "Credit card (Amex)", 
        std::regex("3[47][0-9]{2}[\\s-]?[0-9]{6}[\\s-]?[0-9]{5}")
    ));
    
    // Шаблон для e-mail адресов
    sensitive_patterns_.push_back(std::make_pair(
        "Email address", 
        std::regex("[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}")
    ));
    
    // Шаблон для возможных паролей (примитивный)
    sensitive_patterns_.push_back(std::make_pair(
        "Possible password", 
        std::regex("password[\\s=:\"]+(\\S+)|pwd[\\s=:\"]+(\\S+)")
    ));
    
    // Шаблон для возможных приватных ключей
    sensitive_patterns_.push_back(std::make_pair(
        "Private key", 
        std::regex("-----BEGIN PRIVATE KEY-----|-----BEGIN RSA PRIVATE KEY-----")
    ));
    
    // Шаблон для API ключей
    sensitive_patterns_.push_back(std::make_pair(
        "API key", 
        std::regex("api[_\\s-]?key[\\s=:\"]+(\\S+)")
    ));
    
    // Шаблон для IP-адресов
    sensitive_patterns_.push_back(std::make_pair(
        "IP address", 
        std::regex("\\b\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\b")
    ));
}

bool ContentAnalyzerImpl::analyzeFile(const std::string& file_path, std::map<std::string, std::string>& results) {
    try {
        if (!std::filesystem::exists(file_path)) {
            LogWarning("File does not exist: " + file_path);
            return false;
        }
        
        // Проверяем размер файла
        auto file_size = std::filesystem::file_size(file_path);
        if (file_size > MAX_FILE_SIZE) {
            LogWarning("File is too large for analysis: " + file_path + " (" + 
                       std::to_string(file_size) + " bytes)");
            return false;
        }
        
        // Определяем, является ли файл текстовым
        bool is_text = isTextFile(file_path);
        
        // Заполняем базовую информацию
        results["file_path"] = file_path;
        results["file_size"] = std::to_string(file_size);
        results["is_text"] = is_text ? "true" : "false";
        
        // Для текстовых файлов выполняем дополнительный анализ
        if (is_text) {
            std::string content;
            if (readFileContent(file_path, content)) {
                // Подсчитываем количество строк
                int line_count = std::count(content.begin(), content.end(), '\n') + 1;
                results["line_count"] = std::to_string(line_count);
                
                // Можно выполнить другой анализ текстового содержимого
                // ...
            }
            else {
                LogWarning("Failed to read file content: " + file_path);
            }
        }
        
        // Проверяем на наличие чувствительных данных
        std::vector<std::string> findings;
        if (checkForSensitiveData(file_path, findings)) {
            results["has_sensitive_data"] = "true";
            results["sensitive_data_count"] = std::to_string(findings.size());
            
            // Ограничиваем количество записей
            int max_findings = 10;
            for (int i = 0; i < std::min(max_findings, static_cast<int>(findings.size())); i++) {
                results["sensitive_data_" + std::to_string(i)] = findings[i];
            }
        }
        else {
            results["has_sensitive_data"] = "false";
        }
        
        return true;
    }
    catch (const std::exception& e) {
        LogError("Error analyzing file: " + std::string(e.what()));
        return false;
    }
}

bool ContentAnalyzerImpl::findPatterns(const std::string& file_path, 
                                     const std::vector<std::string>& patterns, 
                                     std::vector<std::string>& matches) {
    try {
        if (!std::filesystem::exists(file_path)) {
            LogWarning("File does not exist: " + file_path);
            return false;
        }
        
        // Проверяем размер файла
        auto file_size = std::filesystem::file_size(file_path);
        if (file_size > MAX_FILE_SIZE) {
            LogWarning("File is too large for pattern search: " + file_path);
            return false;
        }
        
        // Проверяем, является ли файл текстовым
        if (!isTextFile(file_path)) {
            LogWarning("File is not a text file: " + file_path);
            return false;
        }
        
        // Читаем содержимое файла
        std::string content;
        if (!readFileContent(file_path, content)) {
            LogWarning("Failed to read file content: " + file_path);
            return false;
        }
        
        // Компилируем регулярные выражения из паттернов
        std::vector<std::regex> regexes;
        for (const auto& pattern : patterns) {
            try {
                regexes.push_back(std::regex(pattern, std::regex::ECMAScript));
            }
            catch (const std::regex_error& e) {
                LogWarning("Invalid regex pattern: " + pattern + " (" + e.what() + ")");
            }
        }
        
        // Ищем совпадения
        for (size_t i = 0; i < regexes.size(); i++) {
            auto& regex = regexes[i];
            auto& pattern = patterns[i];
            
            std::sregex_iterator it(content.begin(), content.end(), regex);
            std::sregex_iterator end;
            
            while (it != end) {
                std::smatch match = *it;
                matches.push_back("Pattern '" + pattern + "': " + match.str());
                ++it;
            }
        }
        
        return !matches.empty();
    }
    catch (const std::exception& e) {
        LogError("Error finding patterns: " + std::string(e.what()));
        return false;
    }
}

bool ContentAnalyzerImpl::checkForSensitiveData(const std::string& file_path, 
                                              std::vector<std::string>& findings) {
    try {
        if (!std::filesystem::exists(file_path)) {
            LogWarning("File does not exist: " + file_path);
            return false;
        }
        
        // Проверяем размер файла
        auto file_size = std::filesystem::file_size(file_path);
        if (file_size > MAX_FILE_SIZE) {
            LogWarning("File is too large for sensitive data check: " + file_path);
            return false;
        }
        
        // Проверяем, является ли файл текстовым
        if (!isTextFile(file_path)) {
            return false;
        }
        
        // Читаем содержимое файла
        std::string content;
        if (!readFileContent(file_path, content)) {
            LogWarning("Failed to read file content: " + file_path);
            return false;
        }
        
        // Ищем чувствительные данные
        for (const auto& pattern : sensitive_patterns_) {
            std::string pattern_name = pattern.first;
            std::regex pattern_regex = pattern.second;
            
            std::sregex_iterator it(content.begin(), content.end(), pattern_regex);
            std::sregex_iterator end;
            
            while (it != end) {
                std::smatch match = *it;
                findings.push_back(pattern_name + ": " + match.str());
                ++it;
            }
        }
        
        return !findings.empty();
    }
    catch (const std::exception& e) {
        LogError("Error checking for sensitive data: " + std::string(e.what()));
        return false;
    }
}

bool ContentAnalyzerImpl::isTextFile(const std::string& file_path) {
    try {
        // Открываем файл в бинарном режиме
        std::ifstream file(file_path, std::ios::binary);
        if (!file) {
            return false;
        }
        
        // Читаем первые 4096 байт для анализа
        char buffer[4096];
        std::streamsize bytes_read = file.read(buffer, sizeof(buffer)).gcount();
        
        // Если файл пустой, считаем его текстовым
        if (bytes_read == 0) {
            return true;
        }
        
        // Подсчитываем количество нулевых байтов
        int zero_bytes = 0;
        for (std::streamsize i = 0; i < bytes_read; i++) {
            if (buffer[i] == '\0') {
                zero_bytes++;
            }
        }
        
        // Если в файле много нулевых байтов, вероятно, это бинарный файл
        double zero_ratio = static_cast<double>(zero_bytes) / bytes_read;
        if (zero_ratio > 0.1) {
            return false;
        }
        
        // Проверяем наличие недопустимых в тексте символов
        int control_chars = 0;
        for (std::streamsize i = 0; i < bytes_read; i++) {
            unsigned char c = buffer[i];
            // Пропускаем общепринятые управляющие символы
            if (c == 9 || c == 10 || c == 13) {
                continue;
            }
            
            // Подсчитываем другие управляющие символы
            if (c < 32) {
                control_chars++;
            }
        }
        
        // Если слишком много управляющих символов, вероятно, это бинарный файл
        double control_ratio = static_cast<double>(control_chars) / bytes_read;
        if (control_ratio > 0.1) {
            return false;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        LogError("Error checking if file is text: " + std::string(e.what()));
        return false;
    }
}

bool ContentAnalyzerImpl::readFileContent(const std::string& file_path, std::string& content) {
    try {
        std::ifstream file(file_path);
        if (!file) {
            return false;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        content = buffer.str();
        
        return true;
    }
    catch (const std::exception& e) {
        LogError("Error reading file content: " + std::string(e.what()));
        return false;
    }
}

} // namespace mceas