cmake_minimum_required(VERSION 3.18)
project(wasm-tools)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(
  wabt
  GIT_REPOSITORY https://github.com/fix-project/wabt
  GIT_TAG        fixpoint
  )
set(BUILD_TESTS off)
set(BUILD_TOOLS off)
set(BUILD_LIBWASM off)
FetchContent_MakeAvailable(wabt)

include_directories(SYSTEM "${CMAKE_BINARY_DIR}/_deps/wabt-src/include")
include_directories(SYSTEM "${CMAKE_BINARY_DIR}/_deps/wabt-build/include")

include_directories("${PROJECT_SOURCE_DIR}/src/module-combiner")
add_subdirectory("${PROJECT_SOURCE_DIR}/src/module-combiner")

include_directories("${PROJECT_SOURCE_DIR}/src/access-checker")
add_subdirectory("${PROJECT_SOURCE_DIR}/src/access-checker")

include_directories("${PROJECT_SOURCE_DIR}/src/export-audit")
add_subdirectory("${PROJECT_SOURCE_DIR}/src/export-audit")

include_directories("${PROJECT_SOURCE_DIR}/src/import-check")
add_subdirectory("${PROJECT_SOURCE_DIR}/src/import-check")
