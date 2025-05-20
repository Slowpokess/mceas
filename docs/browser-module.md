# Модуль анализа браузеров

## Обзор

Модуль анализа браузеров позволяет обнаруживать установленные браузеры, собирать информацию о них, включая закладки, историю и установленные расширения.

## Архитектура

Модуль построен на основе интерфейса `BrowserAnalyzer`, который определяет общие методы для всех анализаторов браузеров:

```cpp
class BrowserAnalyzer {
public:
    virtual bool isInstalled() = 0;
    virtual BrowserType getType() = 0;
    virtual std::string getName() = 0;
    virtual std::string getVersion() = 0;
    virtual std::string getExecutablePath() = 0;
    virtual std::string getProfilePath() = 0;
    virtual std::map<std::string, std::string> collectData() = 0;
};

Для каждого поддерживаемого браузера реализован специализированный класс:

ChromeAnalyzer для Google Chrome
FirefoxAnalyzer для Mozilla Firefox
EdgeAnalyzer для Microsoft Edge

Принцип работы

При инициализации BrowserModule создает экземпляры всех анализаторов
Метод detectBrowsers() проверяет, какие браузеры установлены
Метод onExecute() собирает данные со всех обнаруженных браузеров
Для каждого браузера вызывается метод collectData(), который возвращает данные

Собираемые данные

Основная информация: версия, путь к профилю, путь к исполняемому файлу
Закладки: заголовок, URL, дата добавления, папка
История: заголовок, URL, дата посещения, количество посещений
Расширения: ID, название, версия, описание, статус

Кроссплатформенность
Модуль использует platform_utils.h для обеспечения совместимости с:

Windows
macOS
Linux

Данные хранятся в разных форматах:

Chrome и Edge: JSON для закладок, SQLite для истории
Firefox: SQLite для закладок и истории