#include "internal/comm/protocols/doh_protocol.h"
#include "internal/logging.h"
#include <curl/curl.h>
#include <sstream>
#include <random>
#include <iomanip>
#include <thread>
#include <algorithm>
#include "external/json/json.hpp"

namespace mceas {
namespace comm {

// Base32 кодирование для DNS
static const char base32_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

static std::string base32_encode(const std::vector<uint8_t>& data) {
    std::string ret;
    int i = 0, j = 0;
    uint8_t char_array_5[5], char_array_8[8];

    for (const auto& byte : data) {
        char_array_5[i++] = byte;
        if (i == 5) {
            char_array_8[0] = (char_array_5[0] & 0xf8) >> 3;
            char_array_8[1] = ((char_array_5[0] & 0x07) << 2) + ((char_array_5[1] & 0xc0) >> 6);
            char_array_8[2] = ((char_array_5[1] & 0x3e) >> 1);
            char_array_8[3] = ((char_array_5[1] & 0x01) << 4) + ((char_array_5[2] & 0xf0) >> 4);
            char_array_8[4] = ((char_array_5[2] & 0x0f) << 1) + ((char_array_5[3] & 0x80) >> 7);
            char_array_8[5] = ((char_array_5[3] & 0x7c) >> 2);
            char_array_8[6] = ((char_array_5[3] & 0x03) << 3) + ((char_array_5[4] & 0xe0) >> 5);
            char_array_8[7] = (char_array_5[4] & 0x1f);

            for (j = 0; j < 8; j++)
                ret += base32_chars[char_array_8[j]];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 5; j++)
            char_array_5[j] = 0;

        char_array_8[0] = (char_array_5[0] & 0xf8) >> 3;
        char_array_8[1] = ((char_array_5[0] & 0x07) << 2) + ((char_array_5[1] & 0xc0) >> 6);
        char_array_8[2] = ((char_array_5[1] & 0x3e) >> 1);
        char_array_8[3] = ((char_array_5[1] & 0x01) << 4) + ((char_array_5[2] & 0xf0) >> 4);
        char_array_8[4] = ((char_array_5[2] & 0x0f) << 1) + ((char_array_5[3] & 0x80) >> 7);
        char_array_8[5] = ((char_array_5[3] & 0x7c) >> 2);
        char_array_8[6] = ((char_array_5[3] & 0x03) << 3) + ((char_array_5[4] & 0xe0) >> 5);
        char_array_8[7] = (char_array_5[4] & 0x1f);

        for (j = 0; j < i + 1; j++)
            ret += base32_chars[char_array_8[j]];

        while ((i++ < 5))
            ret += '=';
    }

    return ret;
}

static std::vector<uint8_t> base32_decode(const std::string& encoded_string) {
    std::vector<uint8_t> ret;
    std::string::size_type in_len = encoded_string.size();
    std::string::size_type i = 0;
    std::string::size_type j = 0;
    int in_ = 0;
    uint8_t char_array_8[8], char_array_5[5];

    while (in_len-- && encoded_string[in_] != '=') {
        char_array_8[i++] = encoded_string[in_++];
        if (i == 8) {
            for (i = 0; i < 8; i++) {
                char_array_8[i] = std::string(base32_chars).find(char_array_8[i]);
            }

            char_array_5[0] = (char_array_8[0] << 3) + ((char_array_8[1] & 0x1C) >> 2);
            char_array_5[1] = ((char_array_8[1] & 0x03) << 6) + (char_array_8[2] << 1) + ((char_array_8[3] & 0x10) >> 4);
            char_array_5[2] = ((char_array_8[3] & 0x0F) << 4) + ((char_array_8[4] & 0x1E) >> 1);
            char_array_5[3] = ((char_array_8[4] & 0x01) << 7) + (char_array_8[5] << 2) + ((char_array_8[6] & 0x18) >> 3);
            char_array_5[4] = ((char_array_8[6] & 0x07) << 5) + char_array_8[7];

            for (i = 0; i < 5; i++)
                ret.push_back(char_array_5[i]);
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 8; j++)
            char_array_8[j] = 0;

        for (j = 0; j < i; j++)
            char_array_8[j] = std::string(base32_chars).find(char_array_8[j]);

        char_array_5[0] = (char_array_8[0] << 3) + ((char_array_8[1] & 0x1C) >> 2);
        char_array_5[1] = ((char_array_8[1] & 0x03) << 6) + (char_array_8[2] << 1) + ((char_array_8[3] & 0x10) >> 4);
        char_array_5[2] = ((char_array_8[3] & 0x0F) << 4) + ((char_array_8[4] & 0x1E) >> 1);
        char_array_5[3] = ((char_array_8[4] & 0x01) << 7) + (char_array_8[5] << 2) + ((char_array_8[6] & 0x18) >> 3);
        char_array_5[4] = ((char_array_8[6] & 0x07) << 5) + char_array_8[7];

        for (j = 0; j < i - 1; j++)
            ret.push_back(char_array_5[j]);
    }

    return ret;
}

// Callback для curl
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    std::string* mem = static_cast<std::string*>(userp);
    mem->append(static_cast<char*>(contents), realsize);
    return realsize;
}

DoHProtocol::DoHProtocol() : state_(ConnectionState::DISCONNECTED) {
}

DoHProtocol::~DoHProtocol() noexcept {
    disconnect();
}

bool DoHProtocol::connect(const ProtocolConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Проверяем тип конфигурации
    if (auto doh_config = dynamic_cast<const DoHProtocolConfig*>(&config)) {
        config_ = *doh_config;
    } else {
        // Создаем конфигурацию по умолчанию с базовыми параметрами
        config_ = DoHProtocolConfig();
        config_.endpoint = config.endpoint;
        config_.port = config.port;
        config_.timeout_ms = config.timeout_ms;
        config_.retry_count = config.retry_count;
        config_.retry_interval_ms = config.retry_interval_ms;
    }
    
    LogInfo("DoH protocol connecting to DNS server: " + config_.dns_server);
    
    // Тестовый запрос для проверки соединения
    std::string response;
    if (!sendDnsQuery("test." + config_.domain_template.substr(config_.domain_template.find('.') + 1), response)) {
        LogError("Failed to connect to DNS server");
        state_ = ConnectionState::ERROR;
        return false;
    }
    
    state_ = ConnectionState::CONNECTED;
    LogInfo("DoH protocol connected successfully");
    return true;
}

bool DoHProtocol::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Очищаем очереди и буферы
    while (!received_messages_.empty()) {
        received_messages_.pop();
    }
    
