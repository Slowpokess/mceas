#include "internal/file_telemetry/classifier.h"
#include "internal/logging.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace mceas {

FileClassifierImpl::FileClassifierImpl() {
    // Инициализация отображений
    initExtensionMap();
    initMimeTypeMap();
    initSensitiveExtensions();
}

FileClassification FileClassifierImpl::classifyByMetadata(const FileMetadata& metadata) {
    // Сначала проверяем расширение файла
    std::string extension = metadata.extension;
    
    // Преобразуем к нижнему регистру
    std::transform(extension.begin(), extension.end(), extension.begin(), 
                  [](unsigned char c){ return std::tolower(c); });
    
    // Ищем в отображении расширений
    auto it = extension_map_.find(extension);
    if (it != extension_map_.end()) {
        // Дополнительно проверяем, является ли это потенциально чувствительным файлом
        if (sensitive_extensions_.find(extension) != sensitive_extensions_.end()) {
            return FileClassification::SENSITIVE;
        }
        
        return it->second;
    }
    
    // Если не нашли по расширению, пробуем классифицировать по MIME-типу
    if (!metadata.mime_type.empty()) {
        auto mime_it = mime_type_map_.find(metadata.mime_type);
        if (mime_it != mime_type_map_.end()) {
            return mime_it->second;
        }
    }
    
    // Если не удалось классифицировать, возвращаем UNKNOWN
    return FileClassification::UNKNOWN;
}

FileClassification FileClassifierImpl::classifyByContent(const std::string& file_path) {
    // Анализ содержимого для определения типа
    // В текущей версии это просто заглушка, в реальном приложении
    // здесь может быть более сложная логика
    
    // Пробуем определить MIME-тип
    std::string mime_type = detectMimeType(file_path);
    if (!mime_type.empty()) {
        auto it = mime_type_map_.find(mime_type);
        if (it != mime_type_map_.end()) {
            return it->second;
        }
    }
    
    // Если не удалось определить по MIME-типу, пробуем анализировать содержимое
    return analyzeFileContent(file_path);
}

std::string FileClassifierImpl::getClassificationName(FileClassification classification) {
    switch (classification) {
        case FileClassification::DOCUMENT:
            return "Document";
        case FileClassification::SPREADSHEET:
            return "Spreadsheet";
        case FileClassification::PRESENTATION:
            return "Presentation";
        case FileClassification::SOURCE_CODE:
            return "Source Code";
        case FileClassification::DATABASE:
            return "Database";
        case FileClassification::ARCHIVE:
            return "Archive";
        case FileClassification::IMAGE:
            return "Image";
        case FileClassification::AUDIO:
            return "Audio";
        case FileClassification::VIDEO:
            return "Video";
        case FileClassification::EXECUTABLE:
            return "Executable";
        case FileClassification::CONFIGURATION:
            return "Configuration";
        case FileClassification::SENSITIVE:
            return "Sensitive";
        case FileClassification::UNKNOWN:
        default:
            return "Unknown";
    }
}

