#ifndef MCEAS_CONTENT_ANALYZER_H
#define MCEAS_CONTENT_ANALYZER_H

#include "telemetry/file_telemetry_module.h"
#include <string>
#include <vector>
#include <map>
#include <regex>

namespace mceas {

// Реализация анализатора содержимого
class ContentAnalyzerImpl : public ContentAnalyzer {
public:
    ContentAnalyzerImpl();
    ~ContentAnalyzerImpl() override = default;
    
    // Реализация методов интерфейса ContentAnalyzer
    bool analyzeFile(const std::string& file_path, std::map<std::string, std::string>& results) override;
    
    bool findPatterns(const std::string& file_path, 
                    const std::vector<std::string>& patterns,
                    std::vector<std::string>& matches) override;
    
    bool checkForSensitiveData(const std::string& file_path, 
                             std::vector<std::string>& findings) override;
    
private:
    // Проверка, является ли файл текстовым
    bool isTextFile(const std::string& file_path);
    
    // Чтение файла в строку
    bool readFileContent(const std::string& file_path, std::string& content);
    
    // Шаблоны для поиска чувствительных данных
    std::vector<std::pair<std::string, std::regex>> sensitive_patterns_;
    
    // Максимальный размер файла для анализа
    const uint64_t MAX_FILE_SIZE = 10 * 1024 * 1024; // 10 МБ
};

} // namespace mceas

#endif // MCEAS_CONTENT_ANALYZER_H