    fragments_.clear();
    
    state_ = ConnectionState::DISCONNECTED;
    LogInfo("DoH protocol disconnected");
    return true;
}

ConnectionState DoHProtocol::getConnectionState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

bool DoHProtocol::send(const Message& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ != ConnectionState::CONNECTED) {
        LogWarning("Cannot send message: DoH protocol not connected");
        return false;
    }
    
    try {
        // Сериализуем сообщение
        std::vector<uint8_t> data = message.serialize();
        
        // Отправляем сообщение, разбивая на части, если нужно
        return sendChunkedMessage(data);
    }
    catch (const std::exception& e) {
        LogError("Error sending message via DoH: " + std::string(e.what()));
        return false;
    }
}

std::optional<Message> DoHProtocol::receive() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ != ConnectionState::CONNECTED) {
        return std::nullopt;
    }
    
    // Проверяем очередь сообщений
    if (!received_messages_.empty()) {
        Message message = received_messages_.front();
        received_messages_.pop();
        return message;
    }
    
    // Пытаемся получить новое сообщение
    auto data = receiveChunkedMessage();
    if (!data) {
        return std::nullopt;
    }
    
    // Десериализуем сообщение
    try {
        Message message = Message::deserialize(*data);
        return message;
    }
    catch (const std::exception& e) {
        LogError("Error deserializing received message: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::optional<Message> DoHProtocol::sendAndWaitForResponse(const Message& message, int timeout_ms) {
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
            LogWarning("Timeout waiting for response via DoH");
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
        
        // Небольшая пауза
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

std::string DoHProtocol::encodePayloadForDns(const std::vector<uint8_t>& payload) {
    // Базовое кодирование для передачи бинарных данных через DNS
    return base32_encode(payload);
}

std::vector<uint8_t> DoHProtocol::decodePayloadFromDns(const std::string& encoded) {
    // Декодирование данных из DNS-ответа
    return base32_decode(encoded);
}

bool DoHProtocol::sendDnsQuery(const std::string& query_domain, std::string& response) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        LogError("Failed to initialize curl for DoH query");
        return false;
    }
    
    // Формируем URL для DNS-over-HTTPS запроса
    std::string url = "https://" + config_.dns_server + "/dns-query";
    
    // Формируем параметры запроса
    std::string params = "?name=" + query_domain + "&type=TXT";
    
    // Буфер для ответа
    std::string response_buffer;
    
    // Настраиваем curl
    curl_easy_setopt(curl, CURLOPT_URL, (url + params).c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, config_.timeout_ms);
    
    // Добавляем заголовок accept для DoH
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "accept: application/dns-json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Выполняем запрос
    CURLcode res = curl_easy_perform(curl);
    
    // Освобождаем заголовки
    curl_slist_free_all(headers);
    
    // Проверяем результат
    if (res != CURLE_OK) {
        LogError("DoH query failed: " + std::string(curl_easy_strerror(res)));
        curl_easy_cleanup(curl);
        return false;
    }
    
    // Получаем статус код
    long status_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    
    // Освобождаем curl
    curl_easy_cleanup(curl);
    
    // Проверяем статус код
    if (status_code != 200) {
        LogError("DoH query failed with status code: " + std::to_string(status_code));
        return false;
    }
    
    // Парсим JSON-ответ
    try {
        nlohmann::json j = nlohmann::json::parse(response_buffer);
        
        // Проверяем наличие ответа
        if (j.contains("Answer") && j["Answer"].is_array() && !j["Answer"].empty()) {
            for (const auto& answer : j["Answer"]) {
                if (answer.contains("type") && answer["type"] == 16 && answer.contains("data")) {
                    // Тип 16 - это TXT запись
                    std::string data = answer["data"];
                    
                    // Удаляем кавычки, если они есть
                    if (data.size() >= 2 && data.front() == '"' && data.back() == '"') {
                        data = data.substr(1, data.size() - 2);
                    }
                    
                    response = data;
                    return true;
                }
            }
        }
        
        LogWarning("No TXT records found in DoH response");
        return false;
    }
    catch (const std::exception& e) {
        LogError("Error parsing DoH response: " + std::string(e.what()));
        return false;
    }
}