void FileClassifierImpl::initExtensionMap() {
    // Документы
    extension_map_[".doc"] = FileClassification::DOCUMENT;
    extension_map_[".docx"] = FileClassification::DOCUMENT;
    extension_map_[".pdf"] = FileClassification::DOCUMENT;
    extension_map_[".txt"] = FileClassification::DOCUMENT;
    extension_map_[".rtf"] = FileClassification::DOCUMENT;
    extension_map_[".odt"] = FileClassification::DOCUMENT;
    
    // Таблицы
    extension_map_[".xls"] = FileClassification::SPREADSHEET;
    extension_map_[".xlsx"] = FileClassification::SPREADSHEET;
    extension_map_[".csv"] = FileClassification::SPREADSHEET;
    extension_map_[".ods"] = FileClassification::SPREADSHEET;
    
    // Презентации
    extension_map_[".ppt"] = FileClassification::PRESENTATION;
    extension_map_[".pptx"] = FileClassification::PRESENTATION;
    extension_map_[".odp"] = FileClassification::PRESENTATION;
    
    // Исходный код
    extension_map_[".c"] = FileClassification::SOURCE_CODE;
    extension_map_[".cpp"] = FileClassification::SOURCE_CODE;
    extension_map_[".h"] = FileClassification::SOURCE_CODE;
    extension_map_[".hpp"] = FileClassification::SOURCE_CODE;
    extension_map_[".java"] = FileClassification::SOURCE_CODE;
    extension_map_[".py"] = FileClassification::SOURCE_CODE;
    extension_map_[".js"] = FileClassification::SOURCE_CODE;
    extension_map_[".html"] = FileClassification::SOURCE_CODE;
    extension_map_[".css"] = FileClassification::SOURCE_CODE;
    extension_map_[".php"] = FileClassification::SOURCE_CODE;
    extension_map_[".swift"] = FileClassification::SOURCE_CODE;
    extension_map_[".go"] = FileClassification::SOURCE_CODE;
    extension_map_[".rb"] = FileClassification::SOURCE_CODE;
    
    // Базы данных
    extension_map_[".db"] = FileClassification::DATABASE;
    extension_map_[".sqlite"] = FileClassification::DATABASE;
    extension_map_[".sqlite3"] = FileClassification::DATABASE;
    extension_map_[".mdb"] = FileClassification::DATABASE;
    extension_map_[".accdb"] = FileClassification::DATABASE;
    
    // Архивы
    extension_map_[".zip"] = FileClassification::ARCHIVE;
    extension_map_[".rar"] = FileClassification::ARCHIVE;
    extension_map_[".7z"] = FileClassification::ARCHIVE;
    extension_map_[".tar"] = FileClassification::ARCHIVE;
    extension_map_[".gz"] = FileClassification::ARCHIVE;
    extension_map_[".bz2"] = FileClassification::ARCHIVE;
    
    // Изображения
    extension_map_[".jpg"] = FileClassification::IMAGE;
    extension_map_[".jpeg"] = FileClassification::IMAGE;
    extension_map_[".png"] = FileClassification::IMAGE;
    extension_map_[".gif"] = FileClassification::IMAGE;
    extension_map_[".bmp"] = FileClassification::IMAGE;
    extension_map_[".svg"] = FileClassification::IMAGE;
    extension_map_[".tiff"] = FileClassification::IMAGE;
    extension_map_[".webp"] = FileClassification::IMAGE;
    
    // Аудио
    extension_map_[".mp3"] = FileClassification::AUDIO;
    extension_map_[".wav"] = FileClassification::AUDIO;
    extension_map_[".ogg"] = FileClassification::AUDIO;
    extension_map_[".flac"] = FileClassification::AUDIO;
    extension_map_[".aac"] = FileClassification::AUDIO;
    
    // Видео
    extension_map_[".mp4"] = FileClassification::VIDEO;
    extension_map_[".avi"] = FileClassification::VIDEO;
    extension_map_[".mkv"] = FileClassification::VIDEO;
    extension_map_[".mov"] = FileClassification::VIDEO;
    extension_map_[".wmv"] = FileClassification::VIDEO;
    extension_map_[".webm"] = FileClassification::VIDEO;
    
    // Исполняемые файлы
    extension_map_[".exe"] = FileClassification::EXECUTABLE;
    extension_map_[".dll"] = FileClassification::EXECUTABLE;
    extension_map_[".so"] = FileClassification::EXECUTABLE;
    extension_map_[".dylib"] = FileClassification::EXECUTABLE;
    extension_map_[".app"] = FileClassification::EXECUTABLE;
    extension_map_[".bin"] = FileClassification::EXECUTABLE;
    
    // Конфигурационные файлы
    extension_map_[".ini"] = FileClassification::CONFIGURATION;
    extension_map_[".xml"] = FileClassification::CONFIGURATION;
    extension_map_[".json"] = FileClassification::CONFIGURATION;
    extension_map_[".yml"] = FileClassification::CONFIGURATION;
    extension_map_[".yaml"] = FileClassification::CONFIGURATION;
    extension_map_[".config"] = FileClassification::CONFIGURATION;
    extension_map_[".conf"] = FileClassification::CONFIGURATION;
}

