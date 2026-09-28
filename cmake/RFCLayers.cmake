# Ordered software layers. Each target may depend only on targets above it in
# the mathematical construction unless explicitly documented as an execution
# adapter. All targets are header-only at this stage.

function(rfc_interface_target name)
  add_library(rfc_${name} INTERFACE)
  add_library(rfc::${name} ALIAS rfc_${name})
  target_include_directories(rfc_${name} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_compile_features(rfc_${name} INTERFACE cxx_std_20)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rfc_${name} INTERFACE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
  endif()
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
add_library(rfc_fundamentals INTERFACE)
add_library(rfc::fundamentals ALIAS rfc_fundamentals)
target_include_directories(rfc_fundamentals INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(rfc_fundamentals INTERFACE cxx_std_20)
target_link_libraries(rfc_fundamentals INTERFACE
  rfc::machine rfc::numeric rfc::composite rfc::core rfc::algebra rfc::geometry
  rfc::local rfc::topology rfc::field rfc::view rfc::path rfc::approximation
  rfc::statistics rfc::temporal)

# 11 persisted relational source / RFCX snapshots. Persistence consumes the
# frozen source representation but does not redefine relational mathematics.
rfc_interface_target(storage)
target_link_libraries(rfc_storage INTERFACE rfc::core rfc::composite rfc::numeric rfc::algebra rfc::local rfc::temporal)

# 12 canonical exact database/accession
rfc_interface_target(database)
target_link_libraries(rfc_database INTERFACE rfc::core rfc::field rfc::temporal rfc::storage)

# Runtime is an execution service. It may accelerate prior layers but owns no
# mathematical semantics.
rfc_interface_target(runtime)
target_link_libraries(rfc_runtime INTERFACE rfc::machine rfc::numeric)

# 12 exact -> derived -> prospective routing
rfc_interface_target(inference)
target_link_libraries(rfc_inference INTERFACE rfc::database rfc::statistics rfc::approximation)

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
target_link_libraries(rfc INTERFACE
  rfc::fundamentals rfc::storage rfc::database rfc::runtime rfc::inference rfc::learning
  rfc::models rfc::llm rfc::multimodal rfc::agent rfc::generation
  rfc::backend rfc::io)
