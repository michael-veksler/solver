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

  set(BOOST_VERSION 1.81.0)
  if(NOT TARGET Boost::numeric_conversion)
    CPMAddPackage(
      NAME boost_numeric_conversion
      VERSION ${BOOST_VERSION}
      GITHUB_REPOSITORY "boostorg/numeric_conversion"
      GIT_TAG "boost-${BOOST_VERSION}"
    )
  endif()
  if(NOT TARGET Boost::config)
    CPMAddPackage(
      NAME boost_config
      VERSION ${BOOST_VERSION}
      GITHUB_REPOSITORY "boostorg/config"
      GIT_TAG "boost-${BOOST_VERSION}"
    )
  endif()
  if(NOT TARGET Boost::core)
    CPMAddPackage(
      NAME boost_core
      VERSION ${BOOST_VERSION}
      GITHUB_REPOSITORY "boostorg/core"
      GIT_TAG "boost-${BOOST_VERSION}"
    )
  endif()
  if(NOT TARGET Boost::type_traits)
    CPMAddPackage(
      NAME boost_type_traits
      VERSION ${BOOST_VERSION}
      GITHUB_REPOSITORY "boostorg/type_traits"
      GIT_TAG "boost-${BOOST_VERSION}"
    )
  endif()
  if(NOT TARGET Boost::mpl)
    CPMAddPackage(
      NAME boost_mpl
      VERSION ${BOOST_VERSION}
      GITHUB_REPOSITORY "boostorg/mpl"
      GIT_TAG "boost-${BOOST_VERSION}"
    )
  endif()
  if(NOT TARGET Boost::preprocessor)
    CPMAddPackage(
      NAME boost_preprocessor
      VERSION ${BOOST_VERSION}
      GITHUB_REPOSITORY "boostorg/preprocessor"
      GIT_TAG "boost-${BOOST_VERSION}"
    )
  endif()
  if(NOT TARGET Boost::throw_exception)
    CPMAddPackage(
      NAME boost_throw_exception
      VERSION ${BOOST_VERSION}
      GITHUB_REPOSITORY "boostorg/throw_exception"
      GIT_TAG "boost-${BOOST_VERSION}"
    )
  endif()
  if(NOT TARGET Boost::assert)
    CPMAddPackage(
      NAME boost_assert
      VERSION ${BOOST_VERSION}
      GITHUB_REPOSITORY "boostorg/assert"
      GIT_TAG "boost-${BOOST_VERSION}"
    )
  endif()

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
