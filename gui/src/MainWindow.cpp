#include "MainWindow.hpp"

// Helper to convert CRect to _U_RECT for WTL control Create methods
inline ATL::_U_RECT RC(int l, int t, int r, int b) {
    static RECT rc;  // Static to ensure it persists
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
    // Extract first word for items like "C++20 (Default)" -> "c++20"
    size_t space = text.find(L' ');
    if (space != std::wstring::npos) {
        std::wstring val = text.substr(0, space);
        // Convert to lowercase for standards
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

    // Compact window size - reduced height to eliminate empty space
    // Use fixed-size window style (no resize, no maximize)
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    RECT rc = {0, 0, 560, 380};
    AdjustWindowRect(&rc, style, FALSE);

    Create(nullptr, rc, L"vcbuild Config Generator", style, WS_EX_APPWINDOW);

    if (!m_hWnd) return false;

    CenterWindow();
    ShowWindow(SW_SHOW);
    UpdateWindow();

    return true;
}

int MainWindow::OnCreate(LPCREATESTRUCT) {
    CreateControls();
    LoadConfigToUI();
    ShowPage(0);
    return 0;
}

void MainWindow::CreateControls() {
    // Modern font for better readability
    HFONT hFont = CreateFont(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    CRect rc;
    GetClientRect(&rc);
    int w = rc.Width();
    int h = rc.Height();

    // Tab control fills most of the window
    tabCtrl_.Create(m_hWnd, RC(kMargin, kMargin, w - kMargin, h - 50),
                    nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | TCS_TABS,
                    0, IDC_TAB);
    tabCtrl_.SetFont(hFont);

    tabCtrl_.InsertItem(0, L" Project ");
    tabCtrl_.InsertItem(1, L" Compiler ");
    tabCtrl_.InsertItem(2, L" Linker ");
    tabCtrl_.InsertItem(3, L" Sources ");
    tabCtrl_.InsertItem(4, L" Resources ");

    // Save button at the bottom right - standard Windows button size (75x23)
    int btnWidth = 75;
    int btnHeight = 23;
    int btnY = h - 35;

    saveBtn_.Create(m_hWnd, RC(w - btnWidth - kMargin, btnY,
                               w - kMargin, btnY + btnHeight),
                    L"Save", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON | BS_DEFPUSHBUTTON,
                    0, IDC_SAVE);
    saveBtn_.SetFont(hFont);

    CreateProjectPage();
    CreateCompilerPage();
    CreateLinkerPage();
    CreateSourcesPage();
    CreateResourcesPage();
}

void MainWindow::CreateProjectPage() {
    HFONT hFont = CreateFont(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    int groupX = kMargin + 6;
    int groupW = 520;
    int x = groupX + 12;
    int y = 50;
    int ctrlX = x + kLabelWidth + 10;
    int ctrlW = groupW - kLabelWidth - 36;
    int rowSpacing = 28;

    // Group box for project settings
    HWND groupBox = CreateWindow(L"BUTTON", L"Project Configuration",
                                  WS_CHILD | BS_GROUPBOX,
                                  groupX, 35, groupW, 135,
                                  m_hWnd, nullptr, nullptr, nullptr);
    SendMessage(groupBox, WM_SETFONT, (WPARAM)hFont, TRUE);
    projectCtrls_.push_back(groupBox);

    auto label = [&](const wchar_t* text) {
        HWND h = CreateWindow(L"STATIC", text, WS_CHILD | SS_LEFT,
                              x, y + 3, kLabelWidth, kCtrlHeight,
                              m_hWnd, nullptr, nullptr, nullptr);
        SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE);
        projectCtrls_.push_back(h);
    };

    label(L"Project Name:");
    projectName_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 21),
                        nullptr, WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL);
    projectName_.SetFont(hFont);
    projectCtrls_.push_back(projectName_);
    y += rowSpacing;

    label(L"Output Type:");
    projectType_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 120),
                        nullptr, WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST);
    projectType_.SetFont(hFont);
    projectType_.AddString(L"Executable (exe)");
    projectType_.AddString(L"Dynamic Library (dll)");
    projectType_.AddString(L"Static Library (lib)");
    projectCtrls_.push_back(projectType_);
    y += rowSpacing;

    label(L"Architecture:");
    projectArch_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 120),
                        nullptr, WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST);
    projectArch_.SetFont(hFont);
    projectArch_.AddString(L"x64 (64-bit)");
    projectArch_.AddString(L"x86 (32-bit)");
    projectArch_.AddString(L"ARM64");
    projectCtrls_.push_back(projectArch_);
    y += rowSpacing;

    label(L"Output Directory:");
    projectOutDir_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 21),
                          nullptr, WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL);
    projectOutDir_.SetFont(hFont);
    projectCtrls_.push_back(projectOutDir_);

    // Help text below group box
    y = 178;
    HWND desc = CreateWindow(L"STATIC",
                             L"Configure basic project settings and output parameters.",
                             WS_CHILD | SS_LEFT,
                             x, y, groupW - 20, 18,
                             m_hWnd, nullptr, nullptr, nullptr);
    HFONT smallFont = CreateFont(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessage(desc, WM_SETFONT, (WPARAM)smallFont, TRUE);
    projectCtrls_.push_back(desc);
}

