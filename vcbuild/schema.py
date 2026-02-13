"""Configuration schema and defaults."""

from typing import Any

DEFAULTS: dict[str, Any] = {
    "project": {
        "name": None,
        "type": "exe",
        "output_dir": "build",
        "output_name": None,
        "architecture": "x64"
    },
    "compiler": {
        "standard": "c++20",
        "runtime": "dynamic",
        "optimization": "auto",
        "warnings": {
            "level": 4,
            "as_errors": False,
            "disabled": []
        },
        "defines": [],
        "conformance": {
            "permissive": False,
            "cplusplus_macro": True
        },
        "security": {
            "buffer_checks": True,
            "control_flow_guard": False
        },
        "debug_info": "auto",
        "parallel": True,
        "exceptions": True,
        "rtti": True,
        "floating_point": "precise",
        "calling_convention": "cdecl",
        "char_set": "unicode",
        "function_level_linking": True,
        "string_pooling": True,
        "additional_flags": []
    },
    "linker": {
        "libraries": [],
        "library_paths": [],
        "subsystem": "console",
        "security": {
            "aslr": True,
            "dep": True,
            "cfg": False
        },
        "lto": "auto",
        "strip_unreferenced": "auto",
        "entry_point": None,
        "def_file": None,
        "stack_size": None,
        "heap_size": None,
        "generate_map": False,
        "generate_debug_info": True,
        "additional_flags": []
    },
    "sources": {
        "root": ".",
        "include_dirs": ["src", "include"],
        "source_dirs": ["src"],
        "extensions": [".cpp", ".cc", ".c", ".cxx"],
        "exclude_patterns": [],
        "explicit_sources": [],
        "external_dirs": []
    },
    "profiles": {
        "debug": {
            "compiler": {
                "optimization": "none",
                "defines": ["_DEBUG"],
                "debug_info": "full"
            },
            "linker": {
                "lto": "off"
            }
        },
        "release": {
            "compiler": {
                "optimization": "full",
                "defines": ["NDEBUG"],
                "debug_info": "minimal"
            },
            "linker": {
                "lto": "on",
                "strip_unreferenced": "on"
            }
        }
    },
    "pch": {
        "enabled": False,
        "header": None,
        "source": None
    },
    "resources": {
        "enabled": False,
        "files": []
    },
    "driver": {
        "enabled": False,
        "type": "wdm",
        "entry_point": "DriverEntry",
        "target_os": "win10",
        "minifilter": False
    }
}

VALID_VALUES = {
    "project.type": ["exe", "dll", "lib", "sys"],
    "project.architecture": ["x86", "x64", "arm64"],
    "compiler.standard": ["c11", "c17", "c++17", "c++20", "c++23", "c++latest"],
    "compiler.runtime": ["dynamic", "static"],
    "compiler.optimization": ["auto", "none", "size", "speed", "full"],
    "compiler.debug_info": ["auto", "none", "minimal", "full"],
    "compiler.floating_point": ["precise", "fast", "strict"],
    "compiler.calling_convention": ["cdecl", "stdcall", "fastcall", "vectorcall"],
    "compiler.char_set": ["unicode", "mbcs", "none"],
    "linker.subsystem": ["console", "windows", "native", "efi_application",
                          "efi_boot_service_driver", "efi_runtime_driver",
                          "boot_application", "posix"],
    "linker.lto": ["auto", "off", "on"],
    "linker.strip_unreferenced": ["auto", "off", "on"],
    "driver.type": ["wdm", "kmdf", "wdf"],
    "driver.target_os": ["win7", "win8", "win81", "win10", "win11"]
}

def validate(config: dict, path: str = "") -> list[str]:
    errors = []

    for key, valid in VALID_VALUES.items():
        parts = key.split(".")
        val = config
        try:
            for p in parts:
                val = val[p]
            if val not in valid:
                errors.append(f"{key}: invalid value '{val}', expected one of {valid}")
        except (KeyError, TypeError):
            pass

    warn_level = config.get("compiler", {}).get("warnings", {}).get("level")
    if warn_level is not None:
        if not isinstance(warn_level, int) or warn_level < 0 or warn_level > 4:
            errors.append("compiler.warnings.level: must be integer 0-4")

    drv = config.get("driver", {})
    if drv.get("enabled"):
        proj_type = config.get("project", {}).get("type")
        if proj_type != "sys":
            errors.append("driver.enabled: project type must be 'sys' for driver builds")

    stack = config.get("linker", {}).get("stack_size")
    if stack is not None and not isinstance(stack, int):
        errors.append("linker.stack_size: must be integer (bytes)")

    heap = config.get("linker", {}).get("heap_size")
    if heap is not None and not isinstance(heap, int):
        errors.append("linker.heap_size: must be integer (bytes)")

    return errors

def deep_merge(base: dict, override: dict) -> dict:
    result = base.copy()
    for key, value in override.items():
        if key in result and isinstance(result[key], dict) and isinstance(value, dict):
            result[key] = deep_merge(result[key], value)
        else:
            result[key] = value
    return result
