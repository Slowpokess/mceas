#include "internal/file_telemetry/scanner.h"
#include "internal/logging.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <openssl/md5.h>

namespace mceas {

FileScannerImpl::FileScannerImpl()
    : min_size_(0), 
      max_size_(std::numeric_limits<uint64_t>::max()),
      from_time_(0), 
      to_time_(std::numeric_limits<time_t>::max()),
      last_scan_success_(false) {
}

std::vector<FileMetadata> FileScannerImpl::scanDirectory(const std::string& directory, bool recursive, int max_depth) {
    std::vector<FileMetadata> results;
    
    try {
        LogInfo("Scanning directory: " + directory + " (recursive: " + 
                 (recursive ? "true" : "false") + ", max depth: " + 
                 std::to_string(max_depth) + ")");
        
        if (!std::filesystem::exists(directory)) {
            LogWarning("Directory does not exist: " + directory);
            last_scan_success_ = false;
            return results;
        }
        
        if (!std::filesystem::is_directory(directory)) {
            LogWarning("Path is not a directory: " + directory);
            last_scan_success_ = false;
            return results;
        }
        
        // Опции для итератора (рекурсивный обход или только текущая директория)
        std::filesystem::directory_options options = std::filesystem::directory_options::skip_permission_denied;
        
        if (recursive) {
            // Рекурсивный обход с учетом максимальной глубины
            for (const auto& entry : std::filesystem::recursive_directory_iterator(
                directory, options)) {
                
                // Пропускаем директории
                if (!entry.is_regular_file()) {
                    continue;
                }
                
                // Проверяем глубину
                if (max_depth > 0) {
                    auto rel_path = std::filesystem::relative(entry.path(), directory);
                    // Подсчитываем количество уровней вложенности
                    int depth = 0;
                    for (const auto& component : rel_path) {
                        depth++;
                    }
                    
                    if (depth > max_depth) {
                        continue;
                    }
                }
                
                // Получаем метаданные файла
                FileMetadata metadata;
                if (extractMetadata(entry.path().string(), metadata)) {
                    // Проверяем, соответствует ли файл фильтрам
                    if (matchesFilters(metadata)) {
                        results.push_back(metadata);
                    }
                }
            }
        }
        else {
            // Только текущая директория
            for (const auto& entry : std::filesystem::directory_iterator(directory, options)) {
                if (!entry.is_regular_file()) {
                    continue;
                }
                
                // Получаем метаданные файла
                FileMetadata metadata;
                if (extractMetadata(entry.path().string(), metadata)) {
                    // Проверяем, соответствует ли файл фильтрам
                    if (matchesFilters(metadata)) {
                        results.push_back(metadata);
                    }
                }
            }
        }
        
        LogInfo("Directory scan completed. Found " + std::to_string(results.size()) + " files");
        last_scan_success_ = true;
    }
    catch (const std::exception& e) {
        LogError("Error scanning directory: " + std::string(e.what()));
        last_scan_success_ = false;
    }
    
    return results;
}

FileMetadata FileScannerImpl::scanFile(const std::string& file_path) {
    FileMetadata metadata;
    
    try {
        if (!std::filesystem::exists(file_path)) {
            LogWarning("File does not exist: " + file_path);
            return metadata;
        }
        
        if (!std::filesystem::is_regular_file(file_path)) {
            LogWarning("Path is not a regular file: " + file_path);
            return metadata;
        }
        
        if (extractMetadata(file_path, metadata)) {
            LogInfo("File scanned successfully: " + file_path);
        }
        else {
            LogWarning("Failed to extract metadata from file: " + file_path);
        }
    }
    catch (const std::exception& e) {
        LogError("Error scanning file: " + std::string(e.what()));
    }
    
    return metadata;
}

void FileScannerImpl::setExtensionFilter(const std::set<std::string>& extensions) {
    std::lock_guard<std::mutex> lock(mutex_);
    extension_filter_ = extensions;
}

void FileScannerImpl::setSizeFilter(uint64_t min_size, uint64_t max_size) {
    std::lock_guard<std::mutex> lock(mutex_);
    min_size_ = min_size;
    max_size_ = max_size;
}

void FileScannerImpl::setTimeFilter(time_t from_time, time_t to_time) {
    std::lock_guard<std::mutex> lock(mutex_);
    from_time_ = from_time;
    to_time_ = to_time;
}

bool FileScannerImpl::getLastScanStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_scan_success_;
}

