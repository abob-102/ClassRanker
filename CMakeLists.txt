cmake_minimum_required(VERSION 3.20)
project(ClassRanker VERSION 1.0.0 LANGUAGES CXX)

option(CLASSRANKER_WITH_XLSX "Enable native .xlsx import and export" ON)

add_executable(ClassRanker src/main.cpp src/csv.cpp src/expression.cpp)
target_compile_features(ClassRanker PRIVATE cxx_std_17)
target_include_directories(ClassRanker PRIVATE src)

if(MSVC)
    target_compile_options(ClassRanker PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(ClassRanker PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    find_package(Qt6 QUIET COMPONENTS Widgets)
    if(Qt6_FOUND)
        qt_add_executable(ClassRankerGUI WIN32 src/qt_gui.cpp)
        target_link_libraries(ClassRankerGUI PRIVATE Qt6::Widgets)
    else()
        add_executable(ClassRankerGUI WIN32 src/gui.cpp)
        target_compile_definitions(ClassRankerGUI PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
        target_link_libraries(ClassRankerGUI PRIVATE comctl32 comdlg32 shell32)
    endif()

    add_dependencies(ClassRankerGUI ClassRanker)
    target_compile_features(ClassRankerGUI PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(ClassRankerGUI PRIVATE /W4 /permissive- /utf-8)
    endif()

    add_custom_command(TARGET ClassRankerGUI POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:ClassRanker>
            $<TARGET_FILE_DIR:ClassRankerGUI>/ClassRankerCLI.exe
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/class_data_template.xlsx
            $<TARGET_FILE_DIR:ClassRankerGUI>/class_data_template.xlsx
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/config/formulas.ini
            $<TARGET_FILE_DIR:ClassRankerGUI>/formulas.ini
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/config/ui_settings.ini
            $<TARGET_FILE_DIR:ClassRankerGUI>/ui_settings.ini
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/操作手册.txt
            $<TARGET_FILE_DIR:ClassRankerGUI>/操作手册.txt)
endif()

if(CLASSRANKER_WITH_XLSX)
    include(FetchContent)
    find_package(OpenXLSX CONFIG QUIET)
    if(NOT OpenXLSX_FOUND)
        set(OPENXLSX_CREATE_DOCS OFF CACHE BOOL "" FORCE)
        set(OPENXLSX_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
        set(OPENXLSX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(OPENXLSX_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(OpenXLSX
            URL ${CMAKE_CURRENT_SOURCE_DIR}/third_party/OpenXLSX-v0.3.2.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
            PATCH_COMMAND ${CMAKE_COMMAND}
                -DOPENXLSX_SOURCE_DIR=<SOURCE_DIR>
                -P ${CMAKE_CURRENT_SOURCE_DIR}/patch_openxlsx.cmake)
        FetchContent_MakeAvailable(OpenXLSX)
    endif()
    target_sources(ClassRanker PRIVATE src/xlsx.cpp)
    target_compile_definitions(ClassRanker PRIVATE CLASSRANKER_WITH_XLSX=1)
    target_link_libraries(ClassRanker PRIVATE OpenXLSX::OpenXLSX)
endif()
