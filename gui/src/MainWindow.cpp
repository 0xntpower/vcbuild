#include "MainWindow.hpp"

// Helper to convert CRect to _U_RECT for WTL control Create methods
inline ATL::_U_RECT RC(int l, int t, int r, int b) {
    static RECT rc;
    rc.left = l;
    rc.top = t;
    rc.right = r;
    rc.bottom = b;
    return ATL::_U_RECT(&rc);
}

// Helper to extract actual value from display text (e.g., "exe" from "Executable (exe)")
inline std::wstring ExtractValue(const std::wstring& text) {
    size_t openParen = text.find(L'(');
    if (openParen != std::wstring::npos) {
        size_t closeParen = text.find(L')', openParen);
        if (closeParen != std::wstring::npos) {
            return text.substr(openParen + 1, closeParen - openParen - 1);
        }
    }
    size_t space = text.find(L' ');
    if (space != std::wstring::npos) {
        std::wstring val = text.substr(0, space);
        for (auto& c : val) c = (wchar_t)std::tolower((wint_t)c);
        return val;
    }
    std::wstring val = text;
    for (auto& c : val) c = (wchar_t)std::tolower((wint_t)c);
    return val;
}

namespace vcbuild::gui {

bool MainWindow::Initialize(const std::filesystem::path& configPath) {
    configPath_ = configPath;

    if (!config_.Load(configPath_)) {
        MessageBox(L"Failed to load config", L"Error", MB_ICONERROR);
        return false;
    }

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    RECT rc = {0, 0, 640, 480};
    AdjustWindowRect(&rc, style, FALSE);

    Create(nullptr, rc, L"vcbuild Config Generator", style, WS_EX_APPWINDOW);

    if (!m_hWnd) return false;

    CenterWindow();
    ShowWindow(SW_SHOW);
    UpdateWindow();

    return true;
}

int MainWindow::OnCreate(LPCREATESTRUCT) {
    // Create fonts once, reuse everywhere
    hFont_ = CreateFont(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    hSmallFont_ = CreateFont(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    CreateControls();
    LoadConfigToUI();
    ShowPage(0);
    return 0;
}

void MainWindow::CreateControls() {
    CRect rc;
    GetClientRect(&rc);
    int w = rc.Width();
    int h = rc.Height();

    // Tab control fills most of the window
    tabCtrl_.Create(m_hWnd, RC(kMargin, kMargin, w - kMargin, h - 44),
                    nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | TCS_TABS,
                    0, IDC_TAB);
    tabCtrl_.SetFont(hFont_);

    tabCtrl_.InsertItem(0, L" Project ");
    tabCtrl_.InsertItem(1, L" Compiler ");
    tabCtrl_.InsertItem(2, L" Linker ");
    tabCtrl_.InsertItem(3, L" Sources ");
    tabCtrl_.InsertItem(4, L" Resources ");
    tabCtrl_.InsertItem(5, L" Driver ");

    // Save button - standard Windows size
    int btnWidth = 80;
    int btnHeight = 24;
    int btnY = h - 34;

    saveBtn_.Create(m_hWnd, RC(w - btnWidth - kMargin, btnY,
                               w - kMargin, btnY + btnHeight),
                    L"Save", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON | BS_DEFPUSHBUTTON,
                    0, IDC_SAVE);
    saveBtn_.SetFont(hFont_);

    CreateProjectPage();
    CreateCompilerPage();
    CreateLinkerPage();
    CreateSourcesPage();
    CreateResourcesPage();
    CreateDriverPage();
}

void MainWindow::CreateProjectPage() {
    int groupX = kMargin + 6;
    int groupW = 600;
    int x = groupX + 12;
    int y = 50;
    int ctrlX = x + kLabelWidth + 8;
    int ctrlW = groupW - kLabelWidth - 32;
    int rowSpacing = 28;

    // Group box
    HWND groupBox = CreateWindow(L"BUTTON", L"Project Configuration",
                                  WS_CHILD | BS_GROUPBOX,
                                  groupX, 35, groupW, 138,
                                  m_hWnd, nullptr, nullptr, nullptr);
    SendMessage(groupBox, WM_SETFONT, (WPARAM)hFont_, TRUE);
    projectCtrls_.push_back(groupBox);

    auto label = [&](const wchar_t* text) {
        HWND h = CreateWindow(L"STATIC", text, WS_CHILD | SS_LEFT,
                              x, y + 3, kLabelWidth, kCtrlHeight,
                              m_hWnd, nullptr, nullptr, nullptr);
        SendMessage(h, WM_SETFONT, (WPARAM)hFont_, TRUE);
        projectCtrls_.push_back(h);
    };

    label(L"Project Name:");
    projectName_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 21),
                        nullptr, WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL);
    projectName_.SetFont(hFont_);
    projectCtrls_.push_back(projectName_);
    y += rowSpacing;

    label(L"Output Type:");
    projectType_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 150),
                        nullptr, WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST);
    projectType_.SetFont(hFont_);
    projectType_.AddString(L"Executable (exe)");
    projectType_.AddString(L"Dynamic Library (dll)");
    projectType_.AddString(L"Static Library (lib)");
    projectType_.AddString(L"Kernel Driver (sys)");
    projectCtrls_.push_back(projectType_);
    y += rowSpacing;

    label(L"Architecture:");
    projectArch_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 150),
                        nullptr, WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST);
    projectArch_.SetFont(hFont_);
    projectArch_.AddString(L"x64 (64-bit)");
    projectArch_.AddString(L"x86 (32-bit)");
    projectArch_.AddString(L"ARM64");
    projectCtrls_.push_back(projectArch_);
    y += rowSpacing;

    label(L"Output Directory:");
    projectOutDir_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 21),
                          nullptr, WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL);
    projectOutDir_.SetFont(hFont_);
    projectCtrls_.push_back(projectOutDir_);

    // Help text
    y = 180;
    HWND desc = CreateWindow(L"STATIC",
                             L"Configure basic project settings. Select 'Kernel Driver' type to enable the Driver tab.",
                             WS_CHILD | SS_LEFT,
                             x, y, groupW - 20, 32,
                             m_hWnd, nullptr, nullptr, nullptr);
    SendMessage(desc, WM_SETFONT, (WPARAM)hSmallFont_, TRUE);
    projectCtrls_.push_back(desc);
}

