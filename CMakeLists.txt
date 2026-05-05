cmake_minimum_required(VERSION 4.2)
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

add_subdirectory(external/xw32ble)

target_link_libraries(ModbusDumper xw32ble)
