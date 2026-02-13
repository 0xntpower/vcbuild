"""MSVC toolchain detection and invocation."""

import os
import subprocess
import shutil
import time
from pathlib import Path
from dataclasses import dataclass

from .config import Config
from .discovery import SourceSet
from . import output

VCVARS_LOCATIONS = [
    r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat",
    r"C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat",
    r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat",
    r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat",
    r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvarsall.bat",
    r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat",
]

WDK_LOCATIONS = [
    r"C:\Program Files (x86)\Windows Kits\10",
    r"C:\Program Files\Windows Kits\10",
]

@dataclass
class BuildResult:
    success: bool
    output_path: Path | None
    errors: list[str]
    warnings: list[str]
    duration: float = 0.0
    files_compiled: int = 0

def find_vcvars() -> Path | None:
    for loc in VCVARS_LOCATIONS:
        path = Path(loc)
        if path.exists():
            return path
    return None

def find_wdk() -> tuple[Path | None, str | None]:
    """Find WDK installation and latest version."""
    for loc in WDK_LOCATIONS:
        kit_root = Path(loc)
        if not kit_root.exists():
            continue
        inc_dir = kit_root / "Include"
        if not inc_dir.exists():
            continue
        versions = sorted(
            [d.name for d in inc_dir.iterdir()
             if d.is_dir() and d.name.startswith("10.")],
            reverse=True
        )
        if versions:
            return kit_root, versions[0]
    return None, None

def _cl_standard(std: str) -> str:
    return {
        "c11": "/std:c11",
        "c17": "/std:c17",
        "c++17": "/std:c++17",
        "c++20": "/std:c++20",
        "c++23": "/std:c++23",
        "c++latest": "/std:c++latest"
    }.get(std, "/std:c++20")

def _cl_optimization(opt: str, profile: str) -> list[str]:
    if opt == "auto":
        opt = "full" if profile == "release" else "none"

    return {
        "none": ["/Od"],
        "size": ["/O1"],
        "speed": ["/O2"],
        "full": ["/O2", "/GL"]
    }.get(opt, ["/O2"])

def _cl_debug(dbg: str, profile: str) -> list[str]:
    if dbg == "auto":
        dbg = "minimal" if profile == "release" else "full"

    return {
        "none": [],
        "minimal": ["/Zi"],
        "full": ["/Zi", "/RTC1"]
    }.get(dbg, [])

def _cl_runtime(rt: str, profile: str) -> str:
    debug = profile == "debug"
    if rt == "static":
        return "/MTd" if debug else "/MT"
    return "/MDd" if debug else "/MD"

def _cl_warnings(cfg: dict) -> list[str]:
    flags = [f"/W{cfg.get('level', 4)}"]
    if cfg.get("as_errors"):
        flags.append("/WX")
    for w in cfg.get("disabled", []):
        flags.append(f"/wd{w}")
    return flags

def _cl_floating_point(fp: str) -> str:
    return {
        "precise": "/fp:precise",
        "fast": "/fp:fast",
        "strict": "/fp:strict"
    }.get(fp, "/fp:precise")

def _cl_calling_convention(cc: str) -> str:
    return {
        "cdecl": "/Gd",
        "stdcall": "/Gr",
        "fastcall": "/Gz",
        "vectorcall": "/Gv"
    }.get(cc, "/Gd")

def _cl_char_set(cs: str) -> list[str]:
    if cs == "unicode":
        return ["/DUNICODE", "/D_UNICODE"]
    elif cs == "mbcs":
        return ["/D_MBCS"]
    return []

def _link_security(cfg: dict) -> list[str]:
    flags = []
    if cfg.get("aslr", True):
        flags.append("/DYNAMICBASE")
    if cfg.get("dep", True):
        flags.append("/NXCOMPAT")
    if cfg.get("cfg", False):
        flags.append("/guard:cf")
    return flags

def _link_lto(lto: str, profile: str) -> list[str]:
    if lto == "auto":
        lto = "on" if profile == "release" else "off"
    return ["/LTCG"] if lto == "on" else []