bool FileScannerImpl::extractMetadata(const std::string& file_path, FileMetadata& metadata) {
    try {
        std::filesystem::path path(file_path);
        
        // Заполняем базовые метаданные
        metadata.path = file_path;
        metadata.name = path.filename().string();
        metadata.extension = path.extension().string();
        
        // Получаем информацию о файле
        auto file_status = std::filesystem::status(path);
        auto file_size = std::filesystem::file_size(path);
        auto last_write_time = std::filesystem::last_write_time(path);
        
        metadata.size = file_size;
        
        // Преобразуем время модификации из std::filesystem::file_time_type в time_t
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            last_write_time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
        metadata.modification_time = std::chrono::system_clock::to_time_t(sctp);
        
        // Дополнительно можно получить время создания и доступа,
        // но это зависит от платформы и может быть недоступно
        metadata.creation_time = metadata.modification_time;
        metadata.access_time = metadata.modification_time;
        
        // Владельца и права доступа тоже зависят от платформы
        metadata.owner = "Unknown";
        metadata.permissions = "Unknown";
        
        // MIME-тип требует дополнительных библиотек или системных вызовов
        metadata.mime_type = "application/octet-stream";
        
        // По умолчанию файл не зашифрован
        metadata.is_encrypted = false;
        
        // Хеш файла (для небольших файлов)
        if (file_size <= 10 * 1024 * 1024) { // 10 МБ
            metadata.hash = calculateFileHash(file_path);
        }
        
        return true;
    }
    catch (const std::exception& e) {
        LogError("Error extracting metadata: " + std::string(e.what()));
        return false;
    }
}

std::string FileScannerImpl::calculateFileHash(const std::string& file_path) {
    try {
        // Используем MD5 для быстрого хеширования (для демонстрации)
        // В реальном приложении может использоваться более надежный алгоритм
        std::ifstream file(file_path, std::ios::binary);
        if (!file) {
            return "";
        }
        
        MD5_CTX md5Context;
        MD5_Init(&md5Context);
        
        char buffer[1024];
        while (file.read(buffer, sizeof(buffer))) {
            MD5_Update(&md5Context, buffer, file.gcount());
        }
        
        if (file.gcount() > 0) {
            MD5_Update(&md5Context, buffer, file.gcount());
        }
        
        unsigned char digest[MD5_DIGEST_LENGTH];
        MD5_Final(digest, &md5Context);
        
        // Преобразуем бинарный хеш в шестнадцатеричную строку
        std::stringstream ss;
        for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
        }
        
        return ss.str();
    }
    catch (const std::exception& e) {
        LogError("Error calculating file hash: " + std::string(e.what()));
        return "";
    }
}

bool FileScannerImpl::matchesFilters(const FileMetadata& metadata) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Проверяем размер файла
    if (metadata.size < min_size_ || metadata.size > max_size_) {
        return false;
    }
    
    // Проверяем время модификации
    if (metadata.modification_time < from_time_ || metadata.modification_time > to_time_) {
        return false;
    }
    
    // Проверяем расширение файла
    if (!extension_filter_.empty()) {
        // Если список расширений не пустой, проверяем, входит ли расширение файла в этот список
        if (extension_filter_.find(metadata.extension) == extension_filter_.end()) {
            return false;
        }
    }
    
    return true;
}

} // namespace mceas