#include "internal/comm/protocols/http_protocol.h"
#include "internal/logging.h"
#include <curl/curl.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <thread>
#include <algorithm>
#include <random>
#include <zlib.h>
#include "external/json/json.hpp"

namespace mceas {
namespace comm {

// Константы для HTTP протокола
namespace {
    // Максимальное количество повторных попыток
    constexpr int MAX_RETRIES = 5;
    
    // Базовое время ожидания между повторами (в миллисекундах)
    constexpr int BASE_RETRY_DELAY_MS = 1000;
    
    // Максимальное время ожидания между повторами (в миллисекундах)
    constexpr int MAX_RETRY_DELAY_MS = 30000;
    
    // Размер буфера для сжатия
    constexpr size_t COMPRESSION_BUFFER_SIZE = 16384;
    
    // Минимальный размер данных для сжатия (в байтах)
    constexpr size_t MIN_COMPRESSION_SIZE = 1024;
    
    // Уровень сжатия (1-9, где 9 - максимальное)
    constexpr int COMPRESSION_LEVEL = 6;
    
    // Максимальный размер чанка для отправки (в байтах)
    constexpr size_t MAX_CHUNK_SIZE = 1024 * 1024; // 1MB
}

// Callback для curl для записи ответа
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    std::vector<uint8_t>* mem = static_cast<std::vector<uint8_t>*>(userp);
    size_t prev_size = mem->size();
    mem->resize(prev_size + realsize);
    std::memcpy(&(*mem)[prev_size], contents, realsize);
    return realsize;
}

// Callback для curl для записи заголовков
static size_t HeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t realsize = size * nitems;
    std::map<std::string, std::string>* headers = 
        static_cast<std::map<std::string, std::string>*>(userdata);
    
    std::string header(buffer, realsize);
    
    // Удаляем перенос строки в конце
    if (!header.empty() && header[header.size() - 1] == '\n') {
        header.erase(header.size() - 1);
    }
    if (!header.empty() && header[header.size() - 1] == '\r') {
        header.erase(header.size() - 1);
    }
    
    // Пропускаем пустые строки и HTTP статус
    if (header.empty() || header.substr(0, 4) == "HTTP") {
        return realsize;
    }
    
    // Ищем разделитель заголовка
    size_t pos = header.find(": ");
    if (pos != std::string::npos) {
        std::string name = header.substr(0, pos);
        std::string value = header.substr(pos + 2);
        (*headers)[name] = value;
    }
    
    return realsize;
}

// Сжатие данных с использованием zlib
static std::vector<uint8_t> compressData(const std::vector<uint8_t>& data) {
    if (data.size() < MIN_COMPRESSION_SIZE) {
        // Не сжимаем маленькие данные
        return data;
    }
    
    std::vector<uint8_t> compressed;
    compressed.resize(COMPRESSION_BUFFER_SIZE);
    
    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));
    
    if (deflateInit2(&zs, COMPRESSION_LEVEL, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        LogError("Failed to initialize zlib for compression");
        return data;
    }
    
    zs.next_in = const_cast<Bytef*>(data.data());
    zs.avail_in = static_cast<uInt>(data.size());
    
    int ret;
    size_t total_size = 0;
    
    do {
        if (compressed.size() < total_size + COMPRESSION_BUFFER_SIZE) {
            compressed.resize(compressed.size() + COMPRESSION_BUFFER_SIZE);
        }
        
        zs.next_out = compressed.data() + total_size;
        zs.avail_out = COMPRESSION_BUFFER_SIZE;
        
        ret = deflate(&zs, Z_FINISH);
        
        if (total_size + COMPRESSION_BUFFER_SIZE - zs.avail_out > compressed.size()) {
            compressed.resize(total_size + COMPRESSION_BUFFER_SIZE - zs.avail_out);
        }
        
        total_size += (COMPRESSION_BUFFER_SIZE - zs.avail_out);
    } while (ret == Z_OK);
    
    deflateEnd(&zs);
    
    if (ret != Z_STREAM_END) {
        LogError("Error during zlib compression");
        return data;
    }
    
    compressed.resize(total_size);
    
    // Проверяем, что сжатие действительно уменьшило размер
    if (compressed.size() >= data.size()) {
        LogDebug("Compression did not reduce data size, using original data");
        return data;
    }
    
    LogDebug("Compressed data from " + std::to_string(data.size()) + 
            " to " + std::to_string(compressed.size()) + " bytes");
    return compressed;
}

