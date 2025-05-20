# MCEAS - Описание модулей

## Введение

Система MCEAS построена на модульной архитектуре, где каждый модуль выполняет специфическую функцию сбора и анализа данных. Все модули реализуют общий интерфейс `Module` и наследуются от базового класса `ModuleBase`, который предоставляет общую функциональность.

## Общий интерфейс модулей

```cpp
class Module {
public:
    virtual ~Module() = default;
    virtual bool initialize() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual ModuleResult execute() = 0;
    virtual bool handleCommand(const Command& command) = 0;
    virtual std::string getName() const = 0;
    virtual ModuleType getType() const = 0;
};

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