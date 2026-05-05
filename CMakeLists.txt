cmake_minimum_required(VERSION 4.0)
project(ModbusDumper)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)


find_package(Qt6 COMPONENTS
        Core
        Gui
        Widgets
        REQUIRED)

add_executable(ModbusDumper
        src/app/main.cpp
        src/ui/MainWindow.cpp
        src/ui/MainWindow.h
        src/model/registerTypes.h
        src/model/registerFormatting.cpp
        src/model/registerFormatting.h
        src/model/RegisterTableModel.cpp
        src/model/RegisterTableModel.h
        src/data/RegisterDataSource.h
        src/data/MockRegisterDataSource.cpp
        src/data/MockRegisterDataSource.h
        src/data/MarsrockMpptBleDataSource.cpp
        src/data/MarsrockMpptBleDataSource.hpp
        src/io/RegisterDataRecorder.h
        src/io/CsvRegisterDataRecorder.cpp
        src/io/CsvRegisterDataRecorder.h
        src/data/generateDataRegisters.cpp
        src/data/generateDataRegisters.h
)

target_include_directories(ModbusDumper PRIVATE src)

target_link_libraries(ModbusDumper
        Qt::Core
        Qt::Gui
        Qt::Widgets
)

add_subdirectory(external/xw32ble EXCLUDE_FROM_ALL)

target_link_libraries(ModbusDumper xw32ble)


install(
        TARGETS ModbusDumper
        RUNTIME DESTINATION bin
)

qt_generate_deploy_app_script(
        TARGET ModbusDumper
        OUTPUT_SCRIPT qt_deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
)

install(SCRIPT ${qt_deploy_script})

find_program(IFW_BINARYCREATOR_EXECUTABLE
        NAMES binarycreator binarycreator.exe
        HINTS
        "$ENV{QTIFWDIR}/bin"
        "D:/Qt/Tools/QtInstallerFramework/4.8/bin"
        "D:/Qt/Tools/QtInstallerFramework/4.9/bin"
        "D:/Qt/Tools/QtInstallerFramework/4.10/bin"
        "D:/Qt/Tools/QtInstallerFramework/4.11/bin"
)

if(IFW_BINARYCREATOR_EXECUTABLE)
    add_custom_target(package-ifw
            COMMAND ${CMAKE_COMMAND}
            --install "${CMAKE_BINARY_DIR}"
            --config $<CONFIG>
            --prefix "${CMAKE_BINARY_DIR}/installer-root"

            COMMAND ${CMAKE_COMMAND} -E rm -rf
            "${CMAKE_SOURCE_DIR}/installer/packages/pl.knowledge.modbusdumper/data"

            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_BINARY_DIR}/installer-root"
            "${CMAKE_SOURCE_DIR}/installer/packages/pl.knowledge.modbusdumper/data"

            COMMAND "${IFW_BINARYCREATOR_EXECUTABLE}"
            -c "${CMAKE_SOURCE_DIR}/installer/config/config.xml"
            -p "${CMAKE_SOURCE_DIR}/installer/packages"
            "${CMAKE_BINARY_DIR}/ModbusDumperSetup.exe"

            DEPENDS ModbusDumper
            COMMENT "Creating Qt Installer Framework installer"
    )
else()
    message(STATUS "Qt Installer Framework binarycreator not found; package-ifw target disabled.")
endif()