void MainWindow::CreateCompilerPage() {
    HFONT hFont = CreateFont(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    int groupX = kMargin + 6;
    int groupW = 520;
    int x = groupX + 12;
    int y = 50;
    int ctrlX = x + kLabelWidth + 10;
    int ctrlW = groupW - kLabelWidth - 36;
    int rowSpacing = 28;

    // Group box for language & runtime settings
    HWND groupBox1 = CreateWindow(L"BUTTON", L"Language && Runtime",
                                   WS_CHILD | BS_GROUPBOX,
                                   groupX, 35, groupW, 107,
                                   m_hWnd, nullptr, nullptr, nullptr);
    SendMessage(groupBox1, WM_SETFONT, (WPARAM)hFont, TRUE);
    compilerCtrls_.push_back(groupBox1);

    auto label = [&](const wchar_t* text) {
        HWND h = CreateWindow(L"STATIC", text, WS_CHILD | SS_LEFT,
                              x, y + 3, kLabelWidth, kCtrlHeight,
                              m_hWnd, nullptr, nullptr, nullptr);
        SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE);
        compilerCtrls_.push_back(h);
    };

    label(L"C++ Standard:");
    compilerStd_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 120),
                        nullptr, WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST);
    compilerStd_.SetFont(hFont);
    compilerStd_.AddString(L"C++17");
    compilerStd_.AddString(L"C++20 (Default)");
    compilerStd_.AddString(L"C++23");
    compilerStd_.AddString(L"C++ Latest");
    compilerCtrls_.push_back(compilerStd_);
    y += rowSpacing;

    label(L"Runtime Library:");
    compilerRuntime_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 120),
                            nullptr, WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST);
    compilerRuntime_.SetFont(hFont);
    compilerRuntime_.AddString(L"Dynamic (DLL)");
    compilerRuntime_.AddString(L"Static");
    compilerCtrls_.push_back(compilerRuntime_);
    y += rowSpacing;

    label(L"Preprocessor:");
    compilerDefines_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 21),
                            nullptr, WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL);
    compilerDefines_.SetFont(hFont);
    compilerCtrls_.push_back(compilerDefines_);

    // Group box for compiler options
    int group2Y = 148;
    y = group2Y + 20;
    HWND groupBox2 = CreateWindow(L"BUTTON", L"Compiler Options",
                                   WS_CHILD | BS_GROUPBOX,
                                   groupX, group2Y, groupW, 68,
                                   m_hWnd, nullptr, nullptr, nullptr);
    SendMessage(groupBox2, WM_SETFONT, (WPARAM)hFont, TRUE);
    compilerCtrls_.push_back(groupBox2);

    int checkCol1 = x;
    int checkCol2 = x + 175;
    int checkCol3 = x + 330;

    compilerExceptions_.Create(m_hWnd, RC(checkCol1, y, checkCol1 + 160, y + 20),
                               L"C++ Exceptions", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX);
    compilerExceptions_.SetFont(hFont);
    compilerCtrls_.push_back(compilerExceptions_);

    compilerParallel_.Create(m_hWnd, RC(checkCol2, y, checkCol2 + 145, y + 20),
                             L"Parallel Build (/MP)", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX);
    compilerParallel_.SetFont(hFont);
    compilerCtrls_.push_back(compilerParallel_);

    compilerBuffer_.Create(m_hWnd, RC(checkCol3, y, checkCol3 + 170, y + 20),
                           L"Security Checks (/GS)", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX);
    compilerBuffer_.SetFont(hFont);
    compilerCtrls_.push_back(compilerBuffer_);
}