void MainWindow::CreateCompilerPage() {
    int groupX = kMargin + 6;
    int groupW = 600;
    int x = groupX + 12;
    int y = 50;
    int ctrlX = x + kLabelWidth + 8;
    int ctrlW = groupW - kLabelWidth - 32;
    int rowSpacing = 27;

    // Group: Language & Runtime
    HWND groupBox1 = CreateWindow(L"BUTTON", L"Language && Runtime",
                                   WS_CHILD | BS_GROUPBOX,
                                   groupX, 35, groupW, 165,
                                   m_hWnd, nullptr, nullptr, nullptr);
    SendMessage(groupBox1, WM_SETFONT, (WPARAM)hFont_, TRUE);
    compilerCtrls_.push_back(groupBox1);

    auto label = [&](const wchar_t* text) {
        HWND h = CreateWindow(L"STATIC", text, WS_CHILD | SS_LEFT,
                              x, y + 3, kLabelWidth, kCtrlHeight,
                              m_hWnd, nullptr, nullptr, nullptr);
        SendMessage(h, WM_SETFONT, (WPARAM)hFont_, TRUE);
        compilerCtrls_.push_back(h);
    };

    label(L"C/C++ Standard:");
    compilerStd_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 150),
                        nullptr, WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST);
    compilerStd_.SetFont(hFont_);
    compilerStd_.AddString(L"C11");
    compilerStd_.AddString(L"C17");
    compilerStd_.AddString(L"C++17");
    compilerStd_.AddString(L"C++20 (Default)");
    compilerStd_.AddString(L"C++23");
    compilerStd_.AddString(L"C++ Latest");
    compilerCtrls_.push_back(compilerStd_);
    y += rowSpacing;

    label(L"Runtime Library:");
    compilerRuntime_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 150),
                            nullptr, WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST);
    compilerRuntime_.SetFont(hFont_);
    compilerRuntime_.AddString(L"Dynamic (DLL)");
    compilerRuntime_.AddString(L"Static");
    compilerCtrls_.push_back(compilerRuntime_);
    y += rowSpacing;

    label(L"Floating Point:");
    compilerFp_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 150),
                       nullptr, WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST);
    compilerFp_.SetFont(hFont_);
    compilerFp_.AddString(L"Precise (Default)");
    compilerFp_.AddString(L"Fast");
    compilerFp_.AddString(L"Strict");
    compilerCtrls_.push_back(compilerFp_);
    y += rowSpacing;

    label(L"Calling Conv.:");
    compilerCallConv_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 150),
                             nullptr, WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST);
    compilerCallConv_.SetFont(hFont_);
    compilerCallConv_.AddString(L"cdecl (Default)");
    compilerCallConv_.AddString(L"stdcall");
    compilerCallConv_.AddString(L"fastcall");
    compilerCallConv_.AddString(L"vectorcall");
    compilerCtrls_.push_back(compilerCallConv_);
    y += rowSpacing;

    label(L"Character Set:");
    compilerCharSet_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 150),
                            nullptr, WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST);
    compilerCharSet_.SetFont(hFont_);
    compilerCharSet_.AddString(L"Unicode (Default)");
    compilerCharSet_.AddString(L"Multi-Byte (MBCS)");
    compilerCharSet_.AddString(L"Not Set");
    compilerCtrls_.push_back(compilerCharSet_);
    y += rowSpacing;

    label(L"Preprocessor:");
    compilerDefines_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 21),
                            nullptr, WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL);
    compilerDefines_.SetFont(hFont_);
    compilerCtrls_.push_back(compilerDefines_);

    // Group: Compiler Options
    int group2Y = 205;
    y = group2Y + 18;
    HWND groupBox2 = CreateWindow(L"BUTTON", L"Compiler Options",
                                   WS_CHILD | BS_GROUPBOX,
                                   groupX, group2Y, groupW, 88,
                                   m_hWnd, nullptr, nullptr, nullptr);
    SendMessage(groupBox2, WM_SETFONT, (WPARAM)hFont_, TRUE);
    compilerCtrls_.push_back(groupBox2);

    int checkCol1 = x;
    int checkCol2 = x + 155;
    int checkCol3 = x + 310;
    int checkCol4 = x + 455;
    int checkW = 150;

    compilerExceptions_.Create(m_hWnd, RC(checkCol1, y, checkCol1 + checkW, y + 18),
                               L"C++ Exceptions", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX);
    compilerExceptions_.SetFont(hFont_);
    compilerCtrls_.push_back(compilerExceptions_);

    compilerRtti_.Create(m_hWnd, RC(checkCol2, y, checkCol2 + checkW, y + 18),
                         L"RTTI (/GR)", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX);
    compilerRtti_.SetFont(hFont_);
    compilerCtrls_.push_back(compilerRtti_);

    compilerParallel_.Create(m_hWnd, RC(checkCol3, y, checkCol3 + checkW, y + 18),
                             L"Parallel Build (/MP)", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX);
    compilerParallel_.SetFont(hFont_);
    compilerCtrls_.push_back(compilerParallel_);

    compilerWarnErr_.Create(m_hWnd, RC(checkCol4, y, checkCol4 + checkW, y + 18),
                            L"Warnings as Errors", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX);
    compilerWarnErr_.SetFont(hFont_);
    compilerCtrls_.push_back(compilerWarnErr_);

    y += 22;

    compilerBuffer_.Create(m_hWnd, RC(checkCol1, y, checkCol1 + checkW, y + 18),
                           L"Buffer Checks (/GS)", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX);
    compilerBuffer_.SetFont(hFont_);
    compilerCtrls_.push_back(compilerBuffer_);

    compilerCfg_.Create(m_hWnd, RC(checkCol2, y, checkCol2 + checkW, y + 18),
                        L"Control Flow Guard", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX);
    compilerCfg_.SetFont(hFont_);
    compilerCtrls_.push_back(compilerCfg_);

    compilerFuncLink_.Create(m_hWnd, RC(checkCol3, y, checkCol3 + checkW, y + 18),
                             L"Function Linking (/Gy)", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX);
    compilerFuncLink_.SetFont(hFont_);
    compilerCtrls_.push_back(compilerFuncLink_);

    compilerStrPool_.Create(m_hWnd, RC(checkCol4, y, checkCol4 + checkW, y + 18),
                            L"String Pooling (/GF)", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX);
    compilerStrPool_.SetFont(hFont_);
    compilerCtrls_.push_back(compilerStrPool_);
}

