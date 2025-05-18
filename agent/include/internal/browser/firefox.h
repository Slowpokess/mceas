#ifndef MCEAS_BROWSER_FIREFOX_H
#define MCEAS_BROWSER_FIREFOX_H

#include "browser/browser_module.h"
#include "browser/common.h"

namespace mceas {

// Анализатор для браузера Firefox
class FirefoxAnalyzer : public BrowserAnalyzer {
public:
    FirefoxAnalyzer();
    
    // Реализация методов интерфейса BrowserAnalyzer
    bool isInstalled() override;
    BrowserType getType() override;
    std::string getName() override;
    std::string getVersion() override;
    std::string getExecutablePath() override;
    std::string getProfilePath() override;
    std::map<std::string, std::string> collectData() override;
    
private:
    // Методы анализа данных Firefox
    std::vector<Bookmark> getBookmarks();
    std::vector<HistoryEntry> getHistory(int limit = 100);
    std::vector<Extension> getExtensions();
    
    // Кэшированные данные
    std::string version_;
    std::string executable_path_;
    std::string profile_path_;
    bool is_installed_;
};

} // namespace mceas

#endif // MCEAS_BROWSER_FIREFOX_H