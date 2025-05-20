#ifndef MCEAS_FILE_CLASSIFIER_H
#define MCEAS_FILE_CLASSIFIER_H

#include "telemetry/file_telemetry_module.h"
#include <string>
#include <map>
#include <set>

namespace mceas {

// Реализация классификатора файлов
class FileClassifierImpl : public FileClassifier {
public:
    FileClassifierImpl();
    ~FileClassifierImpl() override = default;
    
    // Реализация методов интерфейса FileClassifier
    FileClassification classifyByMetadata(const FileMetadata& metadata) override;
    
    FileClassification classifyByContent(const std::string& file_path) override;
    
    std::string getClassificationName(FileClassification classification) override;
    
private:
    // Сопоставление расширений файлов с их классификацией
    std::map<std::string, FileClassification> extension_map_;
    
    // Сопоставление MIME-типов с их классификацией
    std::map<std::string, FileClassification> mime_type_map_;
    
    // Расширения файлов, которые могут содержать чувствительные данные
    std::set<std::string> sensitive_extensions_;
    
    // Инициализация отображений расширений и MIME-типов
    void initExtensionMap();
    void initMimeTypeMap();
    void initSensitiveExtensions();
    
    // Определение MIME-типа файла
    std::string detectMimeType(const std::string& file_path);
    
    // Анализ содержимого для определения типа
    FileClassification analyzeFileContent(const std::string& file_path);
};

} // namespace mceas

#endif // MCEAS_FILE_CLASSIFIER_H