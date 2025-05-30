# Архитектура проекта MCEAS

## Обзор

MCEAS (Module Client Endpoint Audit System) - система аудита и мониторинга конечных точек, построенная на модульной архитектуре. Система способна работать на Windows, macOS и Linux.

## Компоненты верхнего уровня
MCEAS/
├── agent/                # Клиентский агент (C++/Rust)
├── server/               # Серверная часть (FastAPI/Node.js/Go)
├── frontend/             # Веб-интерфейс (React + TypeScript)
├── docs/                 # Документация проекта
├── deployment/           # Скрипты и конфигурации для развертывания
└── README.md             # Основное описание проекта
## Архитектура клиентского агента

Клиентский агент построен по модульному принципу, где каждый модуль отвечает за определенную функциональность:

### Ядро (Core)
- **Agent** - основной класс, координирующий работу агента
- **Config** - управление конфигурацией и настройками
- **Module Manager** - управление жизненным циклом модулей
- **Updater** - автоматическое обновление клиента

### Модули (Modules)
- **Browser Module** - анализ и сбор данных из браузеров
- **Crypto Module** - анализ криптокошельков
- **File Telemetry** - файловая телеметрия
- **Behavior** - поведенческий аудит и мониторинг

### Коммуникации (Communications)
- **Client** - взаимодействие с сервером C2
- **Protocols** - реализация протоколов обмена данными
- **Crypto** - шифрование и защита трафика

### Безопасность (Security)
- **Encryption** - шифрование данных
- **Anti-Analysis** - защита от анализа
- **Self-Protection** - механизмы самозащиты агента

### Утилиты (Utils)
- **System** - системные функции
- **File** - работа с файлами
- **Logging** - логирование и диагностика
- **Platform** - кроссплатформенные утилиты

### Архитектурные принципы

Агент следует следующим архитектурным принципам:

1. **Модульность** - каждый модуль является независимым компонентом с четко определенным интерфейсом
2. **Расширяемость** - новые модули могут быть добавлены без изменения существующего кода
3. **Кроссплатформенность** - поддержка Windows, macOS и Linux
4. **Низкое потребление ресурсов** - оптимизация для минимального влияния на систему
5. **Безопасность** - защита данных в процессе хранения и передачи