// Распаковка сжатых данных
static std::vector<uint8_t> decompressData(const std::vector<uint8_t>& compressed_data, 
                                          const std::string& content_encoding) {
    // Проверяем, нужно ли распаковывать
    if (content_encoding.empty() || 
        (content_encoding != "gzip" && content_encoding != "deflate")) {
        return compressed_data;
    }
    
    std::vector<uint8_t> decompressed;
    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));
    
    // Настраиваем параметры в зависимости от кодировки
    int window_bits = 15;
    if (content_encoding == "gzip") {
        window_bits += 16;
    }
    
    if (inflateInit2(&zs, window_bits) != Z_OK) {
        LogError("Failed to initialize zlib for decompression");
        return compressed_data;
    }
    
    zs.next_in = const_cast<Bytef*>(compressed_data.data());
    zs.avail_in = static_cast<uInt>(compressed_data.size());
    
    int ret;
    std::vector<uint8_t> buffer(COMPRESSION_BUFFER_SIZE);
    
    do {
        zs.next_out = buffer.data();
        zs.avail_out = static_cast<uInt>(buffer.size());
        
        ret = inflate(&zs, Z_NO_FLUSH);
        
        if (ret != Z_OK && ret != Z_STREAM_END) {
            LogError("Error during zlib decompression: " + std::to_string(ret));
            inflateEnd(&zs);
            return compressed_data;
        }
        
        decompressed.insert(decompressed.end(), buffer.begin(), 
                           buffer.begin() + (buffer.size() - zs.avail_out));
    } while (ret != Z_STREAM_END);
    
    inflateEnd(&zs);
    
    LogDebug("Decompressed data from " + std::to_string(compressed_data.size()) + 
            " to " + std::to_string(decompressed.size()) + " bytes");
    return decompressed;
}

// Вычисление времени задержки для повторной попытки с экспоненциальным откатом
static int calculateRetryDelayMs(int attempt) {
    // Экспоненциальный откат с небольшой случайностью
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> jitter(-200, 200);
    
    int delay = BASE_RETRY_DELAY_MS * (1 << attempt) + jitter(gen);
    return std::min(delay, MAX_RETRY_DELAY_MS);
}

// Возвращает строковое описание ошибки curl
static std::string curlErrorToString(CURLcode code) {
    return curl_easy_strerror(code);
}

// Класс для управления сессией curl
class CurlSession {
public:
    CurlSession() : curl_(nullptr) {
        curl_ = curl_easy_init();
    }
    
    ~CurlSession() {
        if (curl_) {
            curl_easy_cleanup(curl_);
        }
    }
    
    CURL* get() {
        return curl_;
    }
    
    bool isValid() const {
        return curl_ != nullptr;
    }
    
private:
    CURL* curl_;
};

HttpProtocol::HttpProtocol() 
    : state_(ConnectionState::DISCONNECTED), connection_(ConnectionParams()) {
    
    // Инициализация curl (глобально)
    curl_global_init(CURL_GLOBAL_ALL);
}

HttpProtocol::~HttpProtocol() noexcept {
    disconnect();
    
    // Очистка curl
    curl_global_cleanup();
}

