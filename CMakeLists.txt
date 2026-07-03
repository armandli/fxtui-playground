cmake_minimum_required(VERSION 3.22)

project(fxtui-playground
  LANGUAGES CXX
  VERSION 1.0.0
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Fetch FTXUI --------------------------------------------------------------
include(FetchContent)

set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/arthursonzogni/ftxui.git
  GIT_TAG        v5.0.0
  GIT_PROGRESS   TRUE
  GIT_SHALLOW    TRUE
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(ftxui)
# ------------------------------------------------------------------------------

# Auto-discover each subdirectory under src/ and build it as a separate executable
file(GLOB SRC_SUBDIRS RELATIVE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/*)

foreach(SUBDIR ${SRC_SUBDIRS})
  if(IS_DIRECTORY ${CMAKE_SOURCE_DIR}/src/${SUBDIR})
    file(GLOB_RECURSE SOURCES ${CMAKE_SOURCE_DIR}/src/${SUBDIR}/*.cpp)
    if(SOURCES)
      add_executable(${SUBDIR} ${SOURCES})
      target_include_directories(${SUBDIR} PRIVATE ${CMAKE_SOURCE_DIR}/src/${SUBDIR})
      target_link_libraries(${SUBDIR}
        PRIVATE ftxui::screen
        PRIVATE ftxui::dom
        PRIVATE ftxui::component
      )
    endif()
  endif()
endforeach()
