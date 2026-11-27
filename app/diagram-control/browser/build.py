"""固定版wccの配布資産と、共通C++ VMのブラウザ用Wasmをビルドする。テストは実行しない。"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import zipfile


def run(command, cwd):
    """失敗時にビルドを中断し、診断をそのまま表示する。"""
    subprocess.run([str(item) for item in command], cwd=cwd, check=True)


def build_wcc(root, sdk, output):
    """固定ソースをEmscriptenでbootstrapし、上流のwccでライブラリーとcc.wasmを生成する。"""
    emcc = sdk / "upstream/emscripten/emcc.bat"
    node = next((sdk / "node").glob("*/bin/node.exe"), None)
    if node is None:
        node = next((sdk / "node").glob("*/node.exe"))
    ar = sdk / "upstream/bin/llvm-ar.exe"
    sources = sorted(root.glob("src/wcc/*.c")) + sorted(root.glob("src/cc/frontend/*.c"))
    sources += [root / name for name in ["src/util/archive.c", "src/util/util.c", "src/util/table.c",
                                        "src/cpp/preprocessor.c", "src/cpp/pp_parser.c", "src/cpp/macro.c"]]
    includes = ["-Isrc/util", "-Isrc/cpp", "-Isrc/cc/frontend"]
    bootstrap = root / "bootstrap.cjs"
    pre = root / "bootstrap-pre.js"
    pre.write_text("Module.preRun = [() => { for (const dir of ['/usr', '/work']) { FS.mkdir(dir); "
                   "FS.mount(NODEFS, {root: " + json.dumps(str(root)) + "}, dir); } FS.chdir('/work'); }];", encoding="utf-8")
    run([emcc, *sources, *includes, "-DXCC_TARGET_ARCH=XCC_ARCH_WASM", "-D_DEFAULT_SOURCE",
         "-O2", "-lnodefs.js", "--pre-js", pre, "-sENVIRONMENT=node", "-sALLOW_MEMORY_GROWTH=1", "-sEXIT_RUNTIME=1",
         "-o", bootstrap], root)
    compiler = [node, bootstrap]
    obj = root / "obj/browser"
    obj.mkdir(parents=True, exist_ok=True)
    (root / "lib").mkdir(exist_ok=True)
    for library, patterns in [
        ("wcrt0.a", ["libsrc/_wasm/crt0/*.c"]),
        ("wlibc.a", ["libsrc/math/*.c", "libsrc/misc/*.c", "libsrc/stdio/*.c", "libsrc/stdlib/*.c",
                      "libsrc/string/*.c", "libsrc/_wasm/unistd/*.c"]),
    ]:
        objects = []
        for source in sorted(path for pattern in patterns for path in root.glob(pattern)):
            target = obj / (source.stem + ".o")
            run([*compiler, "-c", "-o", target.relative_to(root).as_posix(), source.relative_to(root).as_posix()], root)
            objects.append(target)
        run([ar, "rc", root / "lib" / library, *objects], root)
    run([*compiler, *[source.relative_to(root).as_posix() for source in sources], *includes,
         "-DXCC_TARGET_ARCH=XCC_ARCH_WASM", "-DSELF_HOSTING", "-o", "cc.wasm"], root)
    layout = json.loads((root / "src/wcc/www/lib_list.json").read_text(encoding="utf-8"))
    with zipfile.ZipFile(output / "wccfiles.zip", "w", compression=zipfile.ZIP_DEFLATED) as archive:
        def add(tree, prefix=""):
            for name, value in tree.items():
                destination = prefix + name
                if isinstance(value, dict):
                    add(value, destination + "/")
                else:
                    source = root / value
                    data = source.read_bytes()
                    if source.suffix not in [".wasm", ".a"]:
                        text = data.decode("utf-8")
                        text = re.sub(r'^(#include\s+)"\..+/([\w\d\-_.]+)"$', r'\1"\2"', text, flags=re.M)
                        data = text.encode("utf-8")
                    archive.writestr(destination, data)
        add(layout)
    (output / "wcc-LICENSE.txt").write_bytes((root / "LICENSE").read_bytes())


def main():
    """SDKと固定ソースの場所を明示し、成果物をWebのpublic配下へ置く。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--emsdk", type=Path, required=True)
    parser.add_argument("--xcc", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--wcc-only", action="store_true")
    args = parser.parse_args()
    sdk = args.emsdk.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if args.xcc:
        build_wcc(args.xcc.resolve(), sdk, output)
    if args.wcc_only:
        return
    project = Path(__file__).resolve().parent.parent
    emcc = sdk / "upstream/emscripten/em++.bat"
    version = subprocess.check_output([str(emcc), "--version"], text=True)
    if "4.0.23" not in version.splitlines()[0]:
        raise RuntimeError("Emscripten 4.0.23を指定してください")
    exports = ["malloc", "free", "browser_load", "browser_tick", "browser_stop", "browser_state",
               "browser_error", "browser_pc", "browser_instructions", "browser_global_count", "browser_value",
               "browser_register_kernel", "browser_kernel_hash"]
    run([emcc, project / "browser/host.cpp", *[project / "src" / name for name in
         ["bytecode_vm.cpp", "bytecode_format.cpp", "bytecode_heap.cpp", "numpy_native.cpp"]],
         "-I" + str(project / "include"), "-std=c++17", "-O2", "-sMODULARIZE=1", "-sEXPORT_ES6=1",
         "-sENVIRONMENT=worker", "-sALLOW_MEMORY_GROWTH=1", "-sFILESYSTEM=0", "-sEXPORT_NAME=createPythonVM",
         "-sEXPORTED_FUNCTIONS=" + json.dumps(["_" + name for name in exports]),
         "-sEXPORTED_RUNTIME_METHODS=[\"HEAPU8\",\"HEAPF32\"]", "-o", output / "python-vm.mjs"], project)
    license_source = sdk / "upstream/emscripten/LICENSE"
    (output / "emscripten-LICENSE.txt").write_bytes(license_source.read_bytes())
    files = {}
    for name in ["python-vm.mjs", "python-vm.wasm", "wccfiles.zip"]:
        data = (output / name).read_bytes()
        files[name] = {"sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data)}
    manifest = {"wccCommit": "659a1773f20499864a8c3fdc7cd25cb506092bc1", "emscriptenVersion": "4.0.23", "files": files}
    (output / "assets.json").write_text(json.dumps(manifest, indent=4) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
