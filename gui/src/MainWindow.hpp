#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlmisc.h>
#include <atlctrls.h>
#include <atlcrack.h>
#include <atldlgs.h>

#include <string>
#include <filesystem>

#include "resource.h"
#include "ConfigManager.hpp"

namespace vcbuild::gui {

class MainWindow : public CWindowImpl<MainWindow> {
public:
    DECLARE_WND_CLASS(L"VcBuildConfigWindow")

    BEGIN_MSG_MAP(MainWindow)
        MSG_WM_CREATE(OnCreate)
        MSG_WM_SIZE(OnSize)
        MSG_WM_DESTROY(OnDestroy)
        MSG_WM_CLOSE(OnClose)
        COMMAND_ID_HANDLER_EX(IDC_SAVE, OnSave)
        NOTIFY_CODE_HANDLER_EX(TCN_SELCHANGE, OnTabChange)
    END_MSG_MAP()

    bool Initialize(const std::filesystem::path& configPath);

private:
    int OnCreate(LPCREATESTRUCT cs);
    void OnSize(UINT type, CSize size);
    void OnDestroy();
    void OnClose();
    void OnSave(UINT code, int id, HWND hwnd);
    LRESULT OnTabChange(LPNMHDR nmhdr);

    void CreateControls();
    void CreateProjectPage();
    void CreateCompilerPage();
    void CreateLinkerPage();
    void CreateSourcesPage();
    void CreateResourcesPage();
    void CreateDriverPage();
    void ShowPage(int index);
    void LoadConfigToUI();
    void SaveUIToConfig();
    void UpdateTitleBar();

    ConfigManager config_;
    std::filesystem::path configPath_;
    HFONT hFont_ = nullptr;
    HFONT hSmallFont_ = nullptr;

    CTabCtrl tabCtrl_;
    CButton saveBtn_;

    std::vector<HWND> projectCtrls_;
    std::vector<HWND> compilerCtrls_;
    std::vector<HWND> linkerCtrls_;
    std::vector<HWND> sourcesCtrls_;
    std::vector<HWND> resourcesCtrls_;
    std::vector<HWND> driverCtrls_;

    // Project
    CComboBox projectType_;
    CComboBox projectArch_;
    CEdit projectName_;
    CEdit projectOutDir_;

    // Compiler
    CComboBox compilerStd_;
    CComboBox compilerRuntime_;
    CComboBox compilerFp_;
    CComboBox compilerCallConv_;
    CComboBox compilerCharSet_;
    CEdit compilerDefines_;
    CButton compilerExceptions_;
    CButton compilerParallel_;
    CButton compilerBuffer_;
    CButton compilerCfg_;
    CButton compilerRtti_;
    CButton compilerFuncLink_;
    CButton compilerStrPool_;
    CButton compilerWarnErr_;

    // Linker
    CComboBox linkerSubsystem_;
    CEdit linkerLibs_;
    CEdit linkerLibPaths_;
    CEdit linkerEntry_;
    CEdit linkerDefFile_;
    CButton linkerAslr_;
    CButton linkerDep_;
    CButton linkerLto_;
    CButton linkerCfgLink_;
    CButton linkerMap_;
    CButton linkerDebugInfo_;

    // Sources
    CEdit sourcesInclude_;
    CEdit sourcesSource_;
    CEdit sourcesExclude_;
    CEdit sourcesExternal_;

    // Resources
    CButton resourcesEnabled_;
    CEdit resourcesFiles_;

    // Driver
    CButton driverEnabled_;
    CComboBox driverType_;
    CEdit driverEntry_;
    CComboBox driverTargetOs_;
    CButton driverMinifilter_;

    int currentPage_ = 0;
    static constexpr int kMargin = 10;
    static constexpr int kCtrlHeight = 22;
    static constexpr int kLabelWidth = 120;
};

} // namespace vcbuild::gui