def _link_strip(strip: str, profile: str) -> list[str]:
    if strip == "auto":
        strip = "on" if profile == "release" else "off"
    return ["/OPT:REF", "/OPT:ICF"] if strip == "on" else []

def _driver_compiler_flags(drv_cfg: dict) -> list[str]:
    """Generate compiler flags for kernel driver builds."""
    flags = ["/kernel", "/GS-", "/Gy"]
    if drv_cfg.get("type") == "kmdf":
        flags.append("/DKMDF_VERSION_MAJOR=1")
    return flags

def _driver_linker_flags(drv_cfg: dict) -> list[str]:
    """Generate linker flags for kernel driver builds."""
    flags = [
        "/DRIVER",
        "/INTEGRITYCHECK",
        "/MERGE:.rdata=.text",
    ]

    drv_type = drv_cfg.get("type", "wdm")
    if drv_type == "wdm":
        flags.append("/DRIVER:WDM")

    entry = drv_cfg.get("entry_point", "DriverEntry")
    flags.append(f"/ENTRY:{entry}")

    return flags

def _driver_include_paths(wdk_root: Path, wdk_version: str,
                          arch: str, drv_cfg: dict) -> list[str]:
    """Generate WDK include paths."""
    base_inc = wdk_root / "Include" / wdk_version
    paths = [
        str(base_inc / "km"),
        str(base_inc / "shared"),
        str(base_inc / "km" / "crt"),
    ]
    return paths

def _driver_lib_paths(wdk_root: Path, wdk_version: str,
                      arch: str) -> list[str]:
    """Generate WDK library paths."""
    lib_root = wdk_root / "Lib" / wdk_version
    return [
        str(lib_root / "km" / arch),
    ]

def _driver_libraries(drv_cfg: dict) -> list[str]:
    """Generate required driver libraries."""
    libs = ["ntoskrnl.lib", "hal.lib", "wmilib.lib"]
    if drv_cfg.get("minifilter"):
        libs.append("fltMgr.lib")
    drv_type = drv_cfg.get("type", "wdm")
    if drv_type == "kmdf":
        libs.extend(["WdfLdr.lib", "WdfDriverEntry.lib"])
    return libs

def _driver_defines(drv_cfg: dict) -> list[str]:
    """Generate kernel-mode defines."""
    defines = ["_KERNEL_MODE", "NTDDI_WIN10_RS5=0x0A000006"]
    target_os = drv_cfg.get("target_os", "win10")
    ntddi_map = {
        "win7":  ("0x06010000", "0x0601"),
        "win8":  ("0x06020000", "0x0602"),
        "win81": ("0x06030000", "0x0603"),
        "win10": ("0x0A000000", "0x0A00"),
        "win11": ("0x0A00000B", "0x0A00"),
    }
    ntddi, winver = ntddi_map.get(target_os, ("0x0A000000", "0x0A00"))
    defines.append(f"NTDDI_VERSION={ntddi}")
    defines.append(f"_WIN32_WINNT={winver}")
    if drv_cfg.get("minifilter"):
        defines.append("FLT_REGISTRATION_VERSION=0x0203")
    return defines


