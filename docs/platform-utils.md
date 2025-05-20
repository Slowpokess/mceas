### docs/platform-utils.md

```markdown
# Кроссплатформенные утилиты

## Обзор

Модуль `platform_utils` предоставляет кроссплатформенные функции для работы с системными путями, файлами и директориями на различных операционных системах.

## Возможности

- Определение домашней директории пользователя
- Получение стандартных системных директорий
- Работа с файлами и директориями
- Определение установленных приложений и браузеров
- Выполнение системных команд и получение их вывода

## Поддерживаемые платформы

- **Windows**: использует WinAPI и переменные среды
- **macOS**: использует Darwin API и POSIX
- **Linux**: использует стандартные пути POSIX и XDG

## Ключевые функции

### Системные директории

####  Работа с браузерами

```cpp
// Получение домашней директории пользователя
std::string GetHomeDir();

// Получение директории для данных приложений
std::string GetAppDataDir();

// Получение директории для временных файлов
std::string GetTempDir();

// Получение пути к профилю браузера
std::string GetBrowserProfileDir(const std::string& browser_name);

// Проверка установки браузера
bool IsBrowserInstalled(const std::string& browser_name);

// Получение пути к расширению браузера
std::string GetExtensionPath(const std::string& browser, const std::string& extension_id);

#####  COMMAND BUILDING 

// Выполнение команды и получение вывода
std::string ExecuteCommand(const std::string& command);

######  USE CASES

// Получение пути к профилю Chrome
std::string chrome_profile = utils::GetBrowserProfileDir("Chrome");

// Проверка наличия расширения
bool has_extension = utils::IsBrowserExtensionInstalled("Chrome", "extension_id");

// Получение директории для хранения данных
std::string app_data = utils::GetAppDataDir();