#pragma once

#define IDR_MAINFRAME           100

#define IDD_MAIN                101
#define IDD_PROJECT             102
#define IDD_COMPILER            103
#define IDD_LINKER              104
#define IDD_SOURCES             105
#define IDD_RESOURCES           106
#define IDD_DRIVER              107

#define IDC_TAB                 1001
#define IDC_SAVE                1002
#define IDC_STATUSBAR           1003

#define IDC_PROJECT_NAME        2001
#define IDC_PROJECT_TYPE        2002
#define IDC_PROJECT_ARCH        2003
#define IDC_PROJECT_OUTDIR      2004

#define IDC_COMPILER_STD        3001
#define IDC_COMPILER_RUNTIME    3002
#define IDC_COMPILER_WARN       3003
#define IDC_COMPILER_DEFINES    3004
#define IDC_COMPILER_EXCEPTIONS 3005
#define IDC_COMPILER_PERMISSIVE 3006
#define IDC_COMPILER_PARALLEL   3007
#define IDC_COMPILER_BUFFER     3008
#define IDC_COMPILER_CFG        3009
#define IDC_COMPILER_RTTI       3010
#define IDC_COMPILER_FP         3011
#define IDC_COMPILER_CALLCONV   3012
#define IDC_COMPILER_CHARSET    3013
#define IDC_COMPILER_FUNCLINK   3014
#define IDC_COMPILER_STRPOOL    3015
#define IDC_COMPILER_WARNERR    3016

#define IDC_LINKER_LIBS         4001
#define IDC_LINKER_SUBSYSTEM    4002
#define IDC_LINKER_ASLR         4003
#define IDC_LINKER_DEP          4004
#define IDC_LINKER_LTO          4005
#define IDC_LINKER_CFG          4006
#define IDC_LINKER_LIBPATHS     4007
#define IDC_LINKER_ENTRY        4008
#define IDC_LINKER_DEFFILE      4009
#define IDC_LINKER_MAP          4010
#define IDC_LINKER_DEBUGINFO    4011

#define IDC_SOURCES_INCLUDE     5001
#define IDC_SOURCES_SOURCE      5002
#define IDC_SOURCES_EXCLUDE     5003
#define IDC_SOURCES_EXTERNAL    5004

#define IDC_RESOURCES_ENABLED   6001
#define IDC_RESOURCES_FILES     6002

#define IDC_DRIVER_ENABLED      7001
#define IDC_DRIVER_TYPE         7002
#define IDC_DRIVER_ENTRY        7003
#define IDC_DRIVER_TARGETOS     7004
#define IDC_DRIVER_MINIFILTER   7005

#ifndef IDC_STATIC
#define IDC_STATIC              -1
#endif