void MainWindow::CreateLinkerPage() {
    int groupX = kMargin + 6;
    int groupW = 600;
    int x = groupX + 12;
    int y = 50;
    int ctrlX = x + kLabelWidth + 8;
    int ctrlW = groupW - kLabelWidth - 32;
    int rowSpacing = 27;

    // Group: Output & Libraries
    HWND groupBox1 = CreateWindow(L"BUTTON", L"Output && Libraries",
                                   WS_CHILD | BS_GROUPBOX,
                                   groupX, 35, groupW, 165,
                                   m_hWnd, nullptr, nullptr, nullptr);
    SendMessage(groupBox1, WM_SETFONT, (WPARAM)hFont_, TRUE);
    linkerCtrls_.push_back(groupBox1);

    auto label = [&](const wchar_t* text) {
        HWND h = CreateWindow(L"STATIC", text, WS_CHILD | SS_LEFT,
                              x, y + 3, kLabelWidth, kCtrlHeight,
                              m_hWnd, nullptr, nullptr, nullptr);
        SendMessage(h, WM_SETFONT, (WPARAM)hFont_, TRUE);
        linkerCtrls_.push_back(h);
    };

    label(L"Link Libraries:");
    linkerLibs_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 21),
                       nullptr, WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL);
    linkerLibs_.SetFont(hFont_);
    linkerCtrls_.push_back(linkerLibs_);
    y += rowSpacing;

    label(L"Library Paths:");
    linkerLibPaths_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 21),
                           nullptr, WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL);
    linkerLibPaths_.SetFont(hFont_);
    linkerCtrls_.push_back(linkerLibPaths_);
    y += rowSpacing;

    label(L"Subsystem:");
    linkerSubsystem_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 200),
                            nullptr, WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST);
    linkerSubsystem_.SetFont(hFont_);
    linkerSubsystem_.AddString(L"Console Application");
    linkerSubsystem_.AddString(L"Windows GUI");
    linkerSubsystem_.AddString(L"Native (Driver)");
    linkerSubsystem_.AddString(L"EFI Application");
    linkerSubsystem_.AddString(L"Boot Application");
    linkerSubsystem_.AddString(L"POSIX");
    linkerCtrls_.push_back(linkerSubsystem_);
    y += rowSpacing;

    label(L"Entry Point:");
    linkerEntry_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 21),
                        nullptr, WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL);
    linkerEntry_.SetFont(hFont_);
    linkerCtrls_.push_back(linkerEntry_);
    y += rowSpacing;

    label(L"DEF File:");
    linkerDefFile_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 21),
                          nullptr, WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL);
    linkerDefFile_.SetFont(hFont_);
    linkerCtrls_.push_back(linkerDefFile_);

    // Group: Security & Optimization
    int group2Y = 205;
    y = group2Y + 18;
    HWND groupBox2 = CreateWindow(L"BUTTON", L"Security && Optimization",
                                   WS_CHILD | BS_GROUPBOX,
                                   groupX, group2Y, groupW, 88,
                                   m_hWnd, nullptr, nullptr, nullptr);
    SendMessage(groupBox2, WM_SETFONT, (WPARAM)hFont_, TRUE);
    linkerCtrls_.push_back(groupBox2);

    int checkCol1 = x;
    int checkCol2 = x + 155;
    int checkCol3 = x + 310;
    int checkCol4 = x + 455;
    int checkW = 150;

    linkerAslr_.Create(m_hWnd, RC(checkCol1, y, checkCol1 + checkW, y + 18),
                       L"ASLR (/DYNAMICBASE)", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX);
    linkerAslr_.SetFont(hFont_);
    linkerCtrls_.push_back(linkerAslr_);

    linkerDep_.Create(m_hWnd, RC(checkCol2, y, checkCol2 + checkW, y + 18),
                      L"DEP (/NXCOMPAT)", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX);
    linkerDep_.SetFont(hFont_);
    linkerCtrls_.push_back(linkerDep_);

    linkerLto_.Create(m_hWnd, RC(checkCol3, y, checkCol3 + checkW, y + 18),
                      L"Link-Time Codegen", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX);
    linkerLto_.SetFont(hFont_);
    linkerCtrls_.push_back(linkerLto_);

    linkerCfgLink_.Create(m_hWnd, RC(checkCol4, y, checkCol4 + checkW, y + 18),
                          L"Control Flow Guard", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX);
    linkerCfgLink_.SetFont(hFont_);
    linkerCtrls_.push_back(linkerCfgLink_);

    y += 22;

    linkerMap_.Create(m_hWnd, RC(checkCol1, y, checkCol1 + checkW, y + 18),
                      L"Generate Map File", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX);
    linkerMap_.SetFont(hFont_);
    linkerCtrls_.push_back(linkerMap_);

    linkerDebugInfo_.Create(m_hWnd, RC(checkCol2, y, checkCol2 + checkW, y + 18),
                            L"Debug Information", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX);
    linkerDebugInfo_.SetFont(hFont_);
    linkerCtrls_.push_back(linkerDebugInfo_);
}

