# vcbuild

*A tool to break free from Visual Studio project files and bring Windows C++ development with MSVC directly into a VS Code–centric workflow, fitting naturally into multi-language projects.*

A lightweight, MSVC-based command-line build system for Windows C++ projects.

## Requirements

- Python 3.10+
- Visual Studio 2019/2022 with C++ workload

## Installation

Add as a git submodule to your project:

```bash
git submodule add https://github.com/user/vcbuild.git vcbuild
```

## Usage

```bash
# Build with release profile (default)
python vcbuild/vcbuild.py

# Build with debug profile
python vcbuild/vcbuild.py debug

# Add compile definitions
python vcbuild/vcbuild.py -D ENABLE_LOGGING -D VERSION=2

# Clean build artifacts
python vcbuild/vcbuild.py --clean

# Generate config file
python vcbuild/vcbuild.py --init

# Show resolved configuration
python vcbuild/vcbuild.py --show-config

# Preview build command
python vcbuild/vcbuild.py --dry-run
```

## Configuration

Create `vcbuild.json` in your project root. All fields are optional.

```json
{
  "project": {
    "name": "MyApp",
    "type": "exe",
    "architecture": "x64"
  },
  "compiler": {
    "standard": "c++20",
    "defines": ["WIN32_LEAN_AND_MEAN"]
  },
  "linker": {
    "libraries": ["user32.lib"],
    "subsystem": "windows"
  },
  "sources": {
    "source_dirs": ["src"],
    "include_dirs": ["src", "include"]
  }
}
```

## Defaults

Without configuration, vcbuild uses sensible defaults:

- C++20 standard
- x64 architecture
- Warning level 4
- Security hardening (ASLR, DEP, /GS)
- Strict conformance (/permissive-)
- Release: Full optimization, LTO
- Debug: No optimization, full debug info

## GUI Configurator

Build the optional GUI for visual configuration:

```bash
cd vcbuild/gui
build.bat
```

Launch from your project directory:

```bash
vcbuild/gui/build/vcbuild_confgen.exe
```

Or via CLI:

```bash
python vcbuild/vcbuild.py --config-gui
```

## CLI Reference

| Option | Description |
|--------|-------------|
| `debug`/`release` | Build profile |
| `-D NAME[=VAL]` | Add preprocessor define |
| `--arch` | Target architecture (x86/x64/arm64) |
| `--standard` | C++ standard (c++17/c++20/c++23) |
| `-o NAME` | Output filename |
| `--clean` | Remove build artifacts |
| `--rebuild` | Clean and rebuild |
| `--init` | Generate vcbuild.json |
| `--show-config` | Print resolved config |
| `--dry-run` | Show command without running |
| `-v` | Verbose output |
| `--config-gui` | Launch GUI configurator |

## Configuration Reference

See [docs/flags_reference.md](docs/flags_reference.md) for MSVC flags documentation.

### Project

| Field | Values | Default |
|-------|--------|---------|
| `name` | string | folder name |
| `type` | exe, dll, lib | exe |
| `architecture` | x86, x64, arm64 | x64 |
| `output_dir` | path | build |
| `output_name` | string | {name}.{ext} |

### Compiler

| Field | Values | Default |
|-------|--------|---------|
| `standard` | c++17, c++20, c++23, c++latest | c++20 |
| `runtime` | dynamic, static | dynamic |
| `optimization` | auto, none, size, speed, full | auto |
| `debug_info` | auto, none, minimal, full | auto |
| `parallel` | bool | true |
| `exceptions` | bool | true |
| `defines` | string[] | [] |
| `warnings.level` | 0-4 | 4 |
| `warnings.as_errors` | bool | false |
| `conformance.permissive` | bool | false |
| `security.buffer_checks` | bool | true |
| `security.control_flow_guard` | bool | false |

### Linker

| Field | Values | Default |
|-------|--------|---------|
| `libraries` | string[] | [] |
| `library_paths` | string[] | [] |
| `subsystem` | console, windows, native | console |
| `lto` | auto, off, on | auto |
| `strip_unreferenced` | auto, off, on | auto |
| `security.aslr` | bool | true |
| `security.dep` | bool | true |

### Sources

| Field | Default |
|-------|---------|
| `source_dirs` | ["src"] |
| `include_dirs` | ["src", "include"] |
| `extensions` | [".cpp", ".cc", ".c", ".cxx"] |
| `exclude_patterns` | [] |
| `external_dirs` | [] |
