"""Catalog the distributed examples without importing private development notes."""
import hashlib
import json
from pathlib import Path
import subprocess

GROUPS = {
    "Database and retained time": ["algebraic_database", "graph_database", "graph_history", "durable_history", "concurrent_history", "database_preview", "query_tensor", "algebraic_storage", "phase_fold_storage", "phase_fold_histories"],
    "Fields and finite updates": ["field_cache", "native_green", "native_updates", "native_frames", "native_metric_novelty", "native_observations", "native_linear_observations", "native_relative", "native_gluing", "prepared_updates", "prepared_global_actions", "ordered_exact_solve", "selected_exact_solve", "parallel_field", "hardware_field", "work_dispatch", "vulkan_binary"],
    "Products and coherence": ["native_tensor_product", "native_product_associator", "native_product_pentagon", "native_product_interchange", "native_product_hexagon", "native_product_braid", "native_product_chain_map", "native_product_map_composition", "native_product_map_associativity", "algebraic_modules", "algebraic_metrics"],
    "Shapes, phase and declared maps": ["algebraic_graph", "algebraic_phase_fields", "complex_root_certificates", "native_phase_shapes", "native_covering", "native_covering_homotopy", "native_covering_homotopy_base_change", "native_cylinder", "native_relational_homotopy", "native_homotopy_composition", "native_homotopy_sequence", "attaching_transport", "cellular_attachments", "engine_consumer"],
}
RESEARCH = [
    ("01-arity-laplacian", "Arity and declared boundary normalization", "Fields"),
    ("02-hodge-branching", "Branching relations and Hodge sectors", "Fields"),
    ("03-green-dirac-scaling", "Green and Dirac scaling", "Fields"),
    ("04-gadget-perturbation", "Finite operator perturbations", "Dynamics"),
    ("05-solver-swell", "Exact solver coefficient growth", "Execution"),
    ("06-davis-hypergraph", "Primary group relations", "Database"),
    ("07-topology-zoo", "Declared topological families", "Shapes"),
    ("08-mean-value-kernel", "Exact kernel observations", "Fields"),
    ("09-temporal-evolution", "Threshold filtration and sector transport", "Dynamics"),
    ("10-mesh-hodge", "Mesh fields and declared metrics", "Fields"),
    ("11-rotation-sheaf-sync", "Stalk restrictions and rotation", "Fibers"),
    ("12-heart-gadget-declared", "Declared gadget families", "Shapes"),
    ("13-paper-examples-on-library", "Canonical paper examples using C++", "Comparisons"),
    ("14-figures", "Figure generation and presentation geometry", "Presentation"),
    ("15-geometries", "Exact declared geometric examples", "Shapes"),
    ("16-complex-fields", "Complex field actions", "Phase"),
    ("17-functions-learning", "Function profiles and numerical convergence", "Research"),
    ("18-real-complex-cycles", "Real and complex cycle analysis", "Phase"),
    ("19-origami-curves", "Origami curves and smooth limit calculations", "Research"),
]


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
                                f"cmake --build build/examples/{directory.name} --parallel 1"],
                      "qualification": "Catalogued source. This entry is not a claim that the new demo ran this standalone executable."})
    research = [{"name": name, "subject": subject, "chapter": chapter,
                 "status": "Retained research. Its original qualification and declared numerical or unresolved scope apply."}
                for name, subject, chapter in RESEARCH]
    notebooks = []
    notebook_root=library.parent/"rfc-notebook"
    if (notebook_root/"notebooks").is_dir():
        for p in sorted((notebook_root/"notebooks").glob("*.rfnb")):
            notebooks.append({"name": p.name, "source": "rfc-notebook/notebooks/"+p.name,
                              "source_sha256": hashlib.sha256(p.read_bytes()).hexdigest(),
                              "status": "Existing notebook source; original bytes are preserved."})
    research += [{"name": name, "subject": subject, "chapter": "Research",
                  "status": "Retained internal research; it does not supply event availability or a new mathematical proof."}
                 for name,subject in [("linguistic_correspondence", "Declared language correspondences"),
                                     ("encoded_text", "Text representations and exact native actions"),
                                     ("relational_language", "Relational language field experiments")]]
    return {"examples": items, "research": research, "notebooks": notebooks,
            "source": "Distributed canonical C++ examples and the reviewed 19 experiment families",
            "distinctions": ["The temporal incident is newly executed native data.",
                             "A threshold filtration is not an event availability history.",
                             "A numerical convergence study is not an exact convergence proof.",
                             "A figure coordinate is not a mathematical carrier or a transport."]}
