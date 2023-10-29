cmake_minimum_required(VERSION 3.15...3.23)

project(CmakeConfigPackageTests LANGUAGES CXX)

# ---- Test as standalone project the exported config package ----

if(PROJECT_IS_TOP_LEVEL OR TEST_INSTALLED_VERSION)
  enable_testing()

  find_package(solver_exe CONFIG REQUIRED) # for solver_exe, project_options, ...

  if(NOT TARGET solver_options)
    message(FATAL_ERROR "Requiered config package not found!")
    return() # be strictly paranoid for Template Janitor github action! CK
  endif()
endif()

# ---- Dependencies ----

include(${Catch2_SOURCE_DIR}/extras/Catch.cmake)

# Provide a simple smoke test to make sure that the CLI works and can display a --help message
add_test(NAME cli.has_help COMMAND solver_exe --help)

set(TEST_INPUTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/inputs)

add_test(NAME cli.trivial_sat COMMAND solver_exe --solver trivial_sat ${TEST_INPUTS_DIR}/trivial.dimacs)
set_tests_properties(cli.trivial_sat PROPERTIES PASS_REGULAR_EXPRESSION "SAT v1=0 v2=1")

add_test(NAME cli.cdcl_sat COMMAND solver_exe --solver cdcl_sat ${TEST_INPUTS_DIR}/trivial.dimacs)
set_tests_properties(cli.cdcl_sat PROPERTIES PASS_REGULAR_EXPRESSION "SAT v1=0 v2=1")

add_test(NAME cli.empty COMMAND solver_exe --solver cdcl_sat ${TEST_INPUTS_DIR}/empty.dimacs)
set_tests_properties(cli.empty PROPERTIES PASS_REGULAR_EXPRESSION "Invalid dimacs input format - all lines are either empty or commented out")

add_test(NAME cli.cdcl_simple_unsat COMMAND solver_exe --solver cdcl_sat ${TEST_INPUTS_DIR}/simple_unsat.dimacs)
set_tests_properties(cli.cdcl_simple_unsat PROPERTIES PASS_REGULAR_EXPRESSION "UNSAT")

add_test(NAME cli.trivial_simple_unsat COMMAND solver_exe --solver trivial_sat ${TEST_INPUTS_DIR}/simple_unsat.dimacs)
set_tests_properties(cli.trivial_simple_unsat PROPERTIES PASS_REGULAR_EXPRESSION "UNSAT")

file(GLOB test_source_files "*.cpp")


add_executable(tests ${test_source_files})
target_link_libraries(
  tests
  PRIVATE solver::solver_warnings
          solver::solver_options
          solver::solver_library
          spdlog::spdlog
          Catch2::Catch2WithMain)

set(BOOST_MODULES_LIST numeric_conversion config core type_traits mpl preprocessor throw_exception assert icl)
set_boost_includes(BOOST_INCLUDE_DIRS "${BOOST_MODULES_LIST}")


include_directories(
  SYSTEM
  ${BOOST_INCLUDE_DIRS}
  ${Catch2_SOURCE_DIR}/src
  ${spdlog_SOURCE_DIR}/include)

if(WIN32 AND BUILD_SHARED_LIBS)
  add_custom_command(
    TARGET tests
    PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:tests> $<TARGET_FILE_DIR:tests>
    COMMAND_EXPAND_LISTS)
endif()

# automatically discover tests that are defined in catch based test files you can modify the unittests. Set TEST_PREFIX
# to whatever you want, or use different for different binaries
catch_discover_tests(
  tests
  TEST_PREFIX
  "unittests."
  REPORTER
  XML
  OUTPUT_DIR
  .
  OUTPUT_PREFIX
  "unittests."
  OUTPUT_SUFFIX
  .xml)

# Add a file containing a set of constexpr tests
add_executable(constexpr_tests constexpr_tests.cpp)
target_link_libraries(
  constexpr_tests
  PRIVATE solver::solver_warnings
          solver::solver_options
          solver::solver_library
          Catch2::Catch2WithMain)

catch_discover_tests(
  constexpr_tests
  TEST_PREFIX
  "constexpr."
  REPORTER
  XML
  OUTPUT_DIR
  .
  OUTPUT_PREFIX
  "constexpr."
  OUTPUT_SUFFIX
  .xml)

# Disable the constexpr portion of the test, and build again this allows us to have an executable that we can debug when
# things go wrong with the constexpr testing
add_executable(relaxed_constexpr_tests constexpr_tests.cpp)
target_link_libraries(
  relaxed_constexpr_tests
  PRIVATE solver::solver_warnings
          solver::solver_options
          solver::solver_library
          Catch2::Catch2WithMain)
target_compile_definitions(relaxed_constexpr_tests PRIVATE -DCATCH_CONFIG_RUNTIME_STATIC_REQUIRE)

catch_discover_tests(
  relaxed_constexpr_tests
  TEST_PREFIX
  "relaxed_constexpr."
  REPORTER
  XML
  OUTPUT_DIR
  .
  OUTPUT_PREFIX
  "relaxed_constexpr."
  OUTPUT_SUFFIX
  .xml)
