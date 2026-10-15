cmake_minimum_required(VERSION 3.24)
project(rfc_temporal_relations LANGUAGES CXX)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "The native durable-store demonstration currently requires Linux")
endif()
if(RFC_SOURCE_DIR)
  foreach(option RFC_BUILD_APPS RFC_BUILD_LOCAL_TOOLS RFC_BUILD_TESTS RFC_BUILD_BENCHMARKS RFC_BUILD_CAPABILITY_DEMO RFC_BUILD_LEARNING)
    set(${option} OFF CACHE BOOL "" FORCE)
  endforeach()
  add_subdirectory("${RFC_SOURCE_DIR}" rfc EXCLUDE_FROM_ALL)
else()
  find_package(RFC CONFIG REQUIRED)
endif()
set(TEMPORAL_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../temporal_dynamics")
add_executable(temporal_relations main.cpp analysis.cpp
  "${TEMPORAL_SOURCE}/database.cpp" "${TEMPORAL_SOURCE}/fields.cpp"
  "${TEMPORAL_SOURCE}/algebra.cpp" "${TEMPORAL_SOURCE}/products.cpp")
target_link_libraries(temporal_relations PRIVATE rfc::inference)
target_compile_features(temporal_relations PRIVATE cxx_std_20)
include(CTest)
if(BUILD_TESTING)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  add_test(NAME temporal_relations_exact
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/verify.py"
      --exe $<TARGET_FILE:temporal_relations> --events "${TEMPORAL_SOURCE}/events.tsv")
  set_tests_properties(temporal_relations_exact PROPERTIES TIMEOUT 180
    ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
endif()
