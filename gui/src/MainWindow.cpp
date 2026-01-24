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

    // Larger window for better layout
    RECT rc = {0, 0, 620, 520};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    Create(nullptr, rc, L"vcbuild Configuration Editor",
           WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
           WS_EX_APPWINDOW);

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
    // Modern font - slightly larger for better readability
    HFONT hFont = CreateFont(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    CRect rc;
    GetClientRect(&rc);
    int w = rc.Width();

    // Create tab control with modern styling
    tabCtrl_.Create(m_hWnd, RC(kMargin, kMargin, w - kMargin, 430),
                    nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_TABS,
                    0, IDC_TAB);
    tabCtrl_.SetFont(hFont);

    tabCtrl_.InsertItem(0, L"  Project  ");
    tabCtrl_.InsertItem(1, L"  Compiler  ");
    tabCtrl_.InsertItem(2, L"  Linker  ");
    tabCtrl_.InsertItem(3, L"  Sources  ");

    // Modern buttons with better positioning
    saveBtn_.Create(m_hWnd, RC(w - 195, 445, w - 105, 475),
                    L"💾 Save", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_DEFPUSHBUTTON,
                    0, IDC_SAVE);
    saveBtn_.SetFont(hFont);

    reloadBtn_.Create(m_hWnd, RC(w - 95, 445, w - kMargin, 475),
                      L"🔄 Reload", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      0, IDC_RELOAD);
    reloadBtn_.SetFont(hFont);

    CreateProjectPage();
    CreateCompilerPage();
    CreateLinkerPage();
    CreateSourcesPage();
}

