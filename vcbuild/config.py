"""Configuration loading, merging, and resolution."""

import json
from pathlib import Path
from typing import Any

from . import schema
from . import output

CONFIG_FILENAME = "vcbuild.json"

class Config:
    def __init__(self, data: dict[str, Any], project_root: Path):
        self.data = data
        self.project_root = project_root
    
    def get(self, *keys: str, default: Any = None) -> Any:
        val = self.data
        for k in keys:
            if isinstance(val, dict) and k in val:
                val = val[k]
            else:
                return default
        return val

def find_project_root(start: Path) -> Path | None:
    current = start.resolve()
    while current != current.parent:
        if (current / CONFIG_FILENAME).exists():
            return current
        if (current / "src").is_dir():
            return current
        current = current.parent
    return None

def load(project_root: Path, profile: str = "release") -> Config:
    config = schema.DEFAULTS.copy()
    config_file = project_root / CONFIG_FILENAME
    
    if config_file.exists():
        try:
            with open(config_file, "r", encoding="utf-8") as f:
                user_config = json.load(f)
            config = schema.deep_merge(config, user_config)
        except json.JSONDecodeError as e:
            output.error(f"Invalid JSON in {CONFIG_FILENAME}", str(e))
            raise SystemExit(1)
    
    if profile in config.get("profiles", {}):
        profile_overrides = config["profiles"][profile]
        config = schema.deep_merge(config, profile_overrides)
    
    errors = schema.validate(config)
    if errors:
        output.error(f"Configuration errors in {CONFIG_FILENAME}")
        for err in errors:
            output.detail(f"  {err}")
        raise SystemExit(1)
    
    if config["project"]["name"] is None:
        config["project"]["name"] = project_root.name
    
    if config["project"]["output_name"] is None:
        ext = {"exe": ".exe", "dll": ".dll", "lib": ".lib", "sys": ".sys"}
        config["project"]["output_name"] = (
            config["project"]["name"] + ext.get(config["project"]["type"], ".exe")
        )
    
    return Config(config, project_root)

def generate_template(dest: Path):
    template = {
        "project": {
            "name": dest.parent.name,
            "type": "exe",
            "architecture": "x64"
        },
        "compiler": {
            "standard": "c++20",
            "defines": []
        },
        "linker": {
            "libraries": [],
            "subsystem": "console"
        },
        "sources": {
            "include_dirs": ["src", "include"],
            "source_dirs": ["src"]
        }
    }
    
    with open(dest, "w", encoding="utf-8") as f:
        json.dump(template, f, indent=2)
    
    output.success(f"Created {dest}")
