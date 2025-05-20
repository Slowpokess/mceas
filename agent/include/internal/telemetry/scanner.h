#ifndef MCEAS_FILE_SCANNER_H
#define MCEAS_FILE_SCANNER_H

#include "telemetry/file_telemetry_module.h"
#include <string>
#include <vector>
#include <set>
#include <mutex>

namespace mceas {

// Реализация сканера файлов
class FileScannerImpl : public FileScanner {
public:
    FileScannerImpl();
    ~FileScannerImpl() override = default;
    
    // Реализация методов интерфейса FileScanner
    std::vector<FileMetadata> scanDirectory(const std::string& directory, 
                                         bool recursive, 
                                         int max_depth = 5) override;
    
    FileMetadata scanFile(const std::string& file_path) override;
    
    void setExtensionFilter(const std::set<std::string>& extensions) override;
    void setSizeFilter(uint64_t min_size, uint64_t max_size) override;
    void setTimeFilter(time_t from_time, time_t to_time) override;
    
    bool getLastScanStatus() const override;
    
private:
    // Извлечение метаданных из файла
    bool extractMetadata(const std::string& file_path, FileMetadata& metadata);
    
    // Вычисление хеша файла
    std::string calculateFileHash(const std::string& file_path);
    
    // Проверка, подходит ли файл под фильтры
    bool matchesFilters(const FileMetadata& metadata);
    
    // Фильтры
    std::set<std::string> extension_filter_;
    uint64_t min_size_;
    uint64_t max_size_;
    time_t from_time_;
    time_t to_time_;
    
    // Статус последнего сканирования
    bool last_scan_success_;
    
    // Мьютекс для потокобезопасности
    mutable std::mutex mutex_;
};

} // namespace mceas

#endif // MCEAS_FILE_SCANNER_H