cmake_minimum_required(VERSION 3.12)
project(MCEAS_Agent VERSION 0.1.0 LANGUAGES CXX)

# Опции компиляции
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Определяем переменную для операционной системы
if(APPLE)
    set(OS_MACOS TRUE)
    add_definitions(-DOS_MACOS)
elseif(WIN32)
    set(OS_WINDOWS TRUE)
    add_definitions(-DOS_WINDOWS)
elseif(UNIX AND NOT APPLE)
    set(OS_LINUX TRUE)
    add_definitions(-DOS_LINUX)
endif()

# Пути для заголовочных файлов
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/internal
    ${CMAKE_CURRENT_SOURCE_DIR}/include/external
    ${CMAKE_CURRENT_SOURCE_DIR}/include/modules
)

# Собираем исходные файлы
file(GLOB_RECURSE CORE_SOURCES "src/core/*.cpp")
file(GLOB_RECURSE MODULE_SOURCES "src/modules/*.cpp")
file(GLOB_RECURSE UTILS_SOURCES "src/utils/*.cpp")

# Собираем все исходные файлы
ser(SOURCES
    src/main.cpp 
    ${CORE_SOURCES}
    ${MODULE_SOURCES}
    ${UTILS_SOURSEC}
    )

# Создаем исполняемый файл
add_executable(mceas_agent ${SOURCES})

# Для macOS настраиваем специфичные флаги
if(OS_MACOS)
    target_compile_options(mceas_agent PRIVATE -Wall -Wextra)
    # Добавляем библиотеки для macOS
    find_library(CORE_FOUNDATION CoreFoundation)
    find_library(SECURITY Security)
    target_link_libraries(mceas_agent ${CORE_FOUNDATION} ${SECURITY})
endif()

# Включаем тесты
option(BUILD_TESTS "Build the tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()