void MainWindow::CreateSourcesPage() {
    int groupX = kMargin + 6;
    int groupW = 600;
    int x = groupX + 12;
    int y = 50;
    int ctrlX = x + kLabelWidth + 8;
    int ctrlW = groupW - kLabelWidth - 32;
    int rowSpacing = 28;

    HWND groupBox = CreateWindow(L"BUTTON", L"Source && Include Directories",
                                  WS_CHILD | BS_GROUPBOX,
                                  groupX, 35, groupW, 138,
                                  m_hWnd, nullptr, nullptr, nullptr);
    SendMessage(groupBox, WM_SETFONT, (WPARAM)hFont_, TRUE);
    sourcesCtrls_.push_back(groupBox);

    auto label = [&](const wchar_t* text) {
        HWND h = CreateWindow(L"STATIC", text, WS_CHILD | SS_LEFT,
                              x, y + 3, kLabelWidth, kCtrlHeight,
                              m_hWnd, nullptr, nullptr, nullptr);
        SendMessage(h, WM_SETFONT, (WPARAM)hFont_, TRUE);
        sourcesCtrls_.push_back(h);
    };

    label(L"Include Directories:");
    sourcesInclude_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 21),
                           nullptr, WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL);
    sourcesInclude_.SetFont(hFont_);
    sourcesCtrls_.push_back(sourcesInclude_);
    y += rowSpacing;

    label(L"Source Directories:");
    sourcesSource_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 21),
                          nullptr, WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL);
    sourcesSource_.SetFont(hFont_);
    sourcesCtrls_.push_back(sourcesSource_);
    y += rowSpacing;

    label(L"Exclude Patterns:");
    sourcesExclude_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 21),
                           nullptr, WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL);
    sourcesExclude_.SetFont(hFont_);
    sourcesCtrls_.push_back(sourcesExclude_);
    y += rowSpacing;

    label(L"External Directories:");
    sourcesExternal_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 21),
                            nullptr, WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL);
    sourcesExternal_.SetFont(hFont_);
    sourcesCtrls_.push_back(sourcesExternal_);

    y = 180;
    HWND desc = CreateWindow(L"STATIC",
                             L"Separate multiple paths with commas. Glob patterns supported for exclude.",
                             WS_CHILD | SS_LEFT,
                             x, y, groupW - 20, 32,
                             m_hWnd, nullptr, nullptr, nullptr);
    SendMessage(desc, WM_SETFONT, (WPARAM)hSmallFont_, TRUE);
    sourcesCtrls_.push_back(desc);
}

