cmake_minimum_required(VERSION 3.20)
project(mini_judge_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

set(MINI_JUDGE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(MINI_JUDGE_THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

add_library(mini_judge_lib
    src/config.cpp
    src/comparator.cpp
    src/runner_win32.cpp
    src/scheduler.cpp
    src/engine.cpp
    src/reporter.cpp
)

target_include_directories(mini_judge_lib PUBLIC
    ${MINI_JUDGE_INCLUDE_DIR}
    ${MINI_JUDGE_THIRD_PARTY_DIR}
)

target_link_libraries(mini_judge_lib PUBLIC
    user32
    kernel32
)

add_executable(mini_judge src/main.cpp)
target_link_libraries(mini_judge PRIVATE mini_judge_lib)

add_executable(mini_judge_tests tests/test_main.cpp)
target_link_libraries(mini_judge_tests PRIVATE mini_judge_lib)
add_test(NAME mini_judge_unit_tests COMMAND mini_judge_tests)

set(SAMPLE_RUNTIME_DIR ${CMAKE_BINARY_DIR}/sample_runtime)
set(SAMPLE_PROGRAM_DIR ${SAMPLE_RUNTIME_DIR}/programs)
set(SAMPLE_TEST_DIR ${SAMPLE_RUNTIME_DIR}/tests)
set(SAMPLE_CONFIG_DIR ${SAMPLE_RUNTIME_DIR}/configs)

add_executable(sample_1001 samples/programs/1001.cpp)
add_executable(sample_1002 samples/programs/1002.cpp)
add_executable(sample_1003 samples/programs/1003.cpp)
set_target_properties(sample_1001 PROPERTIES OUTPUT_NAME "1001" RUNTIME_OUTPUT_DIRECTORY ${SAMPLE_PROGRAM_DIR})
set_target_properties(sample_1002 PROPERTIES OUTPUT_NAME "1002" RUNTIME_OUTPUT_DIRECTORY ${SAMPLE_PROGRAM_DIR})
set_target_properties(sample_1003 PROPERTIES OUTPUT_NAME "1003" RUNTIME_OUTPUT_DIRECTORY ${SAMPLE_PROGRAM_DIR})

file(MAKE_DIRECTORY ${SAMPLE_TEST_DIR})
file(MAKE_DIRECTORY ${SAMPLE_CONFIG_DIR})
configure_file(samples/tests/01.in ${SAMPLE_TEST_DIR}/01.in COPYONLY)
configure_file(samples/tests/01.out ${SAMPLE_TEST_DIR}/01.out COPYONLY)
configure_file(samples/tests/02.in ${SAMPLE_TEST_DIR}/02.in COPYONLY)
configure_file(samples/tests/02.out ${SAMPLE_TEST_DIR}/02.out COPYONLY)
configure_file(samples/configs/demo_problem.json ${SAMPLE_CONFIG_DIR}/demo_problem.json COPYONLY)

add_test(
    NAME mini_judge_integration
    COMMAND mini_judge --config ${SAMPLE_CONFIG_DIR}/demo_problem.json --jobs 2 --report-dir ${SAMPLE_RUNTIME_DIR}/out
)