void FileClassifierImpl::initMimeTypeMap() {
    // Документы
    mime_type_map_["application/pdf"] = FileClassification::DOCUMENT;
    mime_type_map_["application/msword"] = FileClassification::DOCUMENT;
    mime_type_map_["application/vnd.openxmlformats-officedocument.wordprocessingml.document"] = FileClassification::DOCUMENT;
    mime_type_map_["text/plain"] = FileClassification::DOCUMENT;
    mime_type_map_["text/rtf"] = FileClassification::DOCUMENT;
    mime_type_map_["application/rtf"] = FileClassification::DOCUMENT;
    
    // Таблицы
    mime_type_map_["application/vnd.ms-excel"] = FileClassification::SPREADSHEET;
    mime_type_map_["application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"] = FileClassification::SPREADSHEET;
    mime_type_map_["text/csv"] = FileClassification::SPREADSHEET;
    
    // Презентации
    mime_type_map_["application/vnd.ms-powerpoint"] = FileClassification::PRESENTATION;
    mime_type_map_["application/vnd.openxmlformats-officedocument.presentationml.presentation"] = FileClassification::PRESENTATION;
    
    // Исходный код
    mime_type_map_["text/x-c"] = FileClassification::SOURCE_CODE;
    mime_type_map_["text/x-c++"] = FileClassification::SOURCE_CODE;
    mime_type_map_["text/x-java"] = FileClassification::SOURCE_CODE;
    mime_type_map_["text/x-python"] = FileClassification::SOURCE_CODE;
    mime_type_map_["text/javascript"] = FileClassification::SOURCE_CODE;
    mime_type_map_["text/html"] = FileClassification::SOURCE_CODE;
    mime_type_map_["text/css"] = FileClassification::SOURCE_CODE;
    
    // Базы данных
    mime_type_map_["application/x-sqlite3"] = FileClassification::DATABASE;
    
    // Архивы
    mime_type_map_["application/zip"] = FileClassification::ARCHIVE;
    mime_type_map_["application/x-rar-compressed"] = FileClassification::ARCHIVE;
    mime_type_map_["application/x-7z-compressed"] = FileClassification::ARCHIVE;
    mime_type_map_["application/x-tar"] = FileClassification::ARCHIVE;
    mime_type_map_["application/gzip"] = FileClassification::ARCHIVE;
    
    // Изображения
    mime_type_map_["image/jpeg"] = FileClassification::IMAGE;
    mime_type_map_["image/png"] = FileClassification::IMAGE;
    mime_type_map_["image/gif"] = FileClassification::IMAGE;
    mime_type_map_["image/bmp"] = FileClassification::IMAGE;
    mime_type_map_["image/svg+xml"] = FileClassification::IMAGE;
    mime_type_map_["image/tiff"] = FileClassification::IMAGE;
    mime_type_map_["image/webp"] = FileClassification::IMAGE;
    
    // Аудио
    mime_type_map_["audio/mpeg"] = FileClassification::AUDIO;
    mime_type_map_["audio/wav"] = FileClassification::AUDIO;
    mime_type_map_["audio/ogg"] = FileClassification::AUDIO;
    mime_type_map_["audio/flac"] = FileClassification::AUDIO;
    mime_type_map_["audio/aac"] = FileClassification::AUDIO;
    
    // Видео
    mime_type_map_["video/mp4"] = FileClassification::VIDEO;
    mime_type_map_["video/x-msvideo"] = FileClassification::VIDEO;
    mime_type_map_["video/x-matroska"] = FileClassification::VIDEO;
    mime_type_map_["video/quicktime"] = FileClassification::VIDEO;
    mime_type_map_["video/x-ms-wmv"] = FileClassification::VIDEO;
    mime_type_map_["video/webm"] = FileClassification::VIDEO;
    
    // Исполняемые файлы
    mime_type_map_["application/x-msdownload"] = FileClassification::EXECUTABLE;
    mime_type_map_["application/octet-stream"] = FileClassification::EXECUTABLE;
    
    // Конфигурационные файлы
    mime_type_map_["application/xml"] = FileClassification::CONFIGURATION;
    mime_type_map_["application/json"] = FileClassification::CONFIGURATION;
    mime_type_map_["text/xml"] = FileClassification::CONFIGURATION;
    mime_type_map_["text/yaml"] = FileClassification::CONFIGURATION;
}

void FileClassifierImpl::initSensitiveExtensions() {
    // Потенциально чувствительные типы файлов
    sensitive_extensions_.insert(".key");        // Ключи шифрования
    sensitive_extensions_.insert(".pem");        // Приватные ключи
    sensitive_extensions_.insert(".pfx");        // Сертификаты с приватными ключами
    sensitive_extensions_.insert(".p12");        // Сертификаты с приватными ключами
    sensitive_extensions_.insert(".der");        // Сертификаты
    sensitive_extensions_.insert(".crt");        // Сертификаты
    sensitive_extensions_.insert(".cer");        // Сертификаты
    sensitive_extensions_.insert(".csr");        // Запросы на подпись сертификата
    sensitive_extensions_.insert(".kdb");        // KeePass база паролей
    sensitive_extensions_.insert(".kdbx");       // KeePass 2 база паролей
    sensitive_extensions_.insert(".kwallet");    // KWallet файл
    sensitive_extensions_.insert(".wallet");     // Различные кошельки
    sensitive_extensions_.insert(".dat");        // Различные форматы данных
    sensitive_extensions_.insert(".psafe3");     // Password Safe файл
    sensitive_extensions_.insert(".gpg");        // GnuPG зашифрованный файл
    sensitive_extensions_.insert(".pgp");        // PGP зашифрованный файл
    sensitive_extensions_.insert(".asc");        // PGP ключи и зашифрованные файлы
    sensitive_extensions_.insert(".tc");         // TrueCrypt контейнер
    sensitive_extensions_.insert(".vc");         // VeraCrypt контейнер
    sensitive_extensions_.insert(".enc");        // Зашифрованные файлы
}

