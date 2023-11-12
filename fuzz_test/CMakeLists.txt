# A fuzz test runs until it finds an error. This particular one is going to rely on libFuzzer.
#

find_package(fmt)

file(GLOB fuzz_test_source_files "*.cpp")

add_executable(fuzz_tester ${fuzz_test_source_files})
target_link_libraries(
  fuzz_tester
  PRIVATE solver_options
          solver_warnings
          solver::solver_library
          spdlog::spdlog
          fmt::fmt
          -coverage
          -fsanitize=fuzzer)
target_compile_options(fuzz_tester PRIVATE -fsanitize=fuzzer)

target_link_system_libraries(solver_library PRIVATE spdlog::spdlog)

# Allow short runs during automated testing to see if something new breaks
set(FUZZ_RUNTIME
    10
    CACHE STRING "Number of seconds to run fuzz tests during ctest run") # Default of 10 seconds

add_test(NAME fuzz_tester_run COMMAND fuzz_tester -max_total_time=${FUZZ_RUNTIME})
