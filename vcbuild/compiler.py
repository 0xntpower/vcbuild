"""MSVC toolchain detection and invocation."""

import os
import subprocess
import shutil
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

@dataclass
class BuildResult:
    success: bool
    output_path: Path | None
    errors: list[str]
    warnings: list[str]

def find_vcvars() -> Path | None:
    for loc in VCVARS_LOCATIONS:
        path = Path(loc)
        if path.exists():
            return path
    return None

def _cl_standard(std: str) -> str:
    return {
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

def build_command(config: Config, sources: SourceSet, profile: str) -> tuple[str, Path]:
    cfg = config.data
    comp = cfg.get("compiler", {})
    link = cfg.get("linker", {})
    proj = cfg.get("project", {})
    
    output_dir = config.project_root / cfg.get("project", {}).get("output_dir", "build")
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
    
    if comp.get("exceptions", True):
        cl_flags.append("/EHsc")
    
    conformance = comp.get("conformance", {})
    if not conformance.get("permissive", False):
        cl_flags.append("/permissive-")
    if conformance.get("cplusplus_macro", True):
        cl_flags.append("/Zc:__cplusplus")
    
    cl_flags.append("/utf-8")
    
    security = comp.get("security", {})
    if security.get("buffer_checks", True):
        cl_flags.append("/GS")
    if security.get("control_flow_guard", False):
        cl_flags.append("/guard:cf")
    
    if comp.get("parallel", True):
        cl_flags.append("/MP")
    
    for d in comp.get("defines", []):
        if "=" in d:
            cl_flags.append(f"/D{d}")
        else:
            cl_flags.append(f"/D{d}")
    
    cl_flags.extend(comp.get("additional_flags", []))

    for inc in sources.include_dirs:
        cl_flags.append(f'/I"{inc}"')

    # Specify object file and PDB output directories (compiler flags, not linker flags)
    cl_flags.append(f'/Fo"{obj_dir}\\\\"')
    cl_flags.append(f'/Fd"{output_dir}\\vc.pdb"')

    link_flags = []
    link_sec = link.get("security", {})
    link_flags.extend(_link_security(link_sec))
    link_flags.extend(_link_lto(link.get("lto", "auto"), profile))
    link_flags.extend(_link_strip(link.get("strip_unreferenced", "auto"), profile))
    
    subsystem = link.get("subsystem", "console")
    link_flags.append(f"/SUBSYSTEM:{subsystem.upper()}")
    
    for lib in link.get("libraries", []):
        link_flags.append(f"{lib}")
    
    for lp in link.get("library_paths", []):
        link_flags.append(f'/LIBPATH:"{lp}"')
    
    link_flags.extend(link.get("additional_flags", []))

    link_flags.append(f'/OUT:"{output_path}"')

    source_list = " ".join(f'"{s}"' for s in sources.sources)
    cl_cmd = f'cl.exe /nologo {" ".join(cl_flags)} {source_list} /link {" ".join(link_flags)}'
    
    return cl_cmd, output_path

def build(config: Config, sources: SourceSet, profile: str, 
          verbose: bool = False, dry_run: bool = False) -> BuildResult:
    
    vcvars = find_vcvars()
    if vcvars is None:
        output.error("MSVC not found", "Install Visual Studio with C++ workload")
        return BuildResult(False, None, ["MSVC not found"], [])
    
    arch = config.data.get("project", {}).get("architecture", "x64")
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

        # Clean up any stray intermediate files from project root
        cleanup_stray_files(config.project_root, verbose)

        return BuildResult(
            success=success,
            output_path=output_path if success else None,
            errors=errors,
            warnings=warnings
        )

    except Exception as e:
        # Clean up stray files even on exception
        cleanup_stray_files(config.project_root, verbose)
        return BuildResult(False, None, [str(e)], [])

def cleanup_stray_files(project_root: Path, verbose: bool = False):
    """Remove stray intermediate files from project root."""
    patterns = ["*.obj", "*.pdb", "*.idb", "*.ilk"]
    cleaned = []

    for pattern in patterns:
        for file in project_root.glob(pattern):
            # Only remove files in the root, not in subdirectories
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

    # Also cleanup any stray intermediate files
    cleanup_stray_files(config.project_root, verbose=True)