void MainWindow::CreateResourcesPage() {
    int groupX = kMargin + 6;
    int groupW = 600;
    int x = groupX + 12;
    int y = 50;
    int ctrlX = x + kLabelWidth + 8;
    int ctrlW = groupW - kLabelWidth - 32;
    int rowSpacing = 28;

    HWND groupBox = CreateWindow(L"BUTTON", L"Resource Compilation",
                                  WS_CHILD | BS_GROUPBOX,
                                  groupX, 35, groupW, 107,
                                  m_hWnd, nullptr, nullptr, nullptr);
    SendMessage(groupBox, WM_SETFONT, (WPARAM)hFont_, TRUE);
    resourcesCtrls_.push_back(groupBox);

    resourcesEnabled_.Create(m_hWnd, RC(x, y, x + 220, y + 18),
                             L"Enable Resource Compilation", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX);
    resourcesEnabled_.SetFont(hFont_);
    resourcesCtrls_.push_back(resourcesEnabled_);
    y += rowSpacing;

    auto label = [&](const wchar_t* text) {
        HWND h = CreateWindow(L"STATIC", text, WS_CHILD | SS_LEFT,
                              x, y + 3, kLabelWidth, kCtrlHeight,
                              m_hWnd, nullptr, nullptr, nullptr);
        SendMessage(h, WM_SETFONT, (WPARAM)hFont_, TRUE);
        resourcesCtrls_.push_back(h);
    };

    label(L"Resource Files:");
    resourcesFiles_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 21),
                           nullptr, WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL);
    resourcesFiles_.SetFont(hFont_);
    resourcesCtrls_.push_back(resourcesFiles_);

    y = 150;
    HWND desc = CreateWindow(L"STATIC",
                             L"Compile Windows resource files (.rc) for icons, manifests, version info, dialogs, etc.\nSeparate multiple files with commas.",
                             WS_CHILD | SS_LEFT,
                             x, y, groupW - 20, 40,
                             m_hWnd, nullptr, nullptr, nullptr);
    SendMessage(desc, WM_SETFONT, (WPARAM)hSmallFont_, TRUE);
    resourcesCtrls_.push_back(desc);
}

void MainWindow::CreateDriverPage() {
    int groupX = kMargin + 6;
    int groupW = 600;
    int x = groupX + 12;
    int y = 50;
    int ctrlX = x + kLabelWidth + 8;
    int ctrlW = groupW - kLabelWidth - 32;
    int rowSpacing = 28;

    HWND groupBox = CreateWindow(L"BUTTON", L"Kernel Driver Configuration",
                                  WS_CHILD | BS_GROUPBOX,
                                  groupX, 35, groupW, 167,
                                  m_hWnd, nullptr, nullptr, nullptr);
    SendMessage(groupBox, WM_SETFONT, (WPARAM)hFont_, TRUE);
    driverCtrls_.push_back(groupBox);

    driverEnabled_.Create(m_hWnd, RC(x, y, x + 200, y + 18),
                          L"Enable Driver Build", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX);
    driverEnabled_.SetFont(hFont_);
    driverCtrls_.push_back(driverEnabled_);
    y += rowSpacing;

    auto label = [&](const wchar_t* text) {
        HWND h = CreateWindow(L"STATIC", text, WS_CHILD | SS_LEFT,
                              x, y + 3, kLabelWidth, kCtrlHeight,
                              m_hWnd, nullptr, nullptr, nullptr);
        SendMessage(h, WM_SETFONT, (WPARAM)hFont_, TRUE);
        driverCtrls_.push_back(h);
    };

    label(L"Driver Type:");
    driverType_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 150),
                       nullptr, WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST);
    driverType_.SetFont(hFont_);
    driverType_.AddString(L"WDM");
    driverType_.AddString(L"KMDF");
    driverType_.AddString(L"WDF");
    driverCtrls_.push_back(driverType_);
    y += rowSpacing;

    label(L"Entry Point:");
    driverEntry_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 21),
                        nullptr, WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL);
    driverEntry_.SetFont(hFont_);
    driverCtrls_.push_back(driverEntry_);
    y += rowSpacing;

    label(L"Target OS:");
    driverTargetOs_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 150),
                           nullptr, WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST);
    driverTargetOs_.SetFont(hFont_);
    driverTargetOs_.AddString(L"Windows 7");
    driverTargetOs_.AddString(L"Windows 8");
    driverTargetOs_.AddString(L"Windows 8.1");
    driverTargetOs_.AddString(L"Windows 10 (Default)");
    driverTargetOs_.AddString(L"Windows 11");
    driverCtrls_.push_back(driverTargetOs_);
    y += rowSpacing;

    driverMinifilter_.Create(m_hWnd, RC(x, y, x + 220, y + 18),
                             L"Minifilter Driver (fltMgr.lib)", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX);
    driverMinifilter_.SetFont(hFont_);
    driverCtrls_.push_back(driverMinifilter_);

    y = 210;
    HWND desc = CreateWindow(L"STATIC",
                             L"Requires Windows Driver Kit (WDK). Set project type to 'Kernel Driver' on the Project tab. "
                             L"Automatically configures /kernel, /DRIVER, WDK includes and kernel libraries.",
                             WS_CHILD | SS_LEFT,
                             x, y, groupW - 20, 48,
                             m_hWnd, nullptr, nullptr, nullptr);
    SendMessage(desc, WM_SETFONT, (WPARAM)hSmallFont_, TRUE);
    driverCtrls_.push_back(desc);
}