bool HttpProtocol::connect(const ProtocolConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    config_ = config;
    
    // Формируем базовый URL
    std::string protocol = config.use_encryption ? "https" : "http";
    base_url_ = protocol + "://" + config.endpoint;
    
    if (config.port > 0) {
        base_url_ += ":" + std::to_string(config.port);
    }
    
    // Настраиваем параметры соединения
    ConnectionParams conn_params;
    conn_params.endpoint = config.endpoint;
    conn_params.port = config.port;
    conn_params.connect_timeout_ms = config.timeout_ms;
    conn_params.operation_timeout_ms = config.timeout_ms;
    conn_params.reconnect_attempts = config.retry_count;
    conn_params.reconnect_interval_ms = config.retry_interval_ms;
    conn_params.auto_reconnect = true;
    
    connection_.setParams(conn_params);
    
    LogInfo("HTTP protocol connecting to " + base_url_);
    
    // Проверяем соединение с пробным запросом
    std::string url = base_url_ + "/api/status";
    std::string response_body;
    std::map<std::string, std::string> response_headers;
    int status_code;
    
    bool result = sendHttpRequestWithRetry("GET", url, {}, {}, 
                                         response_body, response_headers, status_code);
    
    if (!result) {
        LogError("Failed to connect to server");
        state_ = ConnectionState::ERROR;
        return false;
    }
    
    if (status_code < 200 || status_code >= 300) {
        LogWarning("Server responded with status code: " + std::to_string(status_code));
        // Но не считаем это ошибкой соединения, если сервер отвечает
    }
    
    state_ = ConnectionState::CONNECTED;
    LogInfo("HTTP protocol connected successfully");
    return true;
}

bool HttpProtocol::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    LogInfo("HTTP protocol disconnecting");
    
    // Очищаем очередь сообщений
    while (!received_messages_.empty()) {
        received_messages_.pop();
    }
    
    state_ = ConnectionState::DISCONNECTED;
    connection_.disconnect();
    
    return true;
}

ConnectionState HttpProtocol::getConnectionState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

bool HttpProtocol::send(const Message& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Проверяем состояние соединения
    if (state_ != ConnectionState::CONNECTED) {
        LogWarning("Cannot send message: not connected");
        return false;
    }
    
    try {
        // Формируем URL для сообщения
        std::string url = getUrlForMessage(message);
        
        // Формируем заголовки
        std::map<std::string, std::string> headers = getHeadersForMessage(message);
        
        // Сериализуем сообщение
        std::vector<uint8_t> body = message.serialize();
        
        // Проверяем, нужно ли разбивать сообщение на части
        if (body.size() > MAX_CHUNK_SIZE) {
            return sendChunkedMessage(message, MAX_CHUNK_SIZE);
        }
        
        // Если нужно, сжимаем данные
        if (body.size() >= MIN_COMPRESSION_SIZE) {
            std::vector<uint8_t> compressed_body = compressData(body);
            if (compressed_body.size() < body.size()) {
                body = compressed_body;
                headers["Content-Encoding"] = "gzip";
            }
        }
        
        // Добавляем информацию о размере
        headers["Content-Length"] = std::to_string(body.size());
        
        // Отправляем POST-запрос
        std::string response_body;
        std::map<std::string, std::string> response_headers;
        int status_code;
        
        bool result = sendHttpRequestWithRetry("POST", url, headers, body, 
                                             response_body, response_headers, status_code);
        
        if (!result || (status_code < 200 || status_code >= 300)) {
            LogError("Failed to send message, status code: " + std::to_string(status_code));
            return false;
        }
        
        // Если в ответе есть тело, обрабатываем его
        if (!response_body.empty()) {
            try {
                // Проверяем, сжаты ли данные
                std::string content_encoding;
                auto it = response_headers.find("Content-Encoding");
                if (it != response_headers.end()) {
                    content_encoding = it->second;
                }
                
                // Преобразуем строку в вектор байт
                std::vector<uint8_t> response_data(response_body.begin(), response_body.end());
                
                // Если нужно, распаковываем данные
                if (!content_encoding.empty()) {
                    response_data = decompressData(response_data, content_encoding);
                }
                
                // Парсим сообщение
                try {
                    Message response_message = Message::deserialize(response_data);
                    received_messages_.push(response_message);
                    
                    LogDebug("Received response, added to queue");
                }
                catch (const std::exception& e) {
                    LogWarning("Failed to parse response as message: " + std::string(e.what()));
                }
            }
            catch (const std::exception& e) {
                LogWarning("Error processing response: " + std::string(e.what()));
            }
        }
        
        return true;
    }
    catch (const std::exception& e) {
        LogError("Error sending message: " + std::string(e.what()));
        return false;
    }
}

