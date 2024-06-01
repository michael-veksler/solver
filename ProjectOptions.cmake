include(cmake/SystemLink.cmake)
include(cmake/LibFuzzer.cmake)
include(CMakeDependentOption)
include(CheckCXXCompilerFlag)


macro(solver_supports_sanitizers)
  if((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND NOT WIN32)
    set(SUPPORTS_UBSAN ON)
  else()
    set(SUPPORTS_UBSAN OFF)
  endif()

  if((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND WIN32)
    set(SUPPORTS_ASAN OFF)
  else()
    set(SUPPORTS_ASAN ON)
  endif()
endmacro()

macro(solver_setup_options)
  option(solver_ENABLE_HARDENING "Enable hardening" ON)
  option(solver_ENABLE_COVERAGE "Enable coverage reporting" OFF)
  cmake_dependent_option(
    solver_ENABLE_GLOBAL_HARDENING
    "Attempt to push hardening options to built dependencies"
    ON
    solver_ENABLE_HARDENING
    OFF)

  solver_supports_sanitizers()

  if(NOT PROJECT_IS_TOP_LEVEL OR solver_PACKAGING_MAINTAINER_MODE)
    option(solver_ENABLE_IPO "Enable IPO/LTO" OFF)
    option(solver_WARNINGS_AS_ERRORS "Treat Warnings As Errors" OFF)
    option(solver_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
    option(solver_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" OFF)
    option(solver_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(solver_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" OFF)
    option(solver_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(solver_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(solver_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(solver_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
    option(solver_ENABLE_CPPCHECK "Enable cpp-check analysis" OFF)
    option(solver_ENABLE_PCH "Enable precompiled headers" OFF)
    option(solver_ENABLE_CACHE "Enable ccache" OFF)
  else()
    option(solver_ENABLE_IPO "Enable IPO/LTO" ON)
    option(solver_WARNINGS_AS_ERRORS "Treat Warnings As Errors" ON)
    option(solver_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
    option(solver_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" ${SUPPORTS_ASAN})
    option(solver_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(solver_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" ${SUPPORTS_UBSAN})
    option(solver_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(solver_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(solver_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(solver_ENABLE_CLANG_TIDY "Enable clang-tidy" ON)
    option(solver_ENABLE_CPPCHECK "Enable cpp-check analysis" ON)
    option(solver_ENABLE_PCH "Enable precompiled headers" OFF)
    option(solver_ENABLE_CACHE "Enable ccache" ON)
  endif()

  if(NOT PROJECT_IS_TOP_LEVEL)
    mark_as_advanced(
      solver_ENABLE_IPO
      solver_WARNINGS_AS_ERRORS
      solver_ENABLE_USER_LINKER
      solver_ENABLE_SANITIZER_ADDRESS
      solver_ENABLE_SANITIZER_LEAK
      solver_ENABLE_SANITIZER_UNDEFINED
      solver_ENABLE_SANITIZER_THREAD
      solver_ENABLE_SANITIZER_MEMORY
      solver_ENABLE_UNITY_BUILD
      solver_ENABLE_CLANG_TIDY
      solver_ENABLE_CPPCHECK
      solver_ENABLE_COVERAGE
      solver_ENABLE_PCH
      solver_ENABLE_CACHE)
  endif()

  solver_check_libfuzzer_support(LIBFUZZER_SUPPORTED)
  if(LIBFUZZER_SUPPORTED AND (solver_ENABLE_SANITIZER_ADDRESS OR solver_ENABLE_SANITIZER_THREAD OR solver_ENABLE_SANITIZER_UNDEFINED))
    set(DEFAULT_FUZZER ON)
  else()
    set(DEFAULT_FUZZER OFF)
  endif()

  option(solver_BUILD_FUZZ_TESTS "Enable fuzz testing executable" ${DEFAULT_FUZZER})

endmacro()

macro(solver_global_options)
  if(solver_ENABLE_IPO)
    include(cmake/InterproceduralOptimization.cmake)
    solver_enable_ipo()
  endif()

  solver_supports_sanitizers()

  if(solver_ENABLE_HARDENING AND solver_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR solver_ENABLE_SANITIZER_UNDEFINED
       OR solver_ENABLE_SANITIZER_ADDRESS
       OR solver_ENABLE_SANITIZER_THREAD
       OR solver_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    message("${solver_ENABLE_HARDENING} ${ENABLE_UBSAN_MINIMAL_RUNTIME} ${solver_ENABLE_SANITIZER_UNDEFINED}")
    solver_enable_hardening(solver_options ON ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()
endmacro()

macro(solver_local_options)
  if(PROJECT_IS_TOP_LEVEL)
    include(cmake/StandardProjectSettings.cmake)
  endif()

  add_library(solver_warnings INTERFACE)
  add_library(solver_options INTERFACE)

  include(cmake/CompilerWarnings.cmake)
  solver_set_project_warnings(
    solver_warnings
    ${solver_WARNINGS_AS_ERRORS}
    ""
    ""
    ""
    "")

  if(solver_ENABLE_USER_LINKER)
    include(cmake/Linker.cmake)
    configure_linker(solver_options)
  endif()

  include(cmake/Sanitizers.cmake)
  solver_enable_sanitizers(
    solver_options
    ${solver_ENABLE_SANITIZER_ADDRESS}
    ${solver_ENABLE_SANITIZER_LEAK}
    ${solver_ENABLE_SANITIZER_UNDEFINED}
    ${solver_ENABLE_SANITIZER_THREAD}
    ${solver_ENABLE_SANITIZER_MEMORY})

  set_target_properties(solver_options PROPERTIES UNITY_BUILD ${solver_ENABLE_UNITY_BUILD})

  if(solver_ENABLE_PCH)
    target_precompile_headers(
      solver_options
      INTERFACE
      <vector>
      <string>
      <utility>)
  endif()

  if(solver_ENABLE_CACHE)
    include(cmake/Cache.cmake)
    solver_enable_cache()
  endif()

  include(cmake/StaticAnalyzers.cmake)
  if(solver_ENABLE_CLANG_TIDY)
    solver_enable_clang_tidy(solver_options ${solver_WARNINGS_AS_ERRORS})
  endif()

  if(solver_ENABLE_CPPCHECK)
    solver_enable_cppcheck(${solver_WARNINGS_AS_ERRORS} "" # override cppcheck options
    )
  endif()

  if(solver_ENABLE_COVERAGE)
    include(cmake/Tests.cmake)
    solver_enable_coverage(solver_options)
  endif()

  if(solver_WARNINGS_AS_ERRORS)
    check_cxx_compiler_flag("-Wl,--fatal-warnings" LINKER_FATAL_WARNINGS)
    if(LINKER_FATAL_WARNINGS)
      # This is not working consistently, so disabling for now
      # target_link_options(solver_options INTERFACE -Wl,--fatal-warnings)
    endif()
  endif()

  if(solver_ENABLE_HARDENING AND NOT solver_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR solver_ENABLE_SANITIZER_UNDEFINED
       OR solver_ENABLE_SANITIZER_ADDRESS
       OR solver_ENABLE_SANITIZER_THREAD
       OR solver_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    solver_enable_hardening(solver_options OFF ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()

endmacro()
