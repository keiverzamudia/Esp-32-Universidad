import os
import subprocess

Import("env")

framework_dir = env.PioPlatform().get_package_dir("framework-espidf")
cmake_bin = os.path.join(
    env.PioPlatform().get_package_dir("tool-cmake"), "bin", "cmake"
)
embed_script = os.path.join(
    framework_dir, "tools", "cmake", "scripts", "data_file_embed_asm.cmake"
)

project_dir = env.subst("$PROJECT_DIR")
build_dir = env.subst("$BUILD_DIR")

text_files = [
    os.path.join(project_dir, "web", "index.html"),
    os.path.join(project_dir, "web", "app.css"),
    os.path.join(project_dir, "web", "app.js"),
    os.path.join(project_dir, "evidence_build", "manifest.json"),
]
bin_files = [
    os.path.join(project_dir, "evidence_build", "%s_%s.jpg" % (eid, kind))
    for eid in ("ev01", "ev02", "ev03", "ev04", "ev05", "ev06")
    for kind in ("full", "thumb")
]

os.makedirs(build_dir, exist_ok=True)
for src, ftype in [(f, "TEXT") for f in text_files] + [(f, "BINARY") for f in bin_files]:
    if not os.path.isfile(src):
        print("TRACE-X embed: MISSING %s" % src)
        continue
    out = os.path.join(build_dir, os.path.basename(src) + ".S")
    subprocess.check_call(
        [
            cmake_bin,
            "-DDATA_FILE=" + os.path.abspath(src),
            "-DSOURCE_FILE=" + os.path.abspath(out),
            "-DFILE_TYPE=" + ftype,
            "-P",
            embed_script,
        ],
        cwd=build_dir,
    )
print("TRACE-X embed: assembly files generated in %s" % build_dir)