void MainWindow::CreateLinkerPage() {
    HFONT hFont = CreateFont(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    int groupX = kMargin + 6;
    int groupW = 520;
    int x = groupX + 12;
    int y = 50;
    int ctrlX = x + kLabelWidth + 10;
    int ctrlW = groupW - kLabelWidth - 36;
    int rowSpacing = 28;

    // Group box for output & libraries
    HWND groupBox1 = CreateWindow(L"BUTTON", L"Output && Libraries",
                                   WS_CHILD | BS_GROUPBOX,
                                   groupX, 35, groupW, 78,
                                   m_hWnd, nullptr, nullptr, nullptr);
    SendMessage(groupBox1, WM_SETFONT, (WPARAM)hFont, TRUE);
    linkerCtrls_.push_back(groupBox1);

    auto label = [&](const wchar_t* text) {
        HWND h = CreateWindow(L"STATIC", text, WS_CHILD | SS_LEFT,
                              x, y + 3, kLabelWidth, kCtrlHeight,
                              m_hWnd, nullptr, nullptr, nullptr);
        SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE);
        linkerCtrls_.push_back(h);
    };

    label(L"Link Libraries:");
    linkerLibs_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 21),
                       nullptr, WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL);
    linkerLibs_.SetFont(hFont);
    linkerCtrls_.push_back(linkerLibs_);
    y += rowSpacing;

    label(L"Subsystem:");
    linkerSubsystem_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 120),
                            nullptr, WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST);
    linkerSubsystem_.SetFont(hFont);
    linkerSubsystem_.AddString(L"Console Application");
    linkerSubsystem_.AddString(L"Windows GUI");
    linkerSubsystem_.AddString(L"Native Driver");
    linkerCtrls_.push_back(linkerSubsystem_);

    // Group box for security & optimization
    int group2Y = 118;
    y = group2Y + 20;
    HWND groupBox2 = CreateWindow(L"BUTTON", L"Security && Optimization",
                                   WS_CHILD | BS_GROUPBOX,
                                   groupX, group2Y, groupW, 68,
                                   m_hWnd, nullptr, nullptr, nullptr);
    SendMessage(groupBox2, WM_SETFONT, (WPARAM)hFont, TRUE);
    linkerCtrls_.push_back(groupBox2);

    int checkCol1 = x;
    int checkCol2 = x + 175;
    int checkCol3 = x + 330;

    linkerAslr_.Create(m_hWnd, RC(checkCol1, y, checkCol1 + 165, y + 20),
                       L"ASLR (Randomization)", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX);
    linkerAslr_.SetFont(hFont);
    linkerCtrls_.push_back(linkerAslr_);

    linkerDep_.Create(m_hWnd, RC(checkCol2, y, checkCol2 + 145, y + 20),
                      L"DEP (No-Execute)", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX);
    linkerDep_.SetFont(hFont);
    linkerCtrls_.push_back(linkerDep_);

    linkerLto_.Create(m_hWnd, RC(checkCol3, y, checkCol3 + 170, y + 20),
                      L"Link-Time Codegen", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX);
    linkerLto_.SetFont(hFont);
    linkerCtrls_.push_back(linkerLto_);
}

void MainWindow::CreateSourcesPage() {
    HFONT hFont = CreateFont(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    int groupX = kMargin + 6;
    int groupW = 520;
    int x = groupX + 12;
    int y = 50;
    int ctrlX = x + kLabelWidth + 10;
    int ctrlW = groupW - kLabelWidth - 36;
    int rowSpacing = 28;

    // Group box for source paths
    HWND groupBox = CreateWindow(L"BUTTON", L"Source && Include Directories",
                                  WS_CHILD | BS_GROUPBOX,
                                  groupX, 35, groupW, 135,
                                  m_hWnd, nullptr, nullptr, nullptr);
    SendMessage(groupBox, WM_SETFONT, (WPARAM)hFont, TRUE);
    sourcesCtrls_.push_back(groupBox);

    auto label = [&](const wchar_t* text) {
        HWND h = CreateWindow(L"STATIC", text, WS_CHILD | SS_LEFT,
                              x, y + 3, kLabelWidth, kCtrlHeight,
                              m_hWnd, nullptr, nullptr, nullptr);
        SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE);
        sourcesCtrls_.push_back(h);
    };

    label(L"Include Directories:");
    sourcesInclude_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 21),
                           nullptr, WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL);
    sourcesInclude_.SetFont(hFont);
    sourcesCtrls_.push_back(sourcesInclude_);
    y += rowSpacing;

    label(L"Source Directories:");
    sourcesSource_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 21),
                          nullptr, WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL);
    sourcesSource_.SetFont(hFont);
    sourcesCtrls_.push_back(sourcesSource_);
    y += rowSpacing;

    label(L"Exclude Patterns:");
    sourcesExclude_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 21),
                           nullptr, WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL);
    sourcesExclude_.SetFont(hFont);
    sourcesCtrls_.push_back(sourcesExclude_);
    y += rowSpacing;

    label(L"External Directories:");
    sourcesExternal_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 21),
                            nullptr, WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL);
    sourcesExternal_.SetFont(hFont);
    sourcesCtrls_.push_back(sourcesExternal_);

    // Help text below group box
    y = 178;
    HWND desc = CreateWindow(L"STATIC",
                             L"Tip: Separate multiple paths with commas (,)",
                             WS_CHILD | SS_LEFT,
                             x, y, groupW - 20, 18,
                             m_hWnd, nullptr, nullptr, nullptr);
    HFONT smallFont = CreateFont(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessage(desc, WM_SETFONT, (WPARAM)smallFont, TRUE);
    sourcesCtrls_.push_back(desc);
}

