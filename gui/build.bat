@echo off
setlocal

set SCRIPT_DIR=%~dp0
set OUT_DIR=%SCRIPT_DIR%build

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

where cl >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Run from Visual Studio Developer Command Prompt
    exit /b 1
)

echo Building vcbuild_gui...

rc /nologo /i "%SCRIPT_DIR%src" /fo "%OUT_DIR%\app.res" "%SCRIPT_DIR%resources\app.rc"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

cl /nologo /std:c++20 /EHsc /O2 /W4 ^
    /D_UNICODE /DUNICODE ^
    /I"%SCRIPT_DIR%src" ^
    /I"C:\files\software\vcpkg_repo\vcpkg\packages\wtl_x64-windows\include" ^
    /Fe"%OUT_DIR%\vcbuild_gui.exe" ^
    /Fo"%OUT_DIR%\\" ^
    "%SCRIPT_DIR%src\Main.cpp" ^
    "%SCRIPT_DIR%src\MainWindow.cpp" ^
    "%OUT_DIR%\app.res" ^
    /link /SUBSYSTEM:WINDOWS ^
    user32.lib gdi32.lib comctl32.lib shell32.lib ole32.lib

if %ERRORLEVEL% equ 0 (
    echo Built: %OUT_DIR%\vcbuild_gui.exe
) else (
    echo Build failed
    exit /b %ERRORLEVEL%
)

del "%OUT_DIR%\*.obj" 2>nul
