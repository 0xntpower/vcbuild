#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <atlbase.h>
#include <atlapp.h>
#include <shellapi.h>
#include <filesystem>

CAppModule _Module;

#include "MainWindow.hpp"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int nCmdShow) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return 1;

    AtlInitCommonControls(ICC_WIN95_CLASSES | ICC_TAB_CLASSES);
    _Module.Init(nullptr, hInstance);

    std::filesystem::path configPath;

    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc > 1) {
        configPath = argv[1];
    } else {
        configPath = std::filesystem::current_path() / L"vcbuild.json";
    }
    LocalFree(argv);

    {
        vcbuild::gui::MainWindow wnd;
        if (!wnd.Initialize(configPath)) {
            _Module.Term();
            CoUninitialize();
            return 1;
        }

        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    _Module.Term();
    CoUninitialize();
    return 0;
}
