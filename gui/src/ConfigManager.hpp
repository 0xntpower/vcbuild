#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <filesystem>

namespace vcbuild::gui {

struct ProjectConfig {
    std::wstring name;
    std::wstring type = L"exe";
    std::wstring architecture = L"x64";
    std::wstring outputDir = L"build";
};

struct CompilerConfig {
    std::wstring standard = L"c++20";
    std::wstring runtime = L"dynamic";
    int warningLevel = 4;
    bool warningsAsErrors = false;
    std::vector<std::wstring> defines;
    std::vector<std::wstring> disabledWarnings;
    bool exceptions = true;
    bool permissive = false;
    bool parallel = true;
    bool bufferChecks = true;
    bool cfGuard = false;
};

struct LinkerConfig {
    std::vector<std::wstring> libraries;
    std::vector<std::wstring> libraryPaths;
    std::wstring subsystem = L"console";
    bool aslr = true;
    bool dep = true;
    bool lto = false;
};

struct SourcesConfig {
    std::vector<std::wstring> includeDirs = {L"src", L"include"};
    std::vector<std::wstring> sourceDirs = {L"src"};
    std::vector<std::wstring> excludePatterns;
    std::vector<std::wstring> externalDirs;
};

class ConfigManager {
public:
    bool Load(const std::filesystem::path& path);
    bool Save(const std::filesystem::path& path);
    bool IsModified() const { return modified_; }
    void SetModified(bool val = true) { modified_ = val; }

    ProjectConfig& Project() { return project_; }
    CompilerConfig& Compiler() { return compiler_; }
    LinkerConfig& Linker() { return linker_; }
    SourcesConfig& Sources() { return sources_; }

private:
    ProjectConfig project_;
    CompilerConfig compiler_;
    LinkerConfig linker_;
    SourcesConfig sources_;
    bool modified_ = false;

    std::wstring EscapeJson(const std::wstring& s);
    std::wstring VectorToJson(const std::vector<std::wstring>& vec);
};

inline std::wstring ConfigManager::EscapeJson(const std::wstring& s) {
    std::wstring out;
    for (wchar_t c : s) {
        switch (c) {
            case L'"': out += L"\\\""; break;
            case L'\\': out += L"\\\\"; break;
            case L'\n': out += L"\\n"; break;
            case L'\t': out += L"\\t"; break;
            default: out += c;
        }
    }
    return out;
}

inline std::wstring ConfigManager::VectorToJson(const std::vector<std::wstring>& vec) {
    if (vec.empty()) return L"[]";
    
    std::wstring out = L"[";
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) out += L", ";
        out += L"\"" + EscapeJson(vec[i]) + L"\"";
    }
    out += L"]";
    return out;
}

inline bool ConfigManager::Load(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        project_.name = path.parent_path().filename().wstring();
        return true;
    }
    
    std::wifstream file(path);
    if (!file) return false;
    
    project_.name = path.parent_path().filename().wstring();
    modified_ = false;
    return true;
}

inline bool ConfigManager::Save(const std::filesystem::path& path) {
    std::wofstream file(path);
    if (!file) return false;
    
    file << L"{\n";
    
    file << L"  \"project\": {\n";
    file << L"    \"name\": \"" << EscapeJson(project_.name) << L"\",\n";
    file << L"    \"type\": \"" << project_.type << L"\",\n";
    file << L"    \"output_dir\": \"" << EscapeJson(project_.outputDir) << L"\",\n";
    file << L"    \"architecture\": \"" << project_.architecture << L"\"\n";
    file << L"  },\n";
    
    file << L"  \"compiler\": {\n";
    file << L"    \"standard\": \"" << compiler_.standard << L"\",\n";
    file << L"    \"runtime\": \"" << compiler_.runtime << L"\",\n";
    file << L"    \"defines\": " << VectorToJson(compiler_.defines) << L",\n";
    file << L"    \"exceptions\": " << (compiler_.exceptions ? L"true" : L"false") << L",\n";
    file << L"    \"parallel\": " << (compiler_.parallel ? L"true" : L"false") << L",\n";
    file << L"    \"warnings\": {\n";
    file << L"      \"level\": " << compiler_.warningLevel << L",\n";
    file << L"      \"as_errors\": " << (compiler_.warningsAsErrors ? L"true" : L"false") << L",\n";
    file << L"      \"disabled\": " << VectorToJson(compiler_.disabledWarnings) << L"\n";
    file << L"    },\n";
    file << L"    \"conformance\": {\n";
    file << L"      \"permissive\": " << (compiler_.permissive ? L"true" : L"false") << L"\n";
    file << L"    },\n";
    file << L"    \"security\": {\n";
    file << L"      \"buffer_checks\": " << (compiler_.bufferChecks ? L"true" : L"false") << L",\n";
    file << L"      \"control_flow_guard\": " << (compiler_.cfGuard ? L"true" : L"false") << L"\n";
    file << L"    }\n";
    file << L"  },\n";
    
    file << L"  \"linker\": {\n";
    file << L"    \"libraries\": " << VectorToJson(linker_.libraries) << L",\n";
    file << L"    \"subsystem\": \"" << linker_.subsystem << L"\",\n";
    file << L"    \"security\": {\n";
    file << L"      \"aslr\": " << (linker_.aslr ? L"true" : L"false") << L",\n";
    file << L"      \"dep\": " << (linker_.dep ? L"true" : L"false") << L"\n";
    file << L"    },\n";
    file << L"    \"lto\": \"" << (linker_.lto ? L"on" : L"auto") << L"\"\n";
    file << L"  },\n";
    
    file << L"  \"sources\": {\n";
    file << L"    \"include_dirs\": " << VectorToJson(sources_.includeDirs) << L",\n";
    file << L"    \"source_dirs\": " << VectorToJson(sources_.sourceDirs) << L",\n";
    file << L"    \"exclude_patterns\": " << VectorToJson(sources_.excludePatterns) << L",\n";
    file << L"    \"external_dirs\": " << VectorToJson(sources_.externalDirs) << L"\n";
    file << L"  }\n";
    
    file << L"}\n";
    
    modified_ = false;
    return true;
}

} // namespace vcbuild::gui