void MainWindow::ShowPage(int index) {
    auto show = [](std::vector<HWND>& ctrls, bool visible) {
        int sw = visible ? SW_SHOW : SW_HIDE;
        for (HWND h : ctrls) ::ShowWindow(h, sw);
    };

    show(projectCtrls_, index == 0);
    show(compilerCtrls_, index == 1);
    show(linkerCtrls_, index == 2);
    show(sourcesCtrls_, index == 3);
    show(resourcesCtrls_, index == 4);
    show(driverCtrls_, index == 5);
    currentPage_ = index;
}

void MainWindow::LoadConfigToUI() {
    auto& proj = config_.Project();
    auto& comp = config_.Compiler();
    auto& link = config_.Linker();
    auto& srcs = config_.Sources();
    auto& res = config_.Resources();
    auto& drv = config_.Driver();

    projectName_.SetWindowText(proj.name.c_str());

    // Select combo items by searching display text for config value
    auto selectComboPartial = [](CComboBox& cb, const std::wstring& val) {
        // Try exact match first
        for (int i = 0; i < cb.GetCount(); ++i) {
            wchar_t buf[256];
            cb.GetLBText(i, buf);
            std::wstring text = buf;
            std::wstring extracted = ExtractValue(text);
            if (extracted == val) {
                cb.SetCurSel(i);
                return;
            }
        }
        // Try case-insensitive partial match
        std::wstring lval = val;
        for (auto& c : lval) c = (wchar_t)std::tolower((wint_t)c);
        for (int i = 0; i < cb.GetCount(); ++i) {
            wchar_t buf[256];
            cb.GetLBText(i, buf);
            std::wstring text = buf;
            for (auto& c : text) c = (wchar_t)std::tolower((wint_t)c);
            if (text.find(lval) != std::wstring::npos) {
                cb.SetCurSel(i);
                return;
            }
        }
        cb.SetCurSel(0);
    };

    selectComboPartial(projectType_, proj.type);
    selectComboPartial(projectArch_, proj.architecture);
    projectOutDir_.SetWindowText(proj.outputDir.c_str());

    selectComboPartial(compilerStd_, comp.standard);
    selectComboPartial(compilerRuntime_, comp.runtime);
    selectComboPartial(compilerFp_, comp.floatingPoint);
    selectComboPartial(compilerCallConv_, comp.callingConvention);
    selectComboPartial(compilerCharSet_, comp.charSet);

    compilerExceptions_.SetCheck(comp.exceptions ? BST_CHECKED : BST_UNCHECKED);
    compilerRtti_.SetCheck(comp.rtti ? BST_CHECKED : BST_UNCHECKED);
    compilerParallel_.SetCheck(comp.parallel ? BST_CHECKED : BST_UNCHECKED);
    compilerWarnErr_.SetCheck(comp.warningsAsErrors ? BST_CHECKED : BST_UNCHECKED);
    compilerBuffer_.SetCheck(comp.bufferChecks ? BST_CHECKED : BST_UNCHECKED);
    compilerCfg_.SetCheck(comp.cfGuard ? BST_CHECKED : BST_UNCHECKED);
    compilerFuncLink_.SetCheck(comp.functionLevelLinking ? BST_CHECKED : BST_UNCHECKED);
    compilerStrPool_.SetCheck(comp.stringPooling ? BST_CHECKED : BST_UNCHECKED);

    // Defines
    auto joinVec = [](const std::vector<std::wstring>& v) -> std::wstring {
        std::wstring out;
        for (size_t i = 0; i < v.size(); ++i) {
            if (i > 0) out += L", ";
            out += v[i];
        }
        return out;
    };

    compilerDefines_.SetWindowText(joinVec(comp.defines).c_str());

    // Linker
    linkerLibs_.SetWindowText(joinVec(link.libraries).c_str());
    linkerLibPaths_.SetWindowText(joinVec(link.libraryPaths).c_str());
    selectComboPartial(linkerSubsystem_, link.subsystem);
    linkerEntry_.SetWindowText(link.entryPoint.c_str());
    linkerDefFile_.SetWindowText(link.defFile.c_str());

    linkerAslr_.SetCheck(link.aslr ? BST_CHECKED : BST_UNCHECKED);
    linkerDep_.SetCheck(link.dep ? BST_CHECKED : BST_UNCHECKED);
    linkerLto_.SetCheck(link.lto ? BST_CHECKED : BST_UNCHECKED);
    linkerCfgLink_.SetCheck(link.cfgLinker ? BST_CHECKED : BST_UNCHECKED);
    linkerMap_.SetCheck(link.generateMap ? BST_CHECKED : BST_UNCHECKED);
    linkerDebugInfo_.SetCheck(link.generateDebugInfo ? BST_CHECKED : BST_UNCHECKED);

    // Sources
    sourcesInclude_.SetWindowText(joinVec(srcs.includeDirs).c_str());
    sourcesSource_.SetWindowText(joinVec(srcs.sourceDirs).c_str());
    sourcesExclude_.SetWindowText(joinVec(srcs.excludePatterns).c_str());
    sourcesExternal_.SetWindowText(joinVec(srcs.externalDirs).c_str());

    // Resources
    resourcesEnabled_.SetCheck(res.enabled ? BST_CHECKED : BST_UNCHECKED);
    resourcesFiles_.SetWindowText(joinVec(res.files).c_str());

    // Driver
    driverEnabled_.SetCheck(drv.enabled ? BST_CHECKED : BST_UNCHECKED);
    selectComboPartial(driverType_, drv.type);
    driverEntry_.SetWindowText(drv.entryPoint.c_str());
    selectComboPartial(driverTargetOs_, drv.targetOs);
    driverMinifilter_.SetCheck(drv.minifilter ? BST_CHECKED : BST_UNCHECKED);
}

