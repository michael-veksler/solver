add_executable(solver_exe main.cpp)

target_link_libraries(
  solver_exe
  PRIVATE solver::solver_options
          solver::solver_warnings
          solver_library)

target_link_system_libraries(
  solver_exe
  PRIVATE
          CLI11::CLI11
          fmt::fmt
          spdlog::spdlog)

include_directories(
  SYSTEM
  ${fmt_SOURCE_DIR}/include()
  ${spdlog_SOURCE_DIR}/include)

target_include_directories(solver_exe PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include")