bool DoHProtocol::sendChunkedMessage(const std::vector<uint8_t>& data) {
    // Генерируем случайный ID для фрагментации
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    std::string chunk_id = std::to_string(dis(gen));
    
    // Определяем, сколько чанков потребуется
    int chunk_size = config_.chunk_size;
    int total_chunks = (data.size() + chunk_size - 1) / chunk_size;
    
    LogDebug("Splitting message into " + std::to_string(total_chunks) + " chunks");
    
    // Разбиваем данные на части и отправляем
    for (int i = 0; i < total_chunks; i++) {
        // Формируем данные чанка
        int offset = i * chunk_size;
        int current_chunk_size = std::min(chunk_size, static_cast<int>(data.size()) - offset);
        std::vector<uint8_t> chunk_data(data.begin() + offset, data.begin() + offset + current_chunk_size);
        
        // Формируем метаданные чанка
        nlohmann::json chunk_meta;
        chunk_meta["id"] = chunk_id;
        chunk_meta["chunk"] = i + 1;
        chunk_meta["total"] = total_chunks;
        
        // Сериализуем метаданные
        std::string meta_str = chunk_meta.dump();
        std::vector<uint8_t> meta_data(meta_str.begin(), meta_str.end());
        
        // Объединяем метаданные и данные
        std::vector<uint8_t> combined_data;
        combined_data.insert(combined_data.end(), meta_data.begin(), meta_data.end());
        combined_data.push_back(0); // Разделитель
        combined_data.insert(combined_data.end(), chunk_data.begin(), chunk_data.end());
        
        // Кодируем для DNS
        std::string encoded = encodePayloadForDns(combined_data);
        
        // Формируем домен запроса
        std::string query_domain = chunk_id + "-" + std::to_string(i + 1) + "." + 
                                  config_.domain_template.substr(config_.domain_template.find("%s") + 2);
        
        // Отправляем запрос
        std::string response;
        if (!sendDnsQuery(query_domain, response)) {
            LogError("Failed to send chunk " + std::to_string(i + 1) + " of " + std::to_string(total_chunks));
            return false;
        }
        
        // Добавляем небольшую задержку между запросами, чтобы не перегружать DNS
        if (i < total_chunks - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    
    LogDebug("All chunks sent successfully");
    return true;
}

std::optional<std::vector<uint8_t>> DoHProtocol::receiveChunkedMessage() {
    // Временно заглушка - в реальном приложении здесь будет логика опроса DNS на наличие новых сообщений
    // и объединение фрагментов в полное сообщение
    return std::nullopt;
}

// Регистрация протокола в фабрике
namespace {
    static bool registerDoHProtocol() {
        // Здесь будет код регистрации протокола
        return true;
    }
    
    static bool registered = registerDoHProtocol();
}

} // namespace comm
} // namespace mceas