def build_command(config: Config, sources: SourceSet, profile: str) -> tuple[str, Path]:
    cfg = config.data
    comp = cfg.get("compiler", {})
    link = cfg.get("linker", {})
    proj = cfg.get("project", {})
    drv_cfg = cfg.get("driver", {})
    is_driver = drv_cfg.get("enabled", False)

    output_dir = config.project_root / proj.get("output_dir", "build")
    output_dir.mkdir(parents=True, exist_ok=True)
    output_path = output_dir / proj.get("output_name", "output.exe")
    obj_dir = output_dir / "obj"
    obj_dir.mkdir(exist_ok=True)

    cl_flags = []
    cl_flags.append(_cl_standard(comp.get("standard", "c++20")))
    cl_flags.append(_cl_runtime(comp.get("runtime", "dynamic"), profile))
    cl_flags.extend(_cl_optimization(comp.get("optimization", "auto"), profile))
    cl_flags.extend(_cl_debug(comp.get("debug_info", "auto"), profile))
    cl_flags.extend(_cl_warnings(comp.get("warnings", {})))

    if comp.get("exceptions", True) and not is_driver:
        cl_flags.append("/EHsc")

    if not comp.get("rtti", True):
        cl_flags.append("/GR-")

    cl_flags.append(_cl_floating_point(comp.get("floating_point", "precise")))
    cl_flags.append(_cl_calling_convention(comp.get("calling_convention", "cdecl")))
    cl_flags.extend(_cl_char_set(comp.get("char_set", "unicode")))

    if comp.get("function_level_linking", True):
        cl_flags.append("/Gy")

    if comp.get("string_pooling", True):
        cl_flags.append("/GF")

    conformance = comp.get("conformance", {})
    if not conformance.get("permissive", False):
        cl_flags.append("/permissive-")
    if conformance.get("cplusplus_macro", True):
        cl_flags.append("/Zc:__cplusplus")

    cl_flags.append("/utf-8")

    security = comp.get("security", {})
    if is_driver:
        cl_flags.extend(_driver_compiler_flags(drv_cfg))
        for d in _driver_defines(drv_cfg):
            cl_flags.append(f"/D{d}")
    else:
        if security.get("buffer_checks", True):
            cl_flags.append("/GS")
        if security.get("control_flow_guard", False):
            cl_flags.append("/guard:cf")

    if comp.get("parallel", True):
        cl_flags.append("/MP")

    for d in comp.get("defines", []):
        cl_flags.append(f"/D{d}")

    cl_flags.extend(comp.get("additional_flags", []))

    # PCH support
    pch_cfg = cfg.get("pch", {})
    if pch_cfg.get("enabled") and pch_cfg.get("header"):
        pch_header = pch_cfg["header"]
        pch_out = output_dir / "pch.pch"
        cl_flags.append(f'/Yu"{pch_header}"')
        cl_flags.append(f'/Fp"{pch_out}"')

    for inc in sources.include_dirs:
        cl_flags.append(f'/I"{inc}"')

    # Driver WDK include paths
    if is_driver:
        wdk_root, wdk_ver = find_wdk()
        if wdk_root and wdk_ver:
            arch = proj.get("architecture", "x64")
            for p in _driver_include_paths(wdk_root, wdk_ver, arch, drv_cfg):
                cl_flags.append(f'/I"{p}"')

    cl_flags.append(f'/Fo"{obj_dir}\\\\"')
    cl_flags.append(f'/Fd"{output_dir}\\vc.pdb"')

    link_flags = []

    if link.get("generate_debug_info", True):
        link_flags.append("/DEBUG")

    link_sec = link.get("security", {})
    if not is_driver:
        link_flags.extend(_link_security(link_sec))
    link_flags.extend(_link_lto(link.get("lto", "auto"), profile))
    link_flags.extend(_link_strip(link.get("strip_unreferenced", "auto"), profile))

    subsystem = link.get("subsystem", "console")
    if is_driver:
        subsystem = "native"
    link_flags.append(f"/SUBSYSTEM:{subsystem.upper()}")

    # Driver-specific linker flags
    if is_driver:
        link_flags.extend(_driver_linker_flags(drv_cfg))
        wdk_root, wdk_ver = find_wdk()
        if wdk_root and wdk_ver:
            arch = proj.get("architecture", "x64")
            for lp in _driver_lib_paths(wdk_root, wdk_ver, arch):
                link_flags.append(f'/LIBPATH:"{lp}"')
            for lib in _driver_libraries(drv_cfg):
                link_flags.append(lib)

    entry_point = link.get("entry_point")
    if entry_point and not is_driver:
        link_flags.append(f"/ENTRY:{entry_point}")

    def_file = link.get("def_file")
    if def_file:
        def_path = config.project_root / def_file
        link_flags.append(f'/DEF:"{def_path}"')

    stack_size = link.get("stack_size")
    if stack_size:
        link_flags.append(f"/STACK:{stack_size}")

    heap_size = link.get("heap_size")
    if heap_size:
        link_flags.append(f"/HEAP:{heap_size}")

    if link.get("generate_map", False):
        map_file = output_dir / (proj.get("name", "output") + ".map")
        link_flags.append(f'/MAP:"{map_file}"')

    for lib in link.get("libraries", []):
        link_flags.append(f"{lib}")

    for lp in link.get("library_paths", []):
        link_flags.append(f'/LIBPATH:"{lp}"')

    link_flags.extend(link.get("additional_flags", []))

    link_flags.append(f'/OUT:"{output_path}"')

    source_list = " ".join(f'"{s}"' for s in sources.sources)
    cl_cmd = f'cl.exe /nologo {" ".join(cl_flags)} {source_list} /link {" ".join(link_flags)}'

    return cl_cmd, output_path