void MainWindow::SaveUIToConfig() {
    auto& proj = config_.Project();
    auto& comp = config_.Compiler();
    auto& link = config_.Linker();
    auto& srcs = config_.Sources();
    auto& res = config_.Resources();
    auto& drv = config_.Driver();

    wchar_t buf[1024];

    projectName_.GetWindowText(buf, 1024);
    proj.name = buf;

    int idx = projectType_.GetCurSel();
    if (idx >= 0) {
        projectType_.GetLBText(idx, buf);
        proj.type = ExtractValue(buf);
    }

    idx = projectArch_.GetCurSel();
    if (idx >= 0) {
        projectArch_.GetLBText(idx, buf);
        proj.architecture = ExtractValue(buf);
    }

    projectOutDir_.GetWindowText(buf, 1024);
    proj.outputDir = buf;

    // Compiler
    idx = compilerStd_.GetCurSel();
    if (idx >= 0) {
        compilerStd_.GetLBText(idx, buf);
        comp.standard = ExtractValue(buf);
    }

    idx = compilerRuntime_.GetCurSel();
    if (idx >= 0) {
        compilerRuntime_.GetLBText(idx, buf);
        comp.runtime = ExtractValue(buf);
    }

    idx = compilerFp_.GetCurSel();
    if (idx >= 0) {
        compilerFp_.GetLBText(idx, buf);
        comp.floatingPoint = ExtractValue(buf);
    }

    idx = compilerCallConv_.GetCurSel();
    if (idx >= 0) {
        compilerCallConv_.GetLBText(idx, buf);
        comp.callingConvention = ExtractValue(buf);
    }

    idx = compilerCharSet_.GetCurSel();
    if (idx >= 0) {
        // Special handling for char set
        if (idx == 0) comp.charSet = L"unicode";
        else if (idx == 1) comp.charSet = L"mbcs";
        else comp.charSet = L"none";
    }

    comp.exceptions = compilerExceptions_.GetCheck() == BST_CHECKED;
    comp.rtti = compilerRtti_.GetCheck() == BST_CHECKED;
    comp.parallel = compilerParallel_.GetCheck() == BST_CHECKED;
    comp.warningsAsErrors = compilerWarnErr_.GetCheck() == BST_CHECKED;
    comp.bufferChecks = compilerBuffer_.GetCheck() == BST_CHECKED;
    comp.cfGuard = compilerCfg_.GetCheck() == BST_CHECKED;
    comp.functionLevelLinking = compilerFuncLink_.GetCheck() == BST_CHECKED;
    comp.stringPooling = compilerStrPool_.GetCheck() == BST_CHECKED;

    // Parse defines
    compilerDefines_.GetWindowText(buf, 1024);
    std::wstring defStr = buf;
    comp.defines.clear();
    size_t start = 0;
    while (start < defStr.length()) {
        size_t comma = defStr.find(L',', start);
        if (comma == std::wstring::npos) comma = defStr.length();
        std::wstring item = defStr.substr(start, comma - start);
        size_t fstart = item.find_first_not_of(L" \t");
        size_t fend = item.find_last_not_of(L" \t");
        if (fstart != std::wstring::npos && fend != std::wstring::npos) {
            comp.defines.push_back(item.substr(fstart, fend - fstart + 1));
        }
        start = comma + 1;
    }

    // Linker
    idx = linkerSubsystem_.GetCurSel();
    if (idx >= 0) {
        const wchar_t* subsystems[] = {L"console", L"windows", L"native",
                                        L"efi_application", L"boot_application", L"posix"};
        if (idx < 6) link.subsystem = subsystems[idx];
    }

    // Parse libraries and paths
    auto parseCSV = [](CEdit& edit, std::vector<std::wstring>& out) {
        wchar_t b[1024];
        edit.GetWindowText(b, 1024);
        std::wstring str = b;
        out.clear();
        size_t s = 0;
        while (s < str.length()) {
            size_t c = str.find(L',', s);
            if (c == std::wstring::npos) c = str.length();
            std::wstring item = str.substr(s, c - s);
            size_t fs = item.find_first_not_of(L" \t");
            size_t fe = item.find_last_not_of(L" \t");
            if (fs != std::wstring::npos && fe != std::wstring::npos) {
                out.push_back(item.substr(fs, fe - fs + 1));
            }
            s = c + 1;
        }
    };

    parseCSV(linkerLibs_, link.libraries);
    parseCSV(linkerLibPaths_, link.libraryPaths);

    linkerEntry_.GetWindowText(buf, 1024);
    link.entryPoint = buf;

    linkerDefFile_.GetWindowText(buf, 1024);
    link.defFile = buf;

    link.aslr = linkerAslr_.GetCheck() == BST_CHECKED;
    link.dep = linkerDep_.GetCheck() == BST_CHECKED;
    link.lto = linkerLto_.GetCheck() == BST_CHECKED;
    link.cfgLinker = linkerCfgLink_.GetCheck() == BST_CHECKED;
    link.generateMap = linkerMap_.GetCheck() == BST_CHECKED;
    link.generateDebugInfo = linkerDebugInfo_.GetCheck() == BST_CHECKED;

    // Sources
    parseCSV(sourcesInclude_, srcs.includeDirs);
    parseCSV(sourcesSource_, srcs.sourceDirs);
    parseCSV(sourcesExclude_, srcs.excludePatterns);
    parseCSV(sourcesExternal_, srcs.externalDirs);

    // Resources
    res.enabled = resourcesEnabled_.GetCheck() == BST_CHECKED;
    parseCSV(resourcesFiles_, res.files);

    // Driver
    drv.enabled = driverEnabled_.GetCheck() == BST_CHECKED;

    idx = driverType_.GetCurSel();
    if (idx >= 0) {
        driverType_.GetLBText(idx, buf);
        drv.type = ExtractValue(buf);
    }

    driverEntry_.GetWindowText(buf, 1024);
    drv.entryPoint = buf;

    idx = driverTargetOs_.GetCurSel();
    if (idx >= 0) {
        const wchar_t* targets[] = {L"win7", L"win8", L"win81", L"win10", L"win11"};
        if (idx < 5) drv.targetOs = targets[idx];
    }

    drv.minifilter = driverMinifilter_.GetCheck() == BST_CHECKED;

    config_.SetModified();
    UpdateTitleBar();
}

