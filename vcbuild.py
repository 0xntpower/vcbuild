#!/usr/bin/env python3
"""vcbuild - Lightweight build system for Windows C++ projects using MSVC."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from vcbuild.cli import main

if __name__ == "__main__":
    sys.exit(main())