def compile_pch(config: Config, vcvars: Path, arch: str,
                profile: str, sources: SourceSet,
                verbose: bool = False) -> bool:
    """Compile precompiled header if configured."""
    pch_cfg = config.data.get("pch", {})
    if not pch_cfg.get("enabled") or not pch_cfg.get("header"):
        return True

    pch_source = pch_cfg.get("source")
    if not pch_source:
        return True

    pch_src_path = config.project_root / pch_source
    if not pch_src_path.exists():
        output.error(f"PCH source not found: {pch_src_path}")
        return False

    comp = config.data.get("compiler", {})
    output_dir = config.project_root / config.data.get("project", {}).get("output_dir", "build")
    output_dir.mkdir(parents=True, exist_ok=True)
    obj_dir = output_dir / "obj"
    obj_dir.mkdir(exist_ok=True)
    pch_out = output_dir / "pch.pch"

    cl_flags = ["/nologo"]
    cl_flags.append(_cl_standard(comp.get("standard", "c++20")))
    cl_flags.append(_cl_runtime(comp.get("runtime", "dynamic"), profile))
    cl_flags.extend(_cl_optimization(comp.get("optimization", "auto"), profile))
    cl_flags.extend(_cl_debug(comp.get("debug_info", "auto"), profile))
    if comp.get("exceptions", True):
        cl_flags.append("/EHsc")

    for inc in sources.include_dirs:
        cl_flags.append(f'/I"{inc}"')

    cl_flags.append(f'/Yc"{pch_cfg["header"]}"')
    cl_flags.append(f'/Fp"{pch_out}"')
    cl_flags.append(f'/Fo"{obj_dir}\\\\"')
    cl_flags.append("/c")

    pch_cmd = f'cl.exe {" ".join(cl_flags)} "{pch_src_path}"'
    full_cmd = f'"{vcvars}" {arch} >nul 2>&1 && {pch_cmd}'

    if verbose:
        output.detail(f"Compiling PCH: {pch_cfg['header']}")

    result = subprocess.run(full_cmd, shell=True, capture_output=True, text=True)
    if result.returncode != 0:
        output.error("PCH compilation failed")
        if result.stderr:
            output.detail(result.stderr.strip())
        return False

    return True

def compile_resources(config: Config, vcvars: Path, arch: str,
                      verbose: bool = False) -> list[Path]:
    """Compile .rc files to .res if resources are configured."""
    res_cfg = config.data.get("resources", {})
    if not res_cfg.get("enabled", False):
        return []

    files = res_cfg.get("files", [])
    if not files:
        return []

    output_dir = config.project_root / config.data.get("project", {}).get("output_dir", "build")
    output_dir.mkdir(parents=True, exist_ok=True)

    inc_dirs = config.data.get("sources", {}).get("include_dirs", [])
    inc_flags = " ".join(f'/i"{config.project_root / d}"' for d in inc_dirs)

    compiled = []
    for rc_name in files:
        rc_file = config.project_root / rc_name
        if not rc_file.exists():
            output.error(f"Resource file not found: {rc_file}")
            continue

        res_file = output_dir / (rc_file.stem + ".res")
        rc_cmd = f'rc /nologo {inc_flags} /fo"{res_file}" "{rc_file}"'
        full_cmd = f'"{vcvars}" {arch} >nul 2>&1 && {rc_cmd}'

        if verbose:
            output.detail(f"Compiling resources: {rc_file.name}")

        result = subprocess.run(full_cmd, shell=True, capture_output=True, text=True)

        if result.returncode != 0 or not res_file.exists():
            output.error(f"Resource compilation failed: {rc_file.name}")
            if result.stderr:
                output.detail(result.stderr.strip())
            return []

        compiled.append(res_file)

    return compiled