std::optional<Message> HttpProtocol::receive() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Проверяем очередь полученных сообщений
    if (received_messages_.empty()) {
        return std::nullopt;
    }
    
    // Берем сообщение из очереди
    Message message = received_messages_.front();
    received_messages_.pop();
    
    return message;
}

std::optional<Message> HttpProtocol::sendAndWaitForResponse(const Message& message, int timeout_ms) {
    // Отправляем сообщение
    if (!send(message)) {
        return std::nullopt;
    }
    
    // Ждем ответа
    auto start_time = std::chrono::steady_clock::now();
    while (true) {
        // Проверяем таймаут
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count() > timeout_ms) {
            LogWarning("Timeout waiting for response");
            return std::nullopt;
        }
        
        // Пытаемся получить сообщение
        auto response = receive();
        if (response) {
            // Проверяем, что это ответ на наше сообщение
            if (response->headers.find("response_to") != response->headers.end() &&
                response->headers["response_to"] == message.message_id) {
                return response;
            }
            
            // Если это не ответ на наше сообщение, возвращаем его в очередь
            std::lock_guard<std::mutex> lock(mutex_);
            received_messages_.push(*response);
        }
        
        // Небольшая пауза для снижения нагрузки
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

bool HttpProtocol::sendHttpRequestWithRetry(const std::string& method, const std::string& url, 
                                           const std::map<std::string, std::string>& headers,
                                           const std::vector<uint8_t>& body,
                                           std::string& response_body,
                                           std::map<std::string, std::string>& response_headers,
                                           int& status_code) {
    // Счетчик попыток
    int attempts = 0;
    
    while (attempts <= MAX_RETRIES) {
        if (attempts > 0) {
            int delay = calculateRetryDelayMs(attempts - 1);
            LogInfo("Retry attempt " + std::to_string(attempts) + ", waiting " + 
                   std::to_string(delay) + "ms");
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }
        
        attempts++;
        
        // Отправляем запрос
        bool result = sendHttpRequest(method, url, headers, body, 
                                     response_body, response_headers, status_code);
        
        // Если запрос успешен, возвращаем результат
        if (result) {
            // Проверяем, нужно ли повторить запрос по статус-коду
            if (status_code >= 500 && status_code < 600) {
                // Серверная ошибка, повторяем запрос
                LogWarning("Server error: " + std::to_string(status_code) + ", retrying...");
                continue;
            }
            
            // Все остальные статусы считаем нормальными
            return true;
        }
        
        // Если ошибка не связана с сетью, прекращаем попытки
        if (last_request_.error.find("timed out") == std::string::npos &&
            last_request_.error.find("network") == std::string::npos &&
            last_request_.error.find("connection") == std::string::npos) {
            LogError("Non-network error, not retrying: " + last_request_.error);
            return false;
        }
    }
    
    LogError("Failed after " + std::to_string(MAX_RETRIES) + " retries");
    return false;
}

bool HttpProtocol::sendHttpRequest(const std::string& method, const std::string& url, 
                                 const std::map<std::string, std::string>& headers,
                                 const std::vector<uint8_t>& body,
                                 std::string& response_body,
                                 std::map<std::string, std::string>& response_headers,
                                 int& status_code) {
    // Создаем сессию curl
    CurlSession curl_session;
    if (!curl_session.isValid()) {
        LogError("Failed to initialize curl");
        last_request_.error = "Failed to initialize curl";
        return false;
    }
    
    CURL* curl = curl_session.get();
    
    // Буфер для ответа
    std::vector<uint8_t> response_buffer;
    
    // Обновляем информацию о последнем запросе
    last_request_.method = method;
    last_request_.url = url;
    last_request_.timestamp = std::chrono::system_clock::now();
    
    // Настройка curl
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, config_.timeout_ms);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, config_.timeout_ms / 2);
    
    // Настройка проверки сертификатов для HTTPS
    if (url.substr(0, 5) == "https") {
        if (config_.use_encryption) {
            // Проверяем сертификаты
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        } else {
            // Отключаем проверку сертификатов (не рекомендуется в продакшене)
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        }
    }
    
    // Настройка метода запроса
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
        if (!body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
        }
    }
    else if (method == "PUT") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
        if (!body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
        }
    }
    else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }
    else if (method != "GET") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
    }
    
    // Добавляем заголовки
    struct curl_slist* curl_headers = nullptr;
    for (const auto& header : headers) {
        std::string header_str = header.first + ": " + header.second;
        curl_headers = curl_slist_append(curl_headers, header_str.c_str());
    }
    
    // Добавляем заголовок Accept-Encoding для поддержки сжатия
    curl_headers = curl_slist_append(curl_headers, "Accept-Encoding: gzip, deflate");
    
    // Добавляем User-Agent для маскировки под обычный браузер
    std::string user_agent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36";
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
    
    if (curl_headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
    }
    
    // Выполняем запрос
    CURLcode res = curl_easy_perform(curl);
    
    // Получаем статус код
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    last_request_.status_code = status_code;
    
    // Освобождаем заголовки
    if (curl_headers) {
        curl_slist_free_all(curl_headers);
    }
    
    // Проверяем результат
    if (res != CURLE_OK) {
        last_request_.error = curlErrorToString(res);
        LogError("HTTP request failed: " + last_request_.error);
        return false;
    }
    
    // Проверяем, сжаты ли данные ответа
    std::string content_encoding;
    auto it = response_headers.find("Content-Encoding");
    if (it != response_headers.end()) {
        content_encoding = it->second;
    }
    
    // Если данные сжаты, распаковываем
    if (!content_encoding.empty() && !response_buffer.empty()) {
        std::vector<uint8_t> decompressed = decompressData(response_buffer, content_encoding);
        response_body = std::string(decompressed.begin(), decompressed.end());
    } else {
        // Преобразуем буфер в строку
        response_body = std::string(response_buffer.begin(), response_buffer.end());
    }
    
    return true;
}

