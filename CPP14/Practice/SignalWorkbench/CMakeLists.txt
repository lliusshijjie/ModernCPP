cmake_minimum_required(VERSION 3.16)
project(SignalWorkbench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(signal_lab src/workbench.cpp src/executor.cpp)
target_include_directories(signal_lab PUBLIC include)
target_link_libraries(signal_lab PUBLIC Threads::Threads)

add_executable(signal_workbench src/main.cpp)
target_link_libraries(signal_workbench PRIVATE signal_lab)

add_executable(signal_self_test tests/self_test.cpp)
target_link_libraries(signal_self_test PRIVATE signal_lab)

foreach(target signal_lab signal_workbench signal_self_test)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -pedantic-errors)
    endif()
endforeach()

enable_testing()
add_test(NAME signal_self_test COMMAND signal_self_test)
add_test(NAME signal_demo COMMAND signal_workbench)
add_test(NAME signal_file_input COMMAND signal_workbench
    "${CMAKE_CURRENT_SOURCE_DIR}/data/demo.signals"
    "${CMAKE_CURRENT_SOURCE_DIR}/data/demo.settings")
set_tests_properties(signal_self_test signal_demo signal_file_input PROPERTIES TIMEOUT 30)
