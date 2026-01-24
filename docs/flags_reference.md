# MSVC Compiler Flags Reference

Quick reference for common MSVC compiler and linker flags used by vcbuild.

## Compiler Flags

### C++ Standard
| Flag | Description |
|------|-------------|
| `/std:c++17` | C++17 standard |
| `/std:c++20` | C++20 standard |
| `/std:c++23` | C++23 standard |
| `/std:c++latest` | Latest available |

### Runtime Library
| Flag | Description |
|------|-------------|
| `/MD` | Dynamic CRT (release) |
| `/MDd` | Dynamic CRT (debug) |
| `/MT` | Static CRT (release) |
| `/MTd` | Static CRT (debug) |

### Optimization
| Flag | Description |
|------|-------------|
| `/Od` | Disabled |
| `/O1` | Minimize size |
| `/O2` | Maximize speed |
| `/GL` | Whole program optimization |

### Warnings
| Flag | Description |
|------|-------------|
| `/W0`-`/W4` | Warning level 0-4 |
| `/WX` | Warnings as errors |
| `/wd####` | Disable warning |

### Debug
| Flag | Description |
|------|-------------|
| `/Zi` | Debug information |
| `/RTC1` | Runtime checks |

### Security
| Flag | Description |
|------|-------------|
| `/GS` | Buffer security check |
| `/guard:cf` | Control flow guard |

### Conformance
| Flag | Description |
|------|-------------|
| `/permissive-` | Strict conformance |
| `/Zc:__cplusplus` | Correct macro value |

### Other
| Flag | Description |
|------|-------------|
| `/EHsc` | C++ exceptions |
| `/MP` | Multi-process compilation |
| `/utf-8` | UTF-8 source charset |

## Linker Flags

### Security
| Flag | Description |
|------|-------------|
| `/DYNAMICBASE` | ASLR |
| `/NXCOMPAT` | DEP |
| `/guard:cf` | CFG |

### Optimization
| Flag | Description |
|------|-------------|
| `/LTCG` | Link-time code generation |
| `/OPT:REF` | Remove unreferenced |
| `/OPT:ICF` | COMDAT folding |

### Subsystem
| Flag | Description |
|------|-------------|
| `/SUBSYSTEM:CONSOLE` | Console application |
| `/SUBSYSTEM:WINDOWS` | GUI application |
| `/SUBSYSTEM:NATIVE` | Driver/kernel mode |
