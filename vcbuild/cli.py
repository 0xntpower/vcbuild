"""Command-line interface."""

import sys
import argparse
import json
from pathlib import Path
from typing import NoReturn

from . import __version__
from . import output
from . import config as cfg
from . import discovery
from . import compiler
from . import cache

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog="vcbuild",
        description="Build Windows C++ projects using MSVC",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    
    parser.add_argument("profile", nargs="?", default="release",
                        choices=["debug", "release"],
                        help="Build profile (default: release)")
    
    parser.add_argument("-D", "--define", action="append", dest="defines",
                        metavar="NAME[=VALUE]", default=[],
                        help="Add preprocessor define")
    
    parser.add_argument("--arch", choices=["x86", "x64", "arm64"],
                        help="Target architecture")
    
    parser.add_argument("--standard", choices=["c++17", "c++20", "c++23"],
                        help="C++ standard")
    
    parser.add_argument("--output", "-o", metavar="NAME",
                        help="Output filename")
    
    parser.add_argument("--clean", action="store_true",
                        help="Remove build artifacts")
    
    parser.add_argument("--rebuild", action="store_true",
                        help="Clean and rebuild")
    
    parser.add_argument("--init", action="store_true",
                        help="Generate vcbuild.json template")
    
    parser.add_argument("--show-config", action="store_true",
                        help="Print resolved configuration")
    
    parser.add_argument("--dry-run", action="store_true",
                        help="Show build command without executing")
    
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Verbose output")
    
    parser.add_argument("--version", action="version",
                        version=f"vcbuild {__version__}")
    
    parser.add_argument("--config-gui", action="store_true",
                        help="Launch configuration GUI")
    
    return parser.parse_args()

def apply_overrides(config_data: dict, args: argparse.Namespace):
    if args.defines:
        config_data.setdefault("compiler", {}).setdefault("defines", [])
        config_data["compiler"]["defines"].extend(args.defines)
    
    if args.arch:
        config_data.setdefault("project", {})["architecture"] = args.arch
    
    if args.standard:
        config_data.setdefault("compiler", {})["standard"] = args.standard
    
    if args.output:
        config_data.setdefault("project", {})["output_name"] = args.output

def cmd_init(project_root: Path) -> int:
    dest = project_root / cfg.CONFIG_FILENAME
    if dest.exists():
        output.warning(f"{cfg.CONFIG_FILENAME} already exists")
        return 1
    cfg.generate_template(dest)
    return 0

def cmd_clean(config: cfg.Config) -> int:
    compiler.clean(config)
    build_cache = cache.BuildCache(config.project_root)
    build_cache.clear()
    return 0

def cmd_show_config(config: cfg.Config) -> int:
    print(json.dumps(config.data, indent=2))
    return 0

def cmd_config_gui(project_root: Path) -> int:
    gui_path = Path(__file__).parent.parent / "gui" / "vcbuild_gui.exe"
    
    if not gui_path.exists():
        output.error("GUI not found", f"Expected at {gui_path}")
        output.detail("Build the GUI from gui/vcbuild_gui.sln")
        return 1
    
    import subprocess
    config_path = project_root / cfg.CONFIG_FILENAME
    subprocess.Popen([str(gui_path), str(config_path)])
    return 0

def cmd_build(config: cfg.Config, profile: str, 
              verbose: bool, dry_run: bool) -> int:
    
    sources = discovery.discover(config)
    
    if not sources.sources:
        output.error("No source files found")
        src_dirs = config.data.get("sources", {}).get("source_dirs", ["src"])
        output.detail(f"Searched in: {', '.join(src_dirs)}")
        return 1
    
    output.step(f"Building {config.data['project']['name']} ({profile})")
    output.detail(f"{len(sources.sources)} source file(s)")
    
    if verbose:
        for src in sources.sources:
            output.detail(f"  {src.relative_to(config.project_root)}")
    
    result = compiler.build(config, sources, profile, verbose, dry_run)
    
    for w in result.warnings[:5]:
        output.warning(w)
    if len(result.warnings) > 5:
        output.detail(f"  ... and {len(result.warnings) - 5} more warning(s)")
    
    for e in result.errors:
        output.error(e)
    
    if result.success:
        if result.output_path:
            rel_path = result.output_path.relative_to(config.project_root)
            output.success(f"Built: {rel_path}")
        return 0
    else:
        output.error("Build failed")
        return 1

def main() -> int:
    args = parse_args()
    
    cwd = Path.cwd()
    project_root = cfg.find_project_root(cwd)
    
    if project_root is None:
        project_root = cwd
    
    if args.init:
        return cmd_init(project_root)
    
    if args.config_gui:
        return cmd_config_gui(project_root)
    
    try:
        config = cfg.load(project_root, args.profile)
    except SystemExit:
        return 1
    
    apply_overrides(config.data, args)
    
    if args.show_config:
        return cmd_show_config(config)
    
    if args.clean:
        return cmd_clean(config)
    
    if args.rebuild:
        cmd_clean(config)
    
    return cmd_build(config, args.profile, args.verbose, args.dry_run)
