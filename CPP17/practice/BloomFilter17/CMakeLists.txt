cmake_minimum_required(VERSION 3.16)
project(BloomFilter17 VERSION 1.0 LANGUAGES CXX)

option(BLOOM17_BUILD_BENCHMARK "Build the standalone benchmark" ON)
option(BLOOM17_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

add_library(bloom17 src/bloom_filter.cpp)
add_library(bloom17::bloom17 ALIAS bloom17)
target_include_directories(bloom17 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(bloom17 PUBLIC cxx_std_17)
set_target_properties(bloom17 PROPERTIES CXX_EXTENSIONS OFF)

function(bloom17_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
    endif()

    if(BLOOM17_SANITIZERS AND NOT MSVC)
        target_compile_options(${target} PUBLIC
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PUBLIC -fsanitize=address,undefined)
    endif()
endfunction()

bloom17_warnings(bloom17)

add_executable(bloom_demo examples/sstable_demo.cpp)
target_link_libraries(bloom_demo PRIVATE bloom17)
bloom17_warnings(bloom_demo)

include(CTest)
if(BUILD_TESTING)
    find_package(Threads REQUIRED)
    add_executable(bloom_tests tests/bloom_filter_test.cpp)
    target_link_libraries(bloom_tests PRIVATE bloom17 Threads::Threads)
    bloom17_warnings(bloom_tests)

    add_test(NAME bloom_tests COMMAND bloom_tests)
    add_test(NAME bloom_demo COMMAND bloom_demo)
    set_tests_properties(bloom_tests bloom_demo PROPERTIES TIMEOUT 60)
endif()

if(BLOOM17_BUILD_BENCHMARK)
    add_executable(bloom_bench benchmarks/bloom_bench.cpp)
    target_link_libraries(bloom_bench PRIVATE bloom17)
    bloom17_warnings(bloom_bench)
endif()
