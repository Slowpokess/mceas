#include "internal/comm/protocols/http_protocol.h"
#include "internal/logging.h"
#include <curl/curl.h>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace mceas {
namespace comm {

// Callback-функция для curl для записи ответа
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    std::vector<uint8_t>* mem = (std::vector<uint8_t>*)userp;
    size_t prev_size = mem->size();
    mem->resize(prev_size + realsize);
    memcpy(&(*mem)[prev_size], contents, realsize);
    return realsize;
}

// Callback-функция для curl для записи заголовков
static size_t HeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t realsize = size * nitems;
    std::map<std::string, std::string>* headers = (std::map<std::string, std::string>*)userdata;
    
    std::string header(buffer, realsize);
    
    // Удаляем перенос строки в конце
    if (!header.empty() && header[header.size() - 1] == '\n') {
        header.erase(header.size() - 1);
    }
    if (!header.empty() && header[header.size() - 1] == '\r') {
        header.erase(header.size() - 1);
    }
    
    // Пропускаем пустые строки
    if (header.empty()) {
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

HttpProtocol::HttpProtocol() 
    : connection_(ConnectionParams()) {
    
    // Инициализация curl
    curl_global_init(CURL_GLOBAL_ALL);
}

HttpProtocol::~HttpProtocol() {
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
    conn_params.reconnect_attempts = config.retry_count;
    conn_params.reconnect_interval_ms = config.retry_interval_ms;
    conn_params.auto_reconnect = true;
    
    connection_.setParams(conn_params);
    
    LogInfo("HTTP protocol connecting to " + base_url_);
    
    return connection_.connect();
}

bool HttpProtocol::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    LogInfo("HTTP protocol disconnecting");
    
    // Очищаем очередь сообщений
    while (!received_messages_.empty()) {
        received_messages_.pop();
    }
    
    return connection_.disconnect();
}

ConnectionState HttpProtocol::getConnectionState() const {
    return connection_.getState();
}

bool HttpProtocol::send(const Message& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Проверяем состояние соединения
    if (connection_.getState() != ConnectionState::CONNECTED) {
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
        
        // Отправляем POST-запрос
        std::string response_body;
        std::map<std::string, std::string> response_headers;
        int status_code;
        
        bool result = sendHttpRequest("POST", url, headers, body, 
                                     response_body, response_headers, status_code);
        
        // Обновляем время последней активности
        connection_.updateLastActivity();
        
        if (!result || (status_code < 200 || status_code >= 300)) {
            LogError("Failed to send message, status code: " + std::to_string(status_code));
            return false;
        }
        
        // Если в ответе есть тело, парсим его как сообщение и добавляем в очередь
        if (!response_body.empty()) {
            try {
                std::vector<uint8_t> response_data(response_body.begin(), response_body.end());
                Message response_message = Message::deserialize(response_data);
                received_messages_.push(response_message);
                
                LogDebug("Received response, added to queue");
            }
            catch (const std::exception& e) {
                LogWarning("Failed to parse response as message: " + std::string(e.what()));
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
        
        // Небольшая пауза, чтобы не нагружать процессор
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

bool HttpProtocol::sendHttpRequest(const std::string& method, const std::string& url, 
                                 const std::map<std::string, std::string>& headers,
                                 const std::vector<uint8_t>& body,
                                 std::string& response_body,
                                 std::map<std::string, std::string>& response_headers,
                                 int& status_code) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        LogError("Failed to initialize curl");
        return false;
    }
    
    // Буфер для ответа
    std::vector<uint8_t> response_buffer;
    
    // Настраиваем curl
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, config_.timeout_ms);
    
    // Метод запроса
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
    }
    else if (method == "PUT") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
    }
    else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }
    else if (method != "GET") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
    }
    
    // Заголовки
    struct curl_slist* curl_headers = nullptr;
    for (const auto& header : headers) {
        std::string header_str = header.first + ": " + header.second;
        curl_headers = curl_slist_append(curl_headers, header_str.c_str());
    }
    
    if (curl_headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
    }
    
    // Выполняем запрос
    CURLcode res = curl_easy_perform(curl);
    
    // Получаем статус код
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    
    // Обновляем информацию о последнем запросе
    last_request_.method = method;
    last_request_.url = url;
    last_request_.status_code = status_code;
    last_request_.timestamp = std::chrono::system_clock::now();
    
    // Освобождаем заголовки
    if (curl_headers) {
        curl_slist_free_all(curl_headers);
    }
    
    // Освобождаем curl
    curl_easy_cleanup(curl);
    
    // Проверяем результат
    if (res != CURLE_OK) {
        last_request_.error = curl_easy_strerror(res);
        LogError("HTTP request failed: " + last_request_.error);
        return false;
    }
    
    // Преобразуем буфер в строку
    response_body = std::string(response_buffer.begin(), response_buffer.end());
    
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

// Регистрация протокола в фабрике
namespace {
    static bool registerHttpProtocol() {
        // Здесь будет код регистрации протокола
        return true;
    }
    
    static bool registered = registerHttpProtocol();
}

} // namespace comm
} // namespace mceas