void MainWindow::CreateResourcesPage() {
    HFONT hFont = CreateFont(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    int groupX = kMargin + 6;
    int groupW = 520;
    int x = groupX + 12;
    int y = 50;
    int ctrlX = x + kLabelWidth + 10;
    int ctrlW = groupW - kLabelWidth - 36;
    int rowSpacing = 28;

    // Group box for resource settings
    HWND groupBox = CreateWindow(L"BUTTON", L"Resource Compilation",
                                  WS_CHILD | BS_GROUPBOX,
                                  groupX, 35, groupW, 107,
                                  m_hWnd, nullptr, nullptr, nullptr);
    SendMessage(groupBox, WM_SETFONT, (WPARAM)hFont, TRUE);
    resourcesCtrls_.push_back(groupBox);

    resourcesEnabled_.Create(m_hWnd, RC(x, y, x + 200, y + 20),
                             L"Enable Resource Compilation", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX);
    resourcesEnabled_.SetFont(hFont);
    resourcesCtrls_.push_back(resourcesEnabled_);
    y += rowSpacing;

    auto label = [&](const wchar_t* text) {
        HWND h = CreateWindow(L"STATIC", text, WS_CHILD | SS_LEFT,
                              x, y + 3, kLabelWidth, kCtrlHeight,
                              m_hWnd, nullptr, nullptr, nullptr);
        SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE);
        resourcesCtrls_.push_back(h);
    };

    label(L"Resource Files:");
    resourcesFiles_.Create(m_hWnd, RC(ctrlX, y, ctrlX + ctrlW, y + 21),
                           nullptr, WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL);
    resourcesFiles_.SetFont(hFont);
    resourcesCtrls_.push_back(resourcesFiles_);

    // Help text below group box
    y = 150;
    HWND desc = CreateWindow(L"STATIC",
                             L"Compile Windows resource files (.rc) containing icons, manifests, version info, etc.",
                             WS_CHILD | SS_LEFT,
                             x, y, groupW - 20, 36,
                             m_hWnd, nullptr, nullptr, nullptr);
    HFONT smallFont = CreateFont(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessage(desc, WM_SETFONT, (WPARAM)smallFont, TRUE);
    resourcesCtrls_.push_back(desc);
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
    currentPage_ = index;
}

void MainWindow::LoadConfigToUI() {
    auto& proj = config_.Project();
    auto& comp = config_.Compiler();
    auto& link = config_.Linker();
    auto& srcs = config_.Sources();
    auto& res = config_.Resources();

    projectName_.SetWindowText(proj.name.c_str());
    
    auto selectCombo = [](CComboBox& cb, const std::wstring& val) {
        int idx = cb.FindStringExact(0, val.c_str());
        cb.SetCurSel(idx >= 0 ? idx : 0);
    };

    selectCombo(projectType_, proj.type);
    selectCombo(projectArch_, proj.architecture);
    projectOutDir_.SetWindowText(proj.outputDir.c_str());

    selectCombo(compilerStd_, comp.standard);
    selectCombo(compilerRuntime_, comp.runtime);
    compilerExceptions_.SetCheck(comp.exceptions ? BST_CHECKED : BST_UNCHECKED);
    compilerParallel_.SetCheck(comp.parallel ? BST_CHECKED : BST_UNCHECKED);
    compilerBuffer_.SetCheck(comp.bufferChecks ? BST_CHECKED : BST_UNCHECKED);

    selectCombo(linkerSubsystem_, link.subsystem);
    linkerAslr_.SetCheck(link.aslr ? BST_CHECKED : BST_UNCHECKED);
    linkerDep_.SetCheck(link.dep ? BST_CHECKED : BST_UNCHECKED);
    linkerLto_.SetCheck(link.lto ? BST_CHECKED : BST_UNCHECKED);

    auto joinVec = [](const std::vector<std::wstring>& v) -> std::wstring {
        std::wstring out;
        for (size_t i = 0; i < v.size(); ++i) {
            if (i > 0) out += L", ";
            out += v[i];
        }
        return out;
    };

    sourcesInclude_.SetWindowText(joinVec(srcs.includeDirs).c_str());
    sourcesSource_.SetWindowText(joinVec(srcs.sourceDirs).c_str());

    resourcesEnabled_.SetCheck(res.enabled ? BST_CHECKED : BST_UNCHECKED);
    resourcesFiles_.SetWindowText(joinVec(res.files).c_str());
}

void MainWindow::SaveUIToConfig() {
    auto& proj = config_.Project();
    auto& comp = config_.Compiler();
    auto& link = config_.Linker();
    auto& srcs = config_.Sources();
    auto& res = config_.Resources();

    wchar_t buf[512];
    
    projectName_.GetWindowText(buf, 512);
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

    projectOutDir_.GetWindowText(buf, 512);
    proj.outputDir = buf;

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

    comp.exceptions = compilerExceptions_.GetCheck() == BST_CHECKED;
    comp.parallel = compilerParallel_.GetCheck() == BST_CHECKED;
    comp.bufferChecks = compilerBuffer_.GetCheck() == BST_CHECKED;

    idx = linkerSubsystem_.GetCurSel();
    if (idx >= 0) {
        linkerSubsystem_.GetLBText(idx, buf);
        std::wstring val = ExtractValue(buf);
        // Map display names back to config values
        if (val.find(L"console") != std::wstring::npos) val = L"console";
        else if (val.find(L"windows") != std::wstring::npos) val = L"windows";
        else if (val.find(L"native") != std::wstring::npos) val = L"native";
        link.subsystem = val;
    }

    link.aslr = linkerAslr_.GetCheck() == BST_CHECKED;
    link.dep = linkerDep_.GetCheck() == BST_CHECKED;
    link.lto = linkerLto_.GetCheck() == BST_CHECKED;

    res.enabled = resourcesEnabled_.GetCheck() == BST_CHECKED;

    // Parse resource files (comma-separated)
    resourcesFiles_.GetWindowText(buf, 512);
    std::wstring resFilesStr = buf;
    res.files.clear();
    size_t start = 0;
    while (start < resFilesStr.length()) {
        size_t comma = resFilesStr.find(L',', start);
        if (comma == std::wstring::npos) comma = resFilesStr.length();
        std::wstring file = resFilesStr.substr(start, comma - start);
        // Trim whitespace
        size_t fstart = file.find_first_not_of(L" \t");
        size_t fend = file.find_last_not_of(L" \t");
        if (fstart != std::wstring::npos && fend != std::wstring::npos) {
            res.files.push_back(file.substr(fstart, fend - fstart + 1));
        }
        start = comma + 1;
    }

    config_.SetModified();
}

void MainWindow::OnSize(UINT, CSize size) {
    if (!tabCtrl_.IsWindow()) return;

    int w = size.cx;
    int h = size.cy;
    int btnWidth = 75;
    int btnHeight = 23;

    tabCtrl_.MoveWindow(kMargin, kMargin, w - 2 * kMargin, h - 50);
    saveBtn_.MoveWindow(w - btnWidth - kMargin, h - 35, btnWidth, btnHeight);
}

void MainWindow::OnDestroy() {
    PostQuitMessage(0);
}

void MainWindow::OnClose() {
    if (config_.IsModified()) {
        int res = MessageBox(L"Save changes?", L"vcbuild",
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
        MessageBox(L"Configuration saved", L"vcbuild", MB_ICONINFORMATION);
    } else {
        MessageBox(L"Failed to save", L"Error", MB_ICONERROR);
    }
}

LRESULT MainWindow::OnTabChange(LPNMHDR) {
    ShowPage(tabCtrl_.GetCurSel());
    return 0;
}

} // namespace vcbuild::gui
