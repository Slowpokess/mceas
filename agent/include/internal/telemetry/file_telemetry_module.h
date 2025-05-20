#ifndef MCEAS_FILE_TELEMETRY_MODULE_H
#define MCEAS_FILE_TELEMETRY_MODULE_H

#include "internal/common.h"
#include "internal/types.h"
#include "internal/module_base.h"
#include <vector>
#include <memory>
#include <string>
#include <set>
#include <mutex>

namespace mceas {

// Структура для хранения метаданных файла
struct FileMetadata {
    std::string path;
    std::string name;
    std::string extension;
    std::string mime_type;
    uint64_t size;
    time_t creation_time;
    time_t modification_time;
    time_t access_time;
    std::string owner;
    std::string permissions;
    std::string hash;
    bool is_encrypted;
    std::string classification;
};

// Перечисление типов файлов для классификации
enum class FileClassification {
    DOCUMENT,        // Документы (doc, docx, pdf, txt)
    SPREADSHEET,     // Таблицы (xls, xlsx, csv)
    PRESENTATION,    // Презентации (ppt, pptx)
    SOURCE_CODE,     // Исходный код (cpp, py, js и т.д.)
    DATABASE,        // Базы данных (db, sqlite, etc)
    ARCHIVE,         // Архивы (zip, rar, 7z, etc)
    IMAGE,           // Изображения (jpg, png, gif, etc)
    AUDIO,           // Аудио (mp3, wav, etc)
    VIDEO,           // Видео (mp4, avi, etc)
    EXECUTABLE,      // Исполняемые файлы (exe, dll, so, etc)
    CONFIGURATION,   // Конфигурационные файлы (ini, xml, json)
    SENSITIVE,       // Потенциально чувствительные файлы
    UNKNOWN          // Неизвестный тип
};

// Интерфейс сканера файлов
class FileScanner {
public:
    virtual ~FileScanner() = default;
    
    // Сканирование директории с опциональной рекурсией
    virtual std::vector<FileMetadata> scanDirectory(const std::string& directory, 
                                                  bool recursive, 
                                                  int max_depth = 5) = 0;
    
    // Сканирование отдельного файла
    virtual FileMetadata scanFile(const std::string& file_path) = 0;
    
    // Установка фильтров для сканирования
    virtual void setExtensionFilter(const std::set<std::string>& extensions) = 0;
    virtual void setSizeFilter(uint64_t min_size, uint64_t max_size) = 0;
    virtual void setTimeFilter(time_t from_time, time_t to_time) = 0;
    
    // Возвращает статус последнего сканирования
    virtual bool getLastScanStatus() const = 0;
};

// Интерфейс анализатора содержимого файлов
class ContentAnalyzer {
public:
    virtual ~ContentAnalyzer() = default;
    
    // Анализ содержимого файла
    virtual bool analyzeFile(const std::string& file_path, std::map<std::string, std::string>& results) = 0;
    
    // Поиск паттернов в файле
    virtual bool findPatterns(const std::string& file_path, 
                            const std::vector<std::string>& patterns,
                            std::vector<std::string>& matches) = 0;
    
    // Проверка на потенциально чувствительное содержимое
    virtual bool checkForSensitiveData(const std::string& file_path, 
                                     std::vector<std::string>& findings) = 0;
};

// Интерфейс классификатора файлов
class FileClassifier {
public:
    virtual ~FileClassifier() = default;
    
    // Классификация файла по метаданным
    virtual FileClassification classifyByMetadata(const FileMetadata& metadata) = 0;
    
    // Классификация файла по содержимому
    virtual FileClassification classifyByContent(const std::string& file_path) = 0;
    
    // Получение текстового описания классификации
    virtual std::string getClassificationName(FileClassification classification) = 0;
};

// Основной модуль файловой телеметрии
class FileTelemetryModule : public ModuleBase {
public:
    FileTelemetryModule();
    ~FileTelemetryModule() override;

protected:
    // Реализация методов базового класса
    bool onInitialize() override;
    bool onStart() override;
    bool onStop() override;
    ModuleResult onExecute() override;
    bool onHandleCommand(const Command& command) override;

private:
    // Инициализация компонентов
    void initComponents();
    
    // Сканирование системных директорий
    void scanSystemDirectories();
    
    // Сканирование пользовательских директорий
    void scanUserDirectories();
    
    // Поиск файлов по шаблону
    std::vector<FileMetadata> findFilesByPattern(const std::string& pattern);
    
    // Сохранение результатов сканирования
    void saveResults();
    
    // Компоненты модуля
    std::unique_ptr<FileScanner> file_scanner_;
    std::unique_ptr<ContentAnalyzer> content_analyzer_;
    std::unique_ptr<FileClassifier> file_classifier_;
    
    // Данные о последнем сканировании
    std::vector<FileMetadata> scan_results_;
    std::map<std::string, int> file_stats_;
    time_t last_scan_time_;
    
    // Конфигурация сканирования
    std::set<std::string> target_extensions_;
    std::set<std::string> excluded_directories_;
    uint64_t min_file_size_;
    uint64_t max_file_size_;
    bool scan_hidden_files_;
    int max_scan_depth_;
    int max_files_to_scan_;
    
    // Мьютекс для потокобезопасности
    std::mutex scan_mutex_;
};

} // namespace mceas

#endif // MCEAS_FILE_TELEMETRY_MODULE_H