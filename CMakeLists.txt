cmake_minimum_required(VERSION 3.16)
project(KarabasFlasher VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network SerialPort)

# Платформенный слой (работа с дисками: поиск, монтирование, извлечение).
if(WIN32)
    set(PLATFORM_SRC src/platform_windows.cpp)
elseif(APPLE)
    set(PLATFORM_SRC src/platform_macos.cpp)
elseif(UNIX)
    set(PLATFORM_SRC src/platform_linux.cpp)
else()
    message(FATAL_ERROR "Unsupported platform. See src/Platform.h for the interface to implement.")
endif()

qt_add_executable(KarabasFlasher WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/Worker.cpp
    src/Worker.h
    src/Models.h
    src/Uf2.cpp
    src/Uf2.h
    src/Util.cpp
    src/Util.h
    src/Platform.h
    ${PLATFORM_SRC}
)

target_link_libraries(KarabasFlasher PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::SerialPort)

if(APPLE)
    set_target_properties(KarabasFlasher PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.karabas.flasher"
        MACOSX_BUNDLE_BUNDLE_NAME "Karabas Flasher"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
endif()

install(TARGETS KarabasFlasher RUNTIME DESTINATION bin BUNDLE DESTINATION .)

# Linux: правило udev выдаёт доступ к порту Карабаса без группы dialout/uucp
if(UNIX AND NOT APPLE)
    install(FILES packaging/70-karabas.rules DESTINATION lib/udev/rules.d)
endif()
