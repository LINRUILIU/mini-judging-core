cmake_minimum_required(VERSION 3.20)
project(mini_judge_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    add_compile_options(/utf-8)
endif()

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
target_link_libraries(mini_judge PRIVATE mini_judge_lib shell32)

add_executable(mini_judge_tests tests/test_main.cpp)
target_link_libraries(mini_judge_tests PRIVATE mini_judge_lib)
add_test(NAME mini_judge_unit_tests COMMAND mini_judge_tests)
set_tests_properties(mini_judge_unit_tests PROPERTIES TIMEOUT 10)
add_test(NAME mini_judge_check_failure COMMAND mini_judge_tests --verify-check-failure)
set_tests_properties(mini_judge_check_failure PROPERTIES WILL_FAIL TRUE TIMEOUT 10)

foreach(test_name IN ITEMS config scheduler reporter runner engine)
    add_executable(mini_judge_${test_name}_tests tests/test_${test_name}.cpp)
    target_link_libraries(mini_judge_${test_name}_tests PRIVATE mini_judge_lib)
endforeach()
target_link_libraries(mini_judge_runner_tests PRIVATE shell32)
target_link_libraries(mini_judge_engine_tests PRIVATE shell32)

add_test(NAME mini_judge_config_tests COMMAND mini_judge_config_tests)
add_test(NAME mini_judge_scheduler_tests COMMAND mini_judge_scheduler_tests)
add_test(NAME mini_judge_reporter_tests COMMAND mini_judge_reporter_tests)
set_tests_properties(mini_judge_config_tests mini_judge_reporter_tests PROPERTIES TIMEOUT 10)
set_tests_properties(mini_judge_scheduler_tests PROPERTIES TIMEOUT 15)

set(FIXTURE_PROGRAM_DIR ${CMAKE_BINARY_DIR}/fixture_programs/$<CONFIG>)
set(FIXTURE_MODES
    ac:0 wa:1 sleep:2 re:3 out63:4 out64:5 out65:6 stderr_only:7
    stderr_flood:8 unread_sleep:9 output_then_read:10 early_exit:11
    spawn_parent:12 spawn_child:13 echo:14 stdout_flood:15
)
foreach(spec IN LISTS FIXTURE_MODES)
    string(REPLACE ":" ";" parts "${spec}")
    list(GET parts 0 fixture_name)
    list(GET parts 1 fixture_mode)
    add_executable(fixture_${fixture_name} tests/fixture_program.cpp)
    target_compile_definitions(fixture_${fixture_name} PRIVATE FIXTURE_MODE=${fixture_mode})
    set_target_properties(fixture_${fixture_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${FIXTURE_PROGRAM_DIR}")
endforeach()

add_test(NAME mini_judge_runner_tests
    COMMAND mini_judge_runner_tests $<TARGET_FILE_DIR:fixture_ac>)
set_tests_properties(mini_judge_runner_tests PROPERTIES TIMEOUT 30)

set(SAMPLE_RUNTIME_DIR ${CMAKE_BINARY_DIR}/sample_runtime/$<CONFIG>)
set(SAMPLE_PROGRAM_DIR ${SAMPLE_RUNTIME_DIR}/programs)
set(SAMPLE_TEST_DIR ${SAMPLE_RUNTIME_DIR}/tests)
set(SAMPLE_CONFIG_DIR ${SAMPLE_RUNTIME_DIR}/configs)

add_executable(sample_1001 samples/programs/1001.cpp)
add_executable(sample_1002 samples/programs/1002.cpp)
add_executable(sample_1003 samples/programs/1003.cpp)
set_target_properties(sample_1001 PROPERTIES OUTPUT_NAME "1001" RUNTIME_OUTPUT_DIRECTORY ${SAMPLE_PROGRAM_DIR})
set_target_properties(sample_1002 PROPERTIES OUTPUT_NAME "1002" RUNTIME_OUTPUT_DIRECTORY ${SAMPLE_PROGRAM_DIR})
set_target_properties(sample_1003 PROPERTIES OUTPUT_NAME "1003" RUNTIME_OUTPUT_DIRECTORY ${SAMPLE_PROGRAM_DIR})

file(GENERATE OUTPUT ${SAMPLE_TEST_DIR}/01.in INPUT ${CMAKE_CURRENT_SOURCE_DIR}/samples/tests/01.in)
file(GENERATE OUTPUT ${SAMPLE_TEST_DIR}/01.out INPUT ${CMAKE_CURRENT_SOURCE_DIR}/samples/tests/01.out)
file(GENERATE OUTPUT ${SAMPLE_TEST_DIR}/02.in INPUT ${CMAKE_CURRENT_SOURCE_DIR}/samples/tests/02.in)
file(GENERATE OUTPUT ${SAMPLE_TEST_DIR}/02.out INPUT ${CMAKE_CURRENT_SOURCE_DIR}/samples/tests/02.out)
file(GENERATE OUTPUT ${SAMPLE_CONFIG_DIR}/demo_problem.json INPUT ${CMAKE_CURRENT_SOURCE_DIR}/samples/configs/demo_problem.json)

add_test(
    NAME mini_judge_integration
    COMMAND $<TARGET_FILE:mini_judge> --config ${SAMPLE_CONFIG_DIR}/demo_problem.json
            --jobs 2 --report-dir ${SAMPLE_RUNTIME_DIR}/out
)
set_tests_properties(mini_judge_integration PROPERTIES TIMEOUT 30)

add_dependencies(mini_judge_engine_tests sample_1001)
add_test(NAME mini_judge_engine_tests
    COMMAND mini_judge_engine_tests $<TARGET_FILE:sample_1001>)
set_tests_properties(mini_judge_engine_tests PROPERTIES TIMEOUT 15)

find_program(POWERSHELL_EXE NAMES pwsh REQUIRED)
add_test(NAME mini_judge_cli_tests
    COMMAND ${POWERSHELL_EXE} -NoProfile -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli.ps1
            -Judge $<TARGET_FILE:mini_judge>
            -FixtureDir $<TARGET_FILE_DIR:fixture_ac>)
set_tests_properties(mini_judge_cli_tests PROPERTIES TIMEOUT 45)