def build(config: Config, sources: SourceSet, profile: str,
          verbose: bool = False, dry_run: bool = False) -> BuildResult:

    start_time = time.perf_counter()

    vcvars = find_vcvars()
    if vcvars is None:
        output.error("MSVC not found", "Install Visual Studio with C++ workload")
        return BuildResult(False, None, ["MSVC not found"], [])

    arch = config.data.get("project", {}).get("architecture", "x64")

    # Driver WDK check
    drv_cfg = config.data.get("driver", {})
    if drv_cfg.get("enabled", False):
        wdk_root, wdk_ver = find_wdk()
        if wdk_root is None:
            output.error("WDK not found", "Install Windows Driver Kit for driver builds")
            return BuildResult(False, None, ["WDK not found"], [])
        output.detail(f"WDK {wdk_ver} at {wdk_root}")

    # Compile PCH if configured
    if not dry_run:
        if not compile_pch(config, vcvars, arch, profile, sources, verbose):
            return BuildResult(False, None, ["PCH compilation failed"], [])

    # Compile resources if configured
    res_files = []
    if not dry_run:
        res_files = compile_resources(config, vcvars, arch, verbose)
        if config.data.get("resources", {}).get("enabled", False) and not res_files:
            res_files_cfg = config.data.get("resources", {}).get("files", [])
            if res_files_cfg:
                return BuildResult(False, None, ["Resource compilation failed"], [])

    # Add resource files to linker flags
    for res_file in res_files:
        config.data.setdefault("linker", {}).setdefault("additional_flags", [])
        res_flag = str(res_file)
        if res_flag not in config.data["linker"]["additional_flags"]:
            config.data["linker"]["additional_flags"].append(res_flag)

    cl_cmd, output_path = build_command(config, sources, profile)

    full_cmd = f'"{vcvars}" {arch} >nul 2>&1 && {cl_cmd}'

    if dry_run:
        output.step("Command (dry run):")
        output.detail(cl_cmd)
        return BuildResult(True, output_path, [], [])

    if verbose:
        output.detail(cl_cmd)

    try:
        result = subprocess.run(
            full_cmd,
            shell=True,
            capture_output=True,
            text=True
        )

        errors = []
        warnings = []

        for line in (result.stdout + result.stderr).splitlines():
            line = line.strip()
            if not line:
                continue
            if ": error " in line or line.startswith("error "):
                errors.append(line)
            elif ": warning " in line:
                warnings.append(line)
            elif verbose:
                output.detail(line)

        success = result.returncode == 0 and output_path.exists()

        cleanup_stray_files(config.project_root, verbose)

        elapsed = time.perf_counter() - start_time

        return BuildResult(
            success=success,
            output_path=output_path if success else None,
            errors=errors,
            warnings=warnings,
            duration=elapsed,
            files_compiled=len(sources.sources)
        )

    except Exception as e:
        cleanup_stray_files(config.project_root, verbose)
        elapsed = time.perf_counter() - start_time
        return BuildResult(False, None, [str(e)], [], duration=elapsed)

def cleanup_stray_files(project_root: Path, verbose: bool = False):
    """Remove stray intermediate files from project root."""
    patterns = ["*.obj", "*.pdb", "*.idb", "*.ilk"]
    cleaned = []

    for pattern in patterns:
        for file in project_root.glob(pattern):
            if file.parent == project_root:
                try:
                    file.unlink()
                    cleaned.append(file.name)
                except Exception:
                    pass

    if cleaned and verbose:
        output.detail(f"Cleaned up: {', '.join(cleaned)}")

def clean(config: Config):
    output_dir = config.project_root / config.data.get("project", {}).get("output_dir", "build")
    if output_dir.exists():
        shutil.rmtree(output_dir)
        output.success(f"Cleaned {output_dir}")
    else:
        output.detail("Nothing to clean")

    cleanup_stray_files(config.project_root, verbose=True)
