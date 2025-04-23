import os
import re
from collections import defaultdict

def extract_object_names(makefile_path):
    pattern = re.compile(r'([a-zA-Z0-9_]+)\$\(OBJ\)')
    object_names = set()
    with open(makefile_path, "r", encoding="utf-8") as f:
        for line in f:
            match = pattern.search(line.strip())
            if match:
                object_names.add(match.group(1))
    return object_names

def build_dependency_graph(source_root):
    include_pattern = re.compile(r'#include\s+"([^"]+)"')
    graph = defaultdict(list)
    for root, _, files in os.walk(source_root):
        for file in files:
            if file.endswith((".c", ".h")):
                full_path = os.path.join(root, file)
                try:
                    with open(full_path, "r", encoding="utf-8", errors="ignore") as f:
                        for line in f:
                            match = include_pattern.search(line)
                            if match:
                                included = match.group(1)
                                graph[file].append(included)
                except Exception as e:
                    print(f"Error reading {full_path}: {e}")
    return graph

def gather_deps(start_file, graph, visited=None, level=0):
    if visited is None:
        visited = set()
    if start_file in visited:
        return []
    visited.add(start_file)

    results = [(start_file, level)]
    for dep in graph.get(start_file, []):
        results.extend(gather_deps(dep, graph, visited, level + 1))

    return results

def scan_files_for_objects(source_root, file_list, object_names):
    obj_usage = set()
    for root, _, files in os.walk(source_root):
        for file in files:
            if file in file_list:
                full_path = os.path.join(root, file)
                try:
                    with open(full_path, "r", encoding="utf-8", errors="ignore") as f:
                        content = f.read()
                        for obj in object_names:
                            if obj in content:
                                obj_usage.add(obj)
                except Exception as e:
                    print(f"Error reading {full_path}: {e}")
    return sorted(obj_usage)

# Run it
if __name__ == "__main__":
    # === CONFIGURATION ===
    SOURCE_ROOT = "SoftFloat-3e/source"
    MAKEFILE_PATH = "SoftFloat-3e/build/Linux-x86_64-GCC/Makefile"  # or wherever it is
    START_FILE = "f16_sqrt.c"
    SHOW_RECURSION_LEVEL = True
    # ======================

    # Step 1: Extract object names from Makefile
    object_names = extract_object_names(MAKEFILE_PATH)

    # Step 2: Build include graph and get recursive file deps
    dependency_graph = build_dependency_graph(SOURCE_ROOT)
    deps_with_levels = gather_deps(START_FILE, dependency_graph)
    files_only = [file for file, _ in deps_with_levels]

    # Step 3: Scan those files for object references
    used_objects = scan_files_for_objects(SOURCE_ROOT, files_only, object_names)

    # Output
    print("\n📁 Included Files:")
    for file, lvl in deps_with_levels:
        indent = "  " * lvl if SHOW_RECURSION_LEVEL else ""
        print(f"{indent}{file}")

    print("\n🔧 Referenced Objects:")
    for obj in used_objects:
        print(obj)
