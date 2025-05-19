#ifndef MCEAS_BROWSER_CHROME_H
#define MCEAS_BROWSER_CHROME_H

#include "browser/browser_module.h"
#include <string>
#include <map>

namespace mceas {

class ChromeAnalyzer : public BrowserAnalyzer {
public:
    ChromeAnalyzer();
    
    // Реализация методов интерфейса BrowserAnalyzer
    bool isInstalled() override;
    BrowserType getType() override;
    std::string getName() override;
    std::string getVersion() override;
    std::string getExecutablePath() override;
    std::string getProfilePath() override;
    std::map<std::string, std::string> collectData() override;
    
private:
    // Кэшированные данные
    std::string version_;
    std::string executable_path_;
    std::string profile_path_;
    bool is_installed_;
};

} // namespace mceas

#endif // MCEAS_BROWSER_CHROME_H