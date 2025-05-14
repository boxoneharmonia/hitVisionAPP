import os
import re

project_root = os.getcwd()

# 更新后的 BUILD.gn 路径定义
target_build_files = {
    "thirdparty": os.path.join(project_root, "thirdparty", "BUILD.gn"),
    "src": os.path.join(project_root, "src", "BUILD.gn"),
}

lib_decl_pattern = re.compile(r'(shared_library|static_library)\s*\(\s*"([^"]+)"\s*\)')

lib_targets = {
    "thirdparty": [],
    "src": []
}

for section, filepath in target_build_files.items():
    if not os.path.isfile(filepath):
        continue
    with open(filepath, "r", encoding="utf-8") as f:
        content = f.read()
        matches = lib_decl_pattern.findall(content)
        if matches:
            lib_targets[section].extend([f"//{section}:{name}" for _, name in matches])

# 分离 lib 和 include 路径采集逻辑
def collect_include_paths_flat(base_folder):
    """只收集 include 目录（所有目录合并到一个变量）"""
    result = set()
    base_path = os.path.join(project_root, base_folder)
    for root, dirs, files in os.walk(base_path):
        if "include" in dirs:
            include_path = os.path.join(root, "include")
            rel = os.path.relpath(include_path, project_root)
            result.add(f"\"//{rel}\"")
    return sorted(result)

def collect_lib_paths_split(base_folder):
    """保留每个 lib 或其子目录独立变量"""
    result = {}
    base_path = os.path.join(project_root, base_folder)
    for root, dirs, files in os.walk(base_path):
        if "lib" in dirs:
            base_target_dir = os.path.join(root, "lib")
            subdirs = [os.path.join(base_target_dir, d) for d in os.listdir(base_target_dir)
                       if os.path.isdir(os.path.join(base_target_dir, d))]
            if subdirs:
                for subdir in subdirs:
                    rel = os.path.relpath(subdir, project_root)
                    key = os.path.relpath(subdir, project_root).replace("/", "_").replace("-", "_")
                    result[f"{key}"] = [f"\"//{rel}\""]
            else:
                rel = os.path.relpath(base_target_dir, project_root)
                key = os.path.relpath(base_target_dir, project_root).replace("/", "_").replace("-", "_")
                result[f"{key}"] = [f"\"//{rel}\""]
    return result

def collect_individual_src_lib_sources():
    result = {}
    src_lib_path = os.path.join(project_root, "src", "lib")
    if os.path.isdir(src_lib_path):
        for fname in os.listdir(src_lib_path):
            if fname.endswith((".c", ".cpp")):
                var_base = os.path.splitext(fname)[0].replace("-", "_")
                var_name = f"src_{var_base}"
                rel_path = os.path.relpath(os.path.join("src", "lib", fname), project_root)
                result[var_name] = f"\"//{rel_path}\""
    return result

# 聚合数据
include_flat = {
    "thirdparty_include": collect_include_paths_flat("thirdparty"),
    "src_include": collect_include_paths_flat("src")
}
lib_split = {}
for section in ["thirdparty", "src"]:
    lib_split.update(collect_lib_paths_split(section))
individual_src_vars = collect_individual_src_lib_sources()

# 写入 .gni
gni_path = os.path.join(project_root, "lib_path.gni")
with open(gni_path, "w", encoding="utf-8") as gni:
    gni.write("# Auto-generated GN include\n\n")

    for section in ["thirdparty", "src"]:
        libs_var = f"{section}_libs".replace("/", "_")
        gni.write(f"{libs_var} = [\n")
        for lib in lib_targets[section]:
            gni.write(f"  \"{lib}\",\n")
        gni.write("]\n\n")

    for var, paths in include_flat.items():
        gni.write(f"{var} = [\n")
        for p in paths:
            gni.write(f"  {p},\n")
        gni.write("]\n\n")

    for var, paths in lib_split.items():
        gni.write(f"{var} = [\n")
        for p in paths:
            gni.write(f"  {p},\n")
        gni.write("]\n\n")

    for var, path in individual_src_vars.items():
        gni.write(f"{var} = [ {path} ]\n")

