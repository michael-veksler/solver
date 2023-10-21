include(GenerateExportHeader)

file(GLOB solver_library_files "*.cpp")

add_library(solver_library ${solver_library_files})

add_library(solver::solver_library ALIAS solver_library)

target_link_libraries(solver_library PRIVATE solver_options solver_warnings)
target_link_system_libraries(solver_library PRIVATE spdlog::spdlog)

target_include_directories(solver_library ${WARNING_GUARD} PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                                                                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>)

target_compile_features(solver_library PUBLIC cxx_std_20)

set_target_properties(
  solver_library
  PROPERTIES VERSION ${PROJECT_VERSION}
             CXX_VISIBILITY_PRESET hidden
             VISIBILITY_INLINES_HIDDEN YES)

generate_export_header(solver_library EXPORT_FILE_NAME ${PROJECT_BINARY_DIR}/include/solver/solver_library_export.hpp)

if(NOT BUILD_SHARED_LIBS)
  target_compile_definitions(solver_library PUBLIC solver_library_STATIC_DEFINE)
endif()
