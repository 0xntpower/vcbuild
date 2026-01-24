"""Source and header file discovery."""

import fnmatch
from pathlib import Path
from dataclasses import dataclass

from .config import Config

@dataclass
class SourceSet:
    sources: list[Path]
    include_dirs: list[Path]

def discover(config: Config) -> SourceSet:
    root = config.project_root
    sources_cfg = config.data.get("sources", {})
    
    extensions = set(sources_cfg.get("extensions", [".cpp", ".cc", ".c", ".cxx"]))
    exclude_patterns = sources_cfg.get("exclude_patterns", [])
    
    source_dirs = [
        root / d for d in sources_cfg.get("source_dirs", ["src"])
    ]
    source_dirs.extend(
        Path(d) if Path(d).is_absolute() else root / d
        for d in sources_cfg.get("external_dirs", [])
    )
    
    include_dirs = [
        root / d for d in sources_cfg.get("include_dirs", ["src", "include"])
    ]
    include_dirs.extend(
        Path(d) if Path(d).is_absolute() else root / d
        for d in sources_cfg.get("external_dirs", [])
    )
    
    explicit = sources_cfg.get("explicit_sources", [])
    if explicit:
        sources = [
            Path(s) if Path(s).is_absolute() else root / s
            for s in explicit
        ]
        return SourceSet(sources=sources, include_dirs=include_dirs)
    
    sources = []
    for src_dir in source_dirs:
        if not src_dir.exists():
            continue
        for ext in extensions:
            for path in src_dir.rglob(f"*{ext}"):
                if not _is_excluded(path, exclude_patterns):
                    sources.append(path)
    
    include_dirs = [d for d in include_dirs if d.exists()]
    
    return SourceSet(sources=sorted(sources), include_dirs=include_dirs)

def _is_excluded(path: Path, patterns: list[str]) -> bool:
    name = path.name
    rel_path = str(path)
    
    for pattern in patterns:
        if fnmatch.fnmatch(name, pattern):
            return True
        if fnmatch.fnmatch(rel_path, pattern):
            return True
    
    return False