void MainWindow::UpdateTitleBar() {
    if (config_.IsModified()) {
        SetWindowText(L"vcbuild Config Generator *");
    } else {
        SetWindowText(L"vcbuild Config Generator");
    }
}

void MainWindow::OnSize(UINT, CSize size) {
    if (!tabCtrl_.IsWindow()) return;

    int w = size.cx;
    int h = size.cy;
    int btnWidth = 80;
    int btnHeight = 24;

    tabCtrl_.MoveWindow(kMargin, kMargin, w - 2 * kMargin, h - 44);
    saveBtn_.MoveWindow(w - btnWidth - kMargin, h - 34, btnWidth, btnHeight);
}

void MainWindow::OnDestroy() {
    if (hFont_) DeleteObject(hFont_);
    if (hSmallFont_) DeleteObject(hSmallFont_);
    PostQuitMessage(0);
}

void MainWindow::OnClose() {
    if (config_.IsModified()) {
        int res = MessageBox(L"Save changes before closing?", L"vcbuild Config Generator",
                             MB_YESNOCANCEL | MB_ICONQUESTION);
        if (res == IDCANCEL) return;
        if (res == IDYES) {
            SaveUIToConfig();
            config_.Save(configPath_);
        }
    }
    DestroyWindow();
}

void MainWindow::OnSave(UINT, int, HWND) {
    SaveUIToConfig();
    if (config_.Save(configPath_)) {
        config_.SetModified(false);
        UpdateTitleBar();
        MessageBox(L"Configuration saved successfully.", L"vcbuild Config Generator", MB_ICONINFORMATION);
    } else {
        MessageBox(L"Failed to save configuration.", L"Error", MB_ICONERROR);
    }
}

LRESULT MainWindow::OnTabChange(LPNMHDR) {
    ShowPage(tabCtrl_.GetCurSel());
    return 0;
}

} // namespace vcbuild::gui
