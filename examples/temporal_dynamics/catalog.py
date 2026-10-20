"""List the standalone examples shipped with the library."""
import hashlib
from pathlib import Path
import subprocess

GROUPS = {
    "Databases and history": ["algebraic_database", "graph_database", "graph_history", "durable_history", "concurrent_history", "database_preview", "query_tensor", "algebraic_storage", "phase_fold_storage", "phase_fold_histories"],
    "Fields and dynamics": ["field_cache", "native_green", "native_updates", "native_frames", "native_metric_novelty", "native_observations", "native_linear_observations", "native_relative", "native_gluing", "prepared_updates", "prepared_global_actions", "ordered_exact_solve", "selected_exact_solve", "parallel_field", "hardware_field", "work_dispatch", "vulkan_binary"],
    "Products and coherence": ["native_tensor_product", "native_product_associator", "native_product_pentagon", "native_product_interchange", "native_product_hexagon", "native_product_braid", "native_product_chain_map", "native_product_map_composition", "native_product_map_associativity", "algebraic_modules", "algebraic_metrics"],
    "Geometry and phase": ["algebraic_graph", "algebraic_phase_fields", "complex_root_certificates", "native_phase_shapes", "native_covering", "native_covering_homotopy", "native_covering_homotopy_base_change", "native_cylinder", "native_relational_homotopy", "native_homotopy_composition", "native_homotopy_sequence", "attaching_transport", "cellular_attachments", "engine_consumer"],
}


def inventory(library):
    library = Path(library)
    if (library/".git").exists():
        names = subprocess.check_output(["git", "-C", str(library), "ls-files", "--cached", "--others", "--exclude-standard", "-z"]).decode().split("\0")
    else:
        names = [p.relative_to(library).as_posix() for p in library.rglob("*") if p.is_file()]
    result = []
    for name in sorted(set(names)):
        p = Path(name)
        if not name or any(x in {".git", "build", "__pycache__", "internal"} for x in p.parts):
            continue
        if p.suffix.lower() == ".md" and p.name != "README.md":
            continue
        if p.suffix.lower() in {".pyc", ".o", ".a", ".so", ".zip", ".pdf"}:
            continue
        f = library/p
        if f.is_file() and not f.is_symlink():
            result.append(name)
    return result


def catalog(library):
    library = Path(library)
    items = []
    for directory in sorted((library/"examples").iterdir()):
        if directory.name == "temporal_dynamics" or not (directory/"CMakeLists.txt").is_file():
            continue
        readme = directory/"README.md"
        group = next((key for key, names in GROUPS.items() if directory.name in names), "Other distributed examples")
        files = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in directory.iterdir() if p.is_file() and p.suffix in {".cpp", ".hpp", ".py"}}
        items.append({"name": directory.name, "group": group,
                      "source": f"rexgraphcpp/examples/{directory.name}/", "source_sha256": files,
                      "readme": readme.read_text() if readme.is_file() else "Read the standalone CMake project and its argument handling.",
                      "build": [f"cmake -S rexgraphcpp/examples/{directory.name} -B build/examples/{directory.name} -DRFC_SOURCE_DIR=\"$PWD/rexgraphcpp\"",
                                f"cmake --build build/examples/{directory.name} --parallel 1"]})
    return {"examples": items, "source": "RexGraphCPP standalone examples"}
