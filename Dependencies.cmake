include(cmake/CPM.cmake)

# Done as a function so that updates to variables like
# CMAKE_CXX_FLAGS don't propagate out to other
# targets
function(solver_setup_dependencies)

  # For each dependency, see if it's
  # already been provided to us by a parent project

  if(NOT TARGET fmtlib::fmtlib)
    cpmaddpackage("gh:fmtlib/fmt#10.1.1")
  endif()

  if(NOT TARGET spdlog::spdlog)
    cpmaddpackage(
      NAME
      spdlog
      VERSION
      1.12.0
      GITHUB_REPOSITORY
      "gabime/spdlog"
      OPTIONS
      "SPDLOG_FMT_EXTERNAL ON")
  endif()

  set(BOOST_MODULES_LIST numeric_conversion config core type_traits mpl preprocessor throw_exception assert)
  set(BOOST_VERSION 1.81.0)
  foreach(boost_package ${BOOST_MODULES_LIST})
    if(NOT TARGET Boost::${boost_package})
      CPMAddPackage(
        NAME boost_${boost_package}
        VERSION ${BOOST_VERSION}
        GITHUB_REPOSITORY "boostorg/${boost_package}"
        GIT_TAG "boost-${BOOST_VERSION}"
      )
    endif()
  endforeach()

  if(NOT TARGET Catch2::Catch2WithMain)
    cpmaddpackage("gh:catchorg/Catch2@3.3.2")
  endif()

  if(NOT TARGET CLI11::CLI11)
    cpmaddpackage("gh:CLIUtils/CLI11@2.3.2")
  endif()

  if(NOT TARGET ftxui::screen)
    cpmaddpackage("gh:ArthurSonzogni/FTXUI#e23dbc7473654024852ede60e2121276c5aab660")
  endif()

  if(NOT TARGET tools::tools)
    cpmaddpackage("gh:lefticus/tools#update_build_system")
  endif()

endfunction()