std::string HttpProtocol::getUrlForMessage(const Message& message) {
    std::string url = base_url_;
    
    // Формируем URL в зависимости от типа сообщения
    switch (message.type) {
        case MessageType::DATA:
            url += "/api/data";
            break;
        case MessageType::COMMAND:
            url += "/api/commands";
            break;
        case MessageType::COMMAND_RESULT:
            url += "/api/command-results";
            break;
        case MessageType::HEARTBEAT:
            url += "/api/heartbeat";
            break;
        case MessageType::AUTH:
            url += "/api/auth";
            break;
        case MessageType::ERROR:
            url += "/api/errors";
            break;
        default:
            url += "/api/messages";
            break;
    }
    
    return url;
}

std::map<std::string, std::string> HttpProtocol::getHeadersForMessage(const Message& message) {
    std::map<std::string, std::string> headers = message.headers;
    
    // Добавляем стандартные заголовки
    headers["Content-Type"] = "application/octet-stream";
    headers["X-Agent-ID"] = message.agent_id;
    headers["X-Message-ID"] = message.message_id;
    headers["X-Message-Type"] = std::to_string(static_cast<int>(message.type));
    headers["X-Timestamp"] = message.timestamp;
    
    return headers;
}

bool HttpProtocol::sendChunkedMessage(const Message& message, size_t chunk_size) {
    LogInfo("Sending message in chunks, size: " + 
           std::to_string(message.payload.size()) + " bytes");
    
    // Генерируем уникальный ID для фрагментации
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(10000, 99999);
    std::string chunk_id = std::to_string(dis(gen));
    
    // Сериализуем сообщение
    std::vector<uint8_t> data = message.serialize();
    
    // Определяем, сколько чанков потребуется
    size_t total_chunks = (data.size() + chunk_size - 1) / chunk_size;
    
    LogDebug("Splitting message into " + std::to_string(total_chunks) + " chunks");
    
    // Разбиваем данные на части и отправляем
    for (size_t i = 0; i < total_chunks; i++) {
        // Формируем данные чанка
        size_t offset = i * chunk_size;
        size_t current_chunk_size = std::min(chunk_size, data.size() - offset);
        std::vector<uint8_t> chunk_data(data.begin() + offset, 
                                       data.begin() + offset + current_chunk_size);
        
        // Создаем сообщение для чанка
        Message chunk_message;
        chunk_message.type = message.type;
        chunk_message.agent_id = message.agent_id;
        chunk_message.message_id = message.message_id + "_chunk_" + 
                                  std::to_string(i + 1) + "_of_" + 
                                  std::to_string(total_chunks);
        chunk_message.timestamp = message.timestamp;
        
        // Добавляем заголовки для чанка
        chunk_message.headers = message.headers;
        chunk_message.headers["X-Chunk-ID"] = chunk_id;
        chunk_message.headers["X-Chunk-Index"] = std::to_string(i + 1);
        chunk_message.headers["X-Chunk-Total"] = std::to_string(total_chunks);
        chunk_message.headers["X-Original-Message-ID"] = message.message_id;
        
        // Устанавливаем данные чанка
        chunk_message.payload = chunk_data;
        
        // Формируем URL для чанка
        std::string url = getUrlForMessage(message);
        
        // Формируем заголовки
        std::map<std::string, std::string> headers = getHeadersForMessage(chunk_message);
        
        // Если нужно, сжимаем данные
        std::vector<uint8_t> body = chunk_data;
        if (body.size() >= MIN_COMPRESSION_SIZE) {
            std::vector<uint8_t> compressed_body = compressData(body);
            if (compressed_body.size() < body.size()) {
                body = compressed_body;
                headers["Content-Encoding"] = "gzip";
            }
        }
        
        // Отправляем POST-запрос
        std::string response_body;
        std::map<std::string, std::string> response_headers;
        int status_code;
        
        bool result = sendHttpRequestWithRetry("POST", url, headers, body, 
                                             response_body, response_headers, status_code);
        
        if (!result || (status_code < 200 || status_code >= 300)) {
            LogError("Failed to send chunk " + std::to_string(i + 1) + 
                    " of " + std::to_string(total_chunks) + 
                    ", status code: " + std::to_string(status_code));
            return false;
        }
        
        LogDebug("Sent chunk " + std::to_string(i + 1) + " of " + 
                std::to_string(total_chunks));
    }
    
    LogInfo("All chunks sent successfully");
    return true;
}

// Регистрация протокола в фабрике
namespace {
    struct HttpProtocolRegistrar {
        HttpProtocolRegistrar() {
            // Регистрация протокола в фабрике
            ProtocolFactory::registerProtocol("http", []() { 
                return std::make_unique<HttpProtocol>(); 
            });
            ProtocolFactory::registerProtocol("https", []() { 
                return std::make_unique<HttpProtocol>(); 
            });
            
            LogDebug("HTTP protocol registered");
        }
    };
    
    // Статический объект для вызова регистрации при загрузке
    static HttpProtocolRegistrar registrar;
}

} // namespace comm
} // namespace mceas