# Ordered software layers. Each target may depend only on targets above it in
# the mathematical construction unless explicitly documented as an execution
# adapter. Mathematical targets are header-only; optional backend leaves may compile.

function(rfc_interface_target name)
  add_library(rfc_${name} INTERFACE)
  add_library(rfc::${name} ALIAS rfc_${name})
  target_include_directories(rfc_${name} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  target_compile_features(rfc_${name} INTERFACE cxx_std_20)
  set_target_properties(rfc_${name} PROPERTIES EXPORT_NAME ${name})
  set(RFC_EXPORT_TARGETS ${RFC_EXPORT_TARGETS} rfc_${name} PARENT_SCOPE)
endfunction()

# 00 machine representation
rfc_interface_target(machine)

# 01 scalar domains
rfc_interface_target(numeric)
target_link_libraries(rfc_numeric INTERFACE rfc::machine PkgConfig::GMPXX PkgConfig::GMP)

# 02 composite-binary semantics
rfc_interface_target(composite)
target_link_libraries(rfc_composite INTERFACE rfc::machine rfc::numeric)

# 03 primary relational complex
rfc_interface_target(core)
target_link_libraries(rfc_core INTERFACE rfc::numeric rfc::composite)

# 04 native algebra/category of relational complexes
rfc_interface_target(algebra)
target_link_libraries(rfc_algebra INTERFACE rfc::core)

# 05 nonmetric geometry / declared realizations
rfc_interface_target(geometry)
target_link_libraries(rfc_geometry INTERFACE rfc::algebra)

# 06 local stalk/gluing over retained relational structure
rfc_interface_target(local)
target_link_libraries(rfc_local INTERFACE rfc::geometry)

# 07 coefficient topology/lattice/modular views
rfc_interface_target(topology)
target_link_libraries(rfc_topology INTERFACE rfc::core rfc::algebra rfc::local)

# 08 exact fields, metrics, Hodge/Dirac/Green/deformation
rfc_interface_target(field)
target_link_libraries(rfc_field INTERFACE rfc::core rfc::local rfc::topology)

# 09 metric-aware typed observation views over nonmetric geometry
rfc_interface_target(view)
target_link_libraries(rfc_view INTERFACE rfc::field rfc::geometry)

# 09 exact realized paths and routing objectives
rfc_interface_target(path)
target_link_libraries(rfc_path INTERFACE rfc::view)

# 09 explicit approximation tower (depends on exact field semantics; owns floating arithmetic)
rfc_interface_target(approximation)
target_link_libraries(rfc_approximation INTERFACE rfc::field)

# 09 exact statistical field layer
rfc_interface_target(statistics)
target_link_libraries(rfc_statistics INTERFACE rfc::field)

# 10 temporal operation words, transfer, and chronological propagation
rfc_interface_target(temporal)
target_link_libraries(rfc_temporal INTERFACE rfc::field rfc::path)

# Fundamental mathematical surface stops before persistence, model orchestration,
# learning, optional backends, and I/O.
rfc_interface_target(fundamentals)
target_link_libraries(rfc_fundamentals INTERFACE
  rfc::machine rfc::numeric rfc::composite rfc::core rfc::algebra rfc::geometry
  rfc::local rfc::topology rfc::field rfc::view rfc::path rfc::approximation
  rfc::statistics rfc::temporal)

# 11 persisted relational source / RFCX snapshots. Persistence consumes the
# frozen source representation but does not redefine relational mathematics.
rfc_interface_target(storage)
target_link_libraries(rfc_storage INTERFACE rfc::core rfc::composite rfc::numeric rfc::algebra rfc::local rfc::temporal)

# Opt-in Linux concurrent history adapter; the mathematical targets do not link it.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_package(Threads REQUIRED)
  rfc_interface_target(concurrent_history)
  target_link_libraries(rfc_concurrent_history INTERFACE rfc::storage Threads::Threads)
endif()

# 12 canonical exact database/accession
rfc_interface_target(database)
target_link_libraries(rfc_database INTERFACE rfc::core rfc::field rfc::temporal rfc::storage)

# Runtime is an execution service. It may accelerate prior layers but owns no
# mathematical semantics.
rfc_interface_target(runtime)
target_link_libraries(rfc_runtime INTERFACE rfc::machine rfc::numeric rfc::composite)

# Owning execution composes database semantics with lower runtime policy.
rfc_interface_target(native_execution)
target_link_libraries(rfc_native_execution INTERFACE rfc::database rfc::runtime)

# Owning scoped field caches are opt-in execution services.
find_package(Threads REQUIRED)
rfc_interface_target(native_field_cache)
target_link_libraries(rfc_native_field_cache INTERFACE rfc::native_execution rfc::parallel_native_field)

# Opt-in pure CPU tile execution; mathematical targets keep their dependencies.
rfc_interface_target(cpu_tiles)
target_link_libraries(rfc_cpu_tiles INTERFACE rfc::runtime Threads::Threads)
rfc_interface_target(parallel_native_field)
target_link_libraries(rfc_parallel_native_field INTERFACE rfc::native_execution rfc::cpu_tiles)

# Finite owned job graphs compose only lower execution/resource services.
rfc_interface_target(work_graph)
target_link_libraries(rfc_work_graph INTERFACE rfc::runtime Threads::Threads)
rfc_interface_target(binary_work)
target_link_libraries(rfc_binary_work INTERFACE rfc::work_graph rfc::cpu_tiles)
rfc_interface_target(field_work)
target_link_libraries(rfc_field_work INTERFACE rfc::parallel_native_field rfc::native_field_cache rfc::work_graph)

# Hardware facts and cooperating resource budgets are opt-in host services.
rfc_interface_target(hardware_inventory)
target_link_libraries(rfc_hardware_inventory INTERFACE rfc::runtime)
rfc_interface_target(resource_native_field)
target_link_libraries(rfc_resource_native_field INTERFACE rfc::parallel_native_field rfc::hardware_inventory)
rfc_interface_target(native_locality)
target_link_libraries(rfc_native_locality INTERFACE rfc::field_work rfc::hardware_inventory)
rfc_interface_target(native_query_execution)
target_link_libraries(rfc_native_query_execution INTERFACE rfc::database rfc::cpu_tiles rfc::work_graph)

# 12 exact -> derived -> prospective routing
rfc_interface_target(inference)
target_link_libraries(rfc_inference INTERFACE rfc::database rfc::statistics rfc::approximation rfc::runtime)

# 13 native prospective learning
rfc_interface_target(learning)
target_link_libraries(rfc_learning INTERFACE rfc::inference)

# 14 generic model and multimodal interoperability
rfc_interface_target(models)
target_link_libraries(rfc_models INTERFACE rfc::inference)
rfc_interface_target(llm)
target_link_libraries(rfc_llm INTERFACE rfc::models rfc::inference)
rfc_interface_target(multimodal)
target_link_libraries(rfc_multimodal INTERFACE rfc::models rfc::database)

# 15 orchestration and validated generation
rfc_interface_target(agent)
target_link_libraries(rfc_agent INTERFACE rfc::llm rfc::learning rfc::multimodal)
rfc_interface_target(generation)
target_link_libraries(rfc_generation INTERFACE rfc::agent rfc::database)

# Optional execution backends and interchange remain leaf adapters.
rfc_interface_target(backend)
target_link_libraries(rfc_backend INTERFACE rfc::runtime rfc::field)
rfc_interface_target(io)
target_link_libraries(rfc_io INTERFACE rfc::generation)

# Full umbrella. This is intentionally defined last.
add_library(rfc INTERFACE)
add_library(rfc::rfc ALIAS rfc)
set_target_properties(rfc PROPERTIES EXPORT_NAME rfc)
list(APPEND RFC_EXPORT_TARGETS rfc)
target_link_libraries(rfc INTERFACE
  rfc::fundamentals rfc::storage rfc::database rfc::runtime rfc::native_execution rfc::inference rfc::learning
  rfc::models rfc::llm rfc::multimodal rfc::agent rfc::generation
  rfc::backend rfc::io)
