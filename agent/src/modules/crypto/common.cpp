#include "internal/crypto/common.h"
#include "internal/logging.h"
#include "internal/utils/platform_utils.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <array>
#include <memory>
#include <sstream>
#include <set>
#include <algorithm>
#include <regex>

namespace mceas {
namespace crypto {

bool isMetamaskInstalledChrome() {
    // ID расширения Metamask для Chrome
    const std::string metamask_id = "nkbihfbeogaeaoehlefnkodbefgpgknn";
    return utils::IsBrowserExtensionInstalled("Chrome", metamask_id);
}

bool isMetamaskInstalledFirefox() {
    // ID расширения Metamask для Firefox отличается
    // Это заглушка, так как нужно определить ID для Firefox
    LogInfo("Metamask detection in Firefox is not fully implemented yet");
    return false;
}

// Небольшая часть словаря BIP-39 для демонстрации
// В реальном приложении здесь должен быть полный словарь
std::set<std::string> getBIP39WordList() {
    return {
        "abandon", "ability", "able", "about", "above", "absent", "absorb", "abstract", "absurd", "abuse",
        "access", "accident", "account", "accuse", "achieve", "acid", "acoustic", "acquire", "across", "act",
        "action", "actor", "actress", "actual", "adapt", "add", "addict", "address", "adjust", "admit",
        "adult", "advance", "advice", "aerobic", "affair", "afford", "afraid", "again", "age", "agent",
        "agree", "ahead", "aim", "air", "airport", "aisle", "alarm", "album", "alcohol", "alert",
        "alien", "all", "alley", "allow", "almost", "alone", "alpha", "already", "also", "alter",
        "always", "amateur", "amazing", "among", "amount", "amused", "analyst", "anchor", "ancient", "anger",
        "angle", "angry", "animal", "ankle", "announce", "annual", "another", "answer", "antenna", "antique",
        "anxiety", "any", "apart", "apology", "appear", "apple", "approve", "april", "arch", "arctic"
        // В реальном приложении здесь должен быть полный словарь из 2048 слов
    };
}

std::vector<SeedPhraseInfo> findPotentialSeedPhrases(const std::string& directory_path, bool mask_phrases) {
    std::vector<SeedPhraseInfo> results;
    
    if (directory_path.empty() || !utils::FileExists(directory_path)) {
        LogWarning("Invalid directory for seed phrase search: " + directory_path);
        return results;
    }
    
    try {
        LogInfo("Searching for potential seed phrases in: " + directory_path);
        
        // Получаем словарь BIP-39
        auto bip39_words = getBIP39WordList();
        
        // Обходим директорию рекурсивно
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory_path)) {
            if (entry.is_regular_file()) {
                // Проверяем только текстовые файлы и документы
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                
                if (ext == ".txt" || ext == ".md" || ext == ".rtf" || ext == ".csv" || 
                    ext == ".doc" || ext == ".docx" || ext == ".pdf" || ext == ".json") {
                    
                    // Пытаемся открыть файл
                    std::ifstream file(entry.path());
                    if (file.is_open()) {
                        std::string line;
                        int line_number = 0;
                        
                        while (std::getline(file, line)) {
                            line_number++;
                            
                            // Проверяем, может ли строка быть seed-фразой
                            std::string type;
                            double confidence;
                            if (isPotentialSeedPhrase(line, type, confidence)) {
                                SeedPhraseInfo info;
                                info.file_path = entry.path().string();
                                info.content = mask_phrases ? maskSeedPhrase(line) : line;
                                info.type = type;
                                info.word_count = std::count(line.begin(), line.end(), ' ') + 1;
                                info.confidence = confidence;
                                
                                results.push_back(info);
                                
                                LogInfo("Found potential seed phrase in: " + entry.path().string() + 
                                         " (line " + std::to_string(line_number) + ")");
                            }
                        }
                        
                        file.close();
                    }
                }
            }
        }
        
        LogInfo("Seed phrase search completed. Found " + std::to_string(results.size()) + " potential phrases");
    }
    catch (const std::exception& e) {
        LogError("Error during seed phrase search: " + std::string(e.what()));
    }
    
    return results;
}

bool isPotentialSeedPhrase(const std::string& text, std::string& type, double& confidence) {
    confidence = 0.0;
    type = "Unknown";
    
    // Если строка пустая или слишком короткая, это не seed-фраза
    if (text.empty() || text.length() < 20) {
        return false;
    }
    
    // Разбиваем текст на слова
    std::vector<std::string> words;
    std::istringstream iss(text);
    std::string word;
    while (iss >> word) {
        // Нормализуем слово (удаляем знаки препинания, переводим в нижний регистр)
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
        words.push_back(word);
    }
    
    // Проверяем количество слов (обычно 12, 18 или 24 для BIP-39)
    int word_count = words.size();
    if (word_count != 12 && word_count != 18 && word_count != 24) {
        return false;
    }
    
    // Получаем словарь BIP-39
    auto bip39_dict = getBIP39WordList();
    
    // Считаем, сколько слов есть в словаре BIP-39
    int bip39_matches = 0;
    for (const auto& word : words) {
        if (bip39_dict.find(word) != bip39_dict.end()) {
            bip39_matches++;
        }
    }
    
    // Рассчитываем уверенность на основе процента совпадений
    double match_percentage = static_cast<double>(bip39_matches) / word_count;
    
    // Если больше 80% слов совпадают со словарем, вероятно это BIP-39 seed-фраза
    if (match_percentage > 0.8) {
        type = "BIP-39";
        confidence = match_percentage;
        return true;
    }
    
    // Проверка на TON seed-фразу (они также используют BIP-39, но могут иметь другие закономерности)
    // Здесь можно добавить дополнительные проверки для TON
    
    // Проверка на Solana seed-фразу
    // Здесь можно добавить дополнительные проверки для Solana
    
    return false;
}

std::string maskSeedPhrase(const std::string& phrase) {
    // Маскируем фразу для безопасного хранения в логах
    // Отображаем только первые и последние 2 символа каждого слова
    std::istringstream iss(phrase);
    std::string word;
    std::string masked_phrase;
    
    while (iss >> word) {
        if (!masked_phrase.empty()) {
            masked_phrase += " ";
        }
        
        if (word.length() <= 4) {
            // Маскируем слово целиком
            masked_phrase += std::string(word.length(), '*');
        } else {
            // Оставляем первые 2 и последние 2 символа
            masked_phrase += word.substr(0, 2) + std::string(word.length() - 4, '*') + word.substr(word.length() - 2);
        }
    }
    
    return masked_phrase;
}

} // namespace crypto
} // namespace mceas