std::string FileClassifierImpl::detectMimeType(const std::string& file_path) {
    try {
        // Это очень простая реализация для демонстрации
        // В реальном приложении лучше использовать специализированные библиотеки
        // или системные вызовы для определения MIME-типа
        
        // Открываем файл и читаем первые байты для определения сигнатуры
        std::ifstream file(file_path, std::ios::binary);
        if (!file) {
            return "";
        }
        
        // Буфер для чтения первых байтов
        char buffer[16];
        file.read(buffer, sizeof(buffer));
        std::streamsize bytes_read = file.gcount();
        
        if (bytes_read >= 4) {
            // Проверяем сигнатуру PDF
            if (buffer[0] == '%' && buffer[1] == 'P' && buffer[2] == 'D' && buffer[3] == 'F') {
                return "application/pdf";
            }
            
            // Проверяем сигнатуру JPEG
            if (buffer[0] == (char)0xFF && buffer[1] == (char)0xD8 && buffer[2] == (char)0xFF) {
                return "image/jpeg";
            }
            
            // Проверяем сигнатуру PNG
            if (buffer[0] == (char)0x89 && buffer[1] == 'P' && buffer[2] == 'N' && buffer[3] == 'G') {
                return "image/png";
            }
            
            // Проверяем сигнатуру GIF
            if (buffer[0] == 'G' && buffer[1] == 'I' && buffer[2] == 'F' && buffer[3] == '8') {
                return "image/gif";
            }
            
            // Проверяем сигнатуру ZIP (также используется для DOCX, XLSX, PPTX и т.д.)
            if (buffer[0] == 'P' && buffer[1] == 'K' && (buffer[2] == 3 || buffer[2] == 5 || buffer[2] == 7) && 
                (buffer[3] == 4 || buffer[3] == 6 || buffer[3] == 8)) {
                return "application/zip";
            }
        }
        
        // По умолчанию возвращаем общий тип для бинарных данных
        return "application/octet-stream";
    }
    catch (const std::exception& e) {
        LogError("Error detecting MIME type: " + std::string(e.what()));
        return "";
    }
}

FileClassification FileClassifierImpl::analyzeFileContent(const std::string& file_path) {
    try {
        // Открываем файл
        std::ifstream file(file_path);
        if (!file) {
            return FileClassification::UNKNOWN;
        }
        
        // Читаем первые несколько строк
        std::vector<std::string> lines;
        std::string line;
        for (int i = 0; i < 10 && std::getline(file, line); i++) {
            lines.push_back(line);
        }
        
        // Пытаемся определить тип по содержимому
        
        // Проверяем на HTML
        for (const auto& l : lines) {
            if (l.find("<!DOCTYPE html>") != std::string::npos || 
                l.find("<html") != std::string::npos) {
                return FileClassification::SOURCE_CODE;
            }
        }
        
        // Проверяем на XML
        for (const auto& l : lines) {
            if (l.find("<?xml") != std::string::npos) {
                return FileClassification::CONFIGURATION;
            }
        }
        
        // Проверяем на JSON
        if (!lines.empty() && (lines[0].find("{") == 0 || lines[0].find("[") == 0)) {
            return FileClassification::CONFIGURATION;
        }
        
        // Проверяем на исходный код
        for (const auto& l : lines) {
            if (l.find("#include") != std::string::npos || 
                l.find("import ") != std::string::npos || 
                l.find("package ") != std::string::npos || 
                l.find("def ") != std::string::npos || 
                l.find("class ") != std::string::npos || 
                l.find("function ") != std::string::npos) {
                return FileClassification::SOURCE_CODE;
            }
        }
        
        // По умолчанию считаем текстовый файл документом
        return FileClassification::DOCUMENT;
    }
    catch (const std::exception& e) {
        LogError("Error analyzing file content: " + std::string(e.what()));
        return FileClassification::UNKNOWN;
    }
}

} // namespace mceas