void MainWindow::CreateProjectPage() {
    HFONT hFont = CreateFont(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    int x = kMargin + 20;
    int y = 55;
    int ctrlW = 420;

    // Group box for project settings
    HWND groupBox = CreateWindow(L"BUTTON", L" Project Configuration ",
                                  WS_CHILD | BS_GROUPBOX,
                                  kMargin + 10, 40, 580, 180,
                                  m_hWnd, nullptr, nullptr, nullptr);
    SendMessage(groupBox, WM_SETFONT, (WPARAM)hFont, TRUE);
    projectCtrls_.push_back(groupBox);

    auto label = [&](const wchar_t* text) {
        HWND h = CreateWindow(L"STATIC", text, WS_CHILD | SS_LEFT,
                              x, y + 5, kLabelWidth + 20, kCtrlHeight,
                              m_hWnd, nullptr, nullptr, nullptr);
        SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE);
        projectCtrls_.push_back(h);
    };

    label(L"Project Name:");
    projectName_.Create(m_hWnd, RC(x + kLabelWidth + 20, y, x + ctrlW, y + kCtrlHeight + 4),
                        nullptr, WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | WS_EX_CLIENTEDGE);
    projectName_.SetFont(hFont);
    projectCtrls_.push_back(projectName_);
    y += 38;

    label(L"Output Type:");
    projectType_.Create(m_hWnd, RC(x + kLabelWidth + 20, y, x + ctrlW, y + 140),
                        nullptr, WS_CHILD | CBS_DROPDOWNLIST);
    projectType_.SetFont(hFont);
    projectType_.AddString(L"Executable (exe)");
    projectType_.AddString(L"Dynamic Library (dll)");
    projectType_.AddString(L"Static Library (lib)");
    projectCtrls_.push_back(projectType_);
    y += 38;

    label(L"Architecture:");
    projectArch_.Create(m_hWnd, RC(x + kLabelWidth + 20, y, x + ctrlW, y + 140),
                        nullptr, WS_CHILD | CBS_DROPDOWNLIST);
    projectArch_.SetFont(hFont);
    projectArch_.AddString(L"x64 (64-bit)");
    projectArch_.AddString(L"x86 (32-bit)");
    projectArch_.AddString(L"ARM64");
    projectCtrls_.push_back(projectArch_);
    y += 38;

    label(L"Output Directory:");
    projectOutDir_.Create(m_hWnd, RC(x + kLabelWidth + 20, y, x + ctrlW, y + kCtrlHeight + 4),
                          nullptr, WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | WS_EX_CLIENTEDGE);
    projectOutDir_.SetFont(hFont);
    projectCtrls_.push_back(projectOutDir_);

    // Add descriptive text
    y += 50;
    HWND desc = CreateWindow(L"STATIC",
                             L"Configure basic project settings and output parameters.",
                             WS_CHILD | SS_LEFT,
                             x, y, 540, 40,
                             m_hWnd, nullptr, nullptr, nullptr);
    HFONT smallFont = CreateFont(-11, 0, 0, 0, FW_NORMAL, TRUE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessage(desc, WM_SETFONT, (WPARAM)smallFont, TRUE);
    projectCtrls_.push_back(desc);
}

void MainWindow::CreateCompilerPage() {
    HFONT hFont = CreateFont(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    int x = kMargin + 20;
    int y = 55;
    int ctrlW = 420;

    // Group box for compiler settings
    HWND groupBox1 = CreateWindow(L"BUTTON", L" Language & Runtime ",
                                   WS_CHILD | BS_GROUPBOX,
                                   kMargin + 10, 40, 580, 140,
                                   m_hWnd, nullptr, nullptr, nullptr);
    SendMessage(groupBox1, WM_SETFONT, (WPARAM)hFont, TRUE);
    compilerCtrls_.push_back(groupBox1);

    auto label = [&](const wchar_t* text) {
        HWND h = CreateWindow(L"STATIC", text, WS_CHILD | SS_LEFT,
                              x, y + 5, kLabelWidth + 20, kCtrlHeight,
                              m_hWnd, nullptr, nullptr, nullptr);
        SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE);
        compilerCtrls_.push_back(h);
    };

    label(L"C++ Standard:");
    compilerStd_.Create(m_hWnd, RC(x + kLabelWidth + 20, y, x + ctrlW, y + 140),
                        nullptr, WS_CHILD | CBS_DROPDOWNLIST);
    compilerStd_.SetFont(hFont);
    compilerStd_.AddString(L"C++17");
    compilerStd_.AddString(L"C++20 (Default)");
    compilerStd_.AddString(L"C++23");
    compilerStd_.AddString(L"C++ Latest");
    compilerCtrls_.push_back(compilerStd_);
    y += 38;

    label(L"Runtime Library:");
    compilerRuntime_.Create(m_hWnd, RC(x + kLabelWidth + 20, y, x + ctrlW, y + 140),
                            nullptr, WS_CHILD | CBS_DROPDOWNLIST);
    compilerRuntime_.SetFont(hFont);
    compilerRuntime_.AddString(L"Dynamic (DLL)");
    compilerRuntime_.AddString(L"Static");
    compilerCtrls_.push_back(compilerRuntime_);
    y += 38;

    label(L"Preprocessor:");
    compilerDefines_.Create(m_hWnd, RC(x + kLabelWidth + 20, y, x + ctrlW, y + kCtrlHeight + 4),
                            nullptr, WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | WS_EX_CLIENTEDGE);
    compilerDefines_.SetFont(hFont);
    compilerCtrls_.push_back(compilerDefines_);
    y += 50;

    // Group box for compiler options
    HWND groupBox2 = CreateWindow(L"BUTTON", L" Compiler Options ",
                                   WS_CHILD | BS_GROUPBOX,
                                   kMargin + 10, y, 580, 90,
                                   m_hWnd, nullptr, nullptr, nullptr);
    SendMessage(groupBox2, WM_SETFONT, (WPARAM)hFont, TRUE);
    compilerCtrls_.push_back(groupBox2);
    y += 25;

    compilerExceptions_.Create(m_hWnd, RC(x, y, x + 180, y + 24),
                               L"Enable C++ Exceptions", WS_CHILD | BS_AUTOCHECKBOX);
    compilerExceptions_.SetFont(hFont);
    compilerCtrls_.push_back(compilerExceptions_);

    compilerParallel_.Create(m_hWnd, RC(x + 200, y, x + 400, y + 24),
                             L"Parallel Build (/MP)", WS_CHILD | BS_AUTOCHECKBOX);
    compilerParallel_.SetFont(hFont);
    compilerCtrls_.push_back(compilerParallel_);
    y += 32;

    compilerBuffer_.Create(m_hWnd, RC(x, y, x + 200, y + 24),
                           L"Buffer Security Checks (/GS)", WS_CHILD | BS_AUTOCHECKBOX);
    compilerBuffer_.SetFont(hFont);
    compilerCtrls_.push_back(compilerBuffer_);
}

void MainWindow::CreateLinkerPage() {
    HFONT hFont = CreateFont(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    int x = kMargin + 20;
    int y = 55;
    int ctrlW = 420;

    // Group box for linker settings
    HWND groupBox1 = CreateWindow(L"BUTTON", L" Output & Libraries ",
                                   WS_CHILD | BS_GROUPBOX,
                                   kMargin + 10, 40, 580, 100,
                                   m_hWnd, nullptr, nullptr, nullptr);
    SendMessage(groupBox1, WM_SETFONT, (WPARAM)hFont, TRUE);
    linkerCtrls_.push_back(groupBox1);

    auto label = [&](const wchar_t* text) {
        HWND h = CreateWindow(L"STATIC", text, WS_CHILD | SS_LEFT,
                              x, y + 5, kLabelWidth + 20, kCtrlHeight,
                              m_hWnd, nullptr, nullptr, nullptr);
        SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE);
        linkerCtrls_.push_back(h);
    };

    label(L"Link Libraries:");
    linkerLibs_.Create(m_hWnd, RC(x + kLabelWidth + 20, y, x + ctrlW, y + kCtrlHeight + 4),
                       nullptr, WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | WS_EX_CLIENTEDGE);
    linkerLibs_.SetFont(hFont);
    linkerCtrls_.push_back(linkerLibs_);
    y += 38;

    label(L"Subsystem:");
    linkerSubsystem_.Create(m_hWnd, RC(x + kLabelWidth + 20, y, x + ctrlW, y + 140),
                            nullptr, WS_CHILD | CBS_DROPDOWNLIST);
    linkerSubsystem_.SetFont(hFont);
    linkerSubsystem_.AddString(L"Console Application");
    linkerSubsystem_.AddString(L"Windows GUI");
    linkerSubsystem_.AddString(L"Native Driver");
    linkerCtrls_.push_back(linkerSubsystem_);
    y += 50;

    // Group box for security & optimization
    HWND groupBox2 = CreateWindow(L"BUTTON", L" Security & Optimization ",
                                   WS_CHILD | BS_GROUPBOX,
                                   kMargin + 10, y, 580, 90,
                                   m_hWnd, nullptr, nullptr, nullptr);
    SendMessage(groupBox2, WM_SETFONT, (WPARAM)hFont, TRUE);
    linkerCtrls_.push_back(groupBox2);
    y += 25;

    linkerAslr_.Create(m_hWnd, RC(x, y, x + 180, y + 24),
                       L"ASLR (Address Randomization)", WS_CHILD | BS_AUTOCHECKBOX);
    linkerAslr_.SetFont(hFont);
    linkerCtrls_.push_back(linkerAslr_);

    linkerDep_.Create(m_hWnd, RC(x + 200, y, x + 400, y + 24),
                      L"DEP (Data Execution Prevention)", WS_CHILD | BS_AUTOCHECKBOX);
    linkerDep_.SetFont(hFont);
    linkerCtrls_.push_back(linkerDep_);
    y += 32;

    linkerLto_.Create(m_hWnd, RC(x, y, x + 220, y + 24),
                      L"Link-Time Code Generation (/LTCG)", WS_CHILD | BS_AUTOCHECKBOX);
    linkerLto_.SetFont(hFont);
    linkerCtrls_.push_back(linkerLto_);
}

void MainWindow::CreateSourcesPage() {
    HFONT hFont = CreateFont(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    int x = kMargin + 20;
    int y = 55;
    int ctrlW = 420;

    // Group box for source paths
    HWND groupBox = CreateWindow(L"BUTTON", L" Source & Include Directories ",
                                  WS_CHILD | BS_GROUPBOX,
                                  kMargin + 10, 40, 580, 200,
                                  m_hWnd, nullptr, nullptr, nullptr);
    SendMessage(groupBox, WM_SETFONT, (WPARAM)hFont, TRUE);
    sourcesCtrls_.push_back(groupBox);

    auto label = [&](const wchar_t* text) {
        HWND h = CreateWindow(L"STATIC", text, WS_CHILD | SS_LEFT,
                              x, y + 5, kLabelWidth + 20, kCtrlHeight,
                              m_hWnd, nullptr, nullptr, nullptr);
        SendMessage(h, WM_SETFONT, (WPARAM)hFont, TRUE);
        sourcesCtrls_.push_back(h);
    };

    label(L"Include Directories:");
    sourcesInclude_.Create(m_hWnd, RC(x + kLabelWidth + 20, y, x + ctrlW, y + kCtrlHeight + 4),
                           nullptr, WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | WS_EX_CLIENTEDGE);
    sourcesInclude_.SetFont(hFont);
    sourcesCtrls_.push_back(sourcesInclude_);
    y += 38;

    label(L"Source Directories:");
    sourcesSource_.Create(m_hWnd, RC(x + kLabelWidth + 20, y, x + ctrlW, y + kCtrlHeight + 4),
                          nullptr, WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | WS_EX_CLIENTEDGE);
    sourcesSource_.SetFont(hFont);
    sourcesCtrls_.push_back(sourcesSource_);
    y += 38;

    label(L"Exclude Patterns:");
    sourcesExclude_.Create(m_hWnd, RC(x + kLabelWidth + 20, y, x + ctrlW, y + kCtrlHeight + 4),
                           nullptr, WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | WS_EX_CLIENTEDGE);
    sourcesExclude_.SetFont(hFont);
    sourcesCtrls_.push_back(sourcesExclude_);
    y += 38;

    label(L"External Directories:");
    sourcesExternal_.Create(m_hWnd, RC(x + kLabelWidth + 20, y, x + ctrlW, y + kCtrlHeight + 4),
                            nullptr, WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | WS_EX_CLIENTEDGE);
    sourcesExternal_.SetFont(hFont);
    sourcesCtrls_.push_back(sourcesExternal_);

    // Add help text
    y += 50;
    HWND desc = CreateWindow(L"STATIC",
                             L"Tip: Separate multiple paths with commas (,)",
                             WS_CHILD | SS_LEFT,
                             x, y, 540, 20,
                             m_hWnd, nullptr, nullptr, nullptr);
    HFONT smallFont = CreateFont(-11, 0, 0, 0, FW_NORMAL, TRUE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessage(desc, WM_SETFONT, (WPARAM)smallFont, TRUE);
    sourcesCtrls_.push_back(desc);
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
    currentPage_ = index;
}

void MainWindow::LoadConfigToUI() {
    auto& proj = config_.Project();
    auto& comp = config_.Compiler();
    auto& link = config_.Linker();
    auto& srcs = config_.Sources();

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
}

void MainWindow::SaveUIToConfig() {
    auto& proj = config_.Project();
    auto& comp = config_.Compiler();
    auto& link = config_.Linker();
    auto& srcs = config_.Sources();

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

    config_.SetModified();
}

void MainWindow::OnSize(UINT, CSize size) {
    if (!tabCtrl_.IsWindow()) return;

    int w = size.cx;
    int h = size.cy;
    tabCtrl_.MoveWindow(kMargin, kMargin, w - 2 * kMargin, h - 60);
    saveBtn_.MoveWindow(w - 195, h - 45, 80, 30);
    reloadBtn_.MoveWindow(w - 105, h - 45, 80, 30);
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

void MainWindow::OnReload(UINT, int, HWND) {
    config_.Load(configPath_);
    LoadConfigToUI();
}

LRESULT MainWindow::OnTabChange(LPNMHDR) {
    ShowPage(tabCtrl_.GetCurSel());
    return 0;
}

} // namespace vcbuild::gui
