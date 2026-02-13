#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <map>

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
    bool rtti = true;
    std::wstring floatingPoint = L"precise";
    std::wstring callingConvention = L"cdecl";
    std::wstring charSet = L"unicode";
    bool functionLevelLinking = true;
    bool stringPooling = true;
};

struct LinkerConfig {
    std::vector<std::wstring> libraries;
    std::vector<std::wstring> libraryPaths;
    std::wstring subsystem = L"console";
    bool aslr = true;
    bool dep = true;
    bool lto = false;
    bool cfgLinker = false;
    std::wstring entryPoint;
    std::wstring defFile;
    int stackSize = 0;
    int heapSize = 0;
    bool generateMap = false;
    bool generateDebugInfo = true;
};

struct SourcesConfig {
    std::vector<std::wstring> includeDirs = {L"src", L"include"};
    std::vector<std::wstring> sourceDirs = {L"src"};
    std::vector<std::wstring> excludePatterns;
    std::vector<std::wstring> externalDirs;
};

struct ResourcesConfig {
    bool enabled = false;
    std::vector<std::wstring> files;
};

struct DriverConfig {
    bool enabled = false;
    std::wstring type = L"wdm";
    std::wstring entryPoint = L"DriverEntry";
    std::wstring targetOs = L"win10";
    bool minifilter = false;
};

struct PchConfig {
    bool enabled = false;
    std::wstring header;
    std::wstring source;
};

// Minimal JSON parser for vcbuild.json (no external dependencies)
class JsonParser {
public:
    static std::map<std::wstring, std::wstring> ParseFlat(const std::wstring& json) {
        std::map<std::wstring, std::wstring> result;
        ParseObject(json, L"", result, 0);
        return result;
    }

private:
    static size_t SkipWhitespace(const std::wstring& s, size_t pos) {
        while (pos < s.size() && (s[pos] == L' ' || s[pos] == L'\t' ||
               s[pos] == L'\n' || s[pos] == L'\r'))
            ++pos;
        return pos;
    }

    static std::wstring ParseString(const std::wstring& s, size_t& pos) {
        if (pos >= s.size() || s[pos] != L'"') return L"";
        ++pos;
        std::wstring result;
        while (pos < s.size() && s[pos] != L'"') {
            if (s[pos] == L'\\' && pos + 1 < s.size()) {
                ++pos;
                switch (s[pos]) {
                    case L'"': result += L'"'; break;
                    case L'\\': result += L'\\'; break;
                    case L'n': result += L'\n'; break;
                    case L't': result += L'\t'; break;
                    default: result += s[pos]; break;
                }
            } else {
                result += s[pos];
            }
            ++pos;
        }
        if (pos < s.size()) ++pos; // skip closing quote
        return result;
    }

    static std::wstring ParseValue(const std::wstring& s, size_t& pos) {
        pos = SkipWhitespace(s, pos);
        if (pos >= s.size()) return L"";

        if (s[pos] == L'"') {
            return ParseString(s, pos);
        }

        // Parse non-string value (number, bool, null)
        size_t start = pos;
        while (pos < s.size() && s[pos] != L',' && s[pos] != L'}' &&
               s[pos] != L']' && s[pos] != L'\n' && s[pos] != L'\r' &&
               s[pos] != L' ' && s[pos] != L'\t') {
            ++pos;
        }
        return s.substr(start, pos - start);
    }

    static void ParseArray(const std::wstring& s, const std::wstring& prefix,
                          std::map<std::wstring, std::wstring>& out, size_t& pos) {
        ++pos; // skip [
        pos = SkipWhitespace(s, pos);
        std::wstring items;
        while (pos < s.size() && s[pos] != L']') {
            pos = SkipWhitespace(s, pos);
            if (s[pos] == L'{') {
                // Skip nested objects in arrays
                int depth = 0;
                while (pos < s.size()) {
                    if (s[pos] == L'{') ++depth;
                    else if (s[pos] == L'}') { --depth; if (depth == 0) { ++pos; break; } }
                    ++pos;
                }
            } else {
                std::wstring val = ParseValue(s, pos);
                if (!val.empty()) {
                    if (!items.empty()) items += L",";
                    items += val;
                }
            }
            pos = SkipWhitespace(s, pos);
            if (pos < s.size() && s[pos] == L',') ++pos;
        }
        if (pos < s.size()) ++pos; // skip ]
        out[prefix] = items;
    }

    static void ParseObject(const std::wstring& s, const std::wstring& prefix,
                           std::map<std::wstring, std::wstring>& out, size_t pos) {
        pos = SkipWhitespace(s, pos);
        if (pos >= s.size() || s[pos] != L'{') return;
        ++pos;

        ParseObjectInner(s, prefix, out, pos);
    }

    static void ParseObjectInner(const std::wstring& s, const std::wstring& prefix,
                                 std::map<std::wstring, std::wstring>& out, size_t& pos) {
        while (pos < s.size() && s[pos] != L'}') {
            pos = SkipWhitespace(s, pos);
            if (pos >= s.size() || s[pos] == L'}') break;

            // Skip $comment and other special keys
            std::wstring key = ParseString(s, pos);
            pos = SkipWhitespace(s, pos);
            if (pos < s.size() && s[pos] == L':') ++pos;
            pos = SkipWhitespace(s, pos);

            std::wstring fullKey = prefix.empty() ? key : prefix + L"." + key;

            if (pos < s.size() && s[pos] == L'{') {
                ++pos;
                ParseObjectInner(s, fullKey, out, pos);
                if (pos < s.size() && s[pos] == L'}') ++pos;
            } else if (pos < s.size() && s[pos] == L'[') {
                ParseArray(s, fullKey, out, pos);
            } else {
                std::wstring val = ParseValue(s, pos);
                out[fullKey] = val;
            }

            pos = SkipWhitespace(s, pos);
            if (pos < s.size() && s[pos] == L',') ++pos;
        }
    }
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
    ResourcesConfig& Resources() { return resources_; }
    DriverConfig& Driver() { return driver_; }
    PchConfig& Pch() { return pch_; }

private:
    ProjectConfig project_;
    CompilerConfig compiler_;
    LinkerConfig linker_;
    SourcesConfig sources_;
    ResourcesConfig resources_;
    DriverConfig driver_;
    PchConfig pch_;
    bool modified_ = false;

    std::wstring EscapeJson(const std::wstring& s);
    std::wstring VectorToJson(const std::vector<std::wstring>& vec);
    std::vector<std::wstring> SplitCsv(const std::wstring& csv);
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

inline std::vector<std::wstring> ConfigManager::SplitCsv(const std::wstring& csv) {
    std::vector<std::wstring> result;
    size_t start = 0;
    while (start < csv.length()) {
        size_t comma = csv.find(L',', start);
        if (comma == std::wstring::npos) comma = csv.length();
        std::wstring item = csv.substr(start, comma - start);
        size_t fstart = item.find_first_not_of(L" \t");
        size_t fend = item.find_last_not_of(L" \t");
        if (fstart != std::wstring::npos && fend != std::wstring::npos) {
            result.push_back(item.substr(fstart, fend - fstart + 1));
        }
        start = comma + 1;
    }
    return result;
}

inline bool ConfigManager::Load(const std::filesystem::path& path) {
    project_.name = path.parent_path().filename().wstring();

    if (!std::filesystem::exists(path)) {
        modified_ = false;
        return true;
    }

    std::wifstream file(path);
    if (!file) return false;

    std::wstringstream ss;
    ss << file.rdbuf();
    std::wstring json = ss.str();
    file.close();

    auto data = JsonParser::ParseFlat(json);

    auto get = [&](const std::wstring& key) -> std::wstring {
        auto it = data.find(key);
        return (it != data.end()) ? it->second : L"";
    };

    auto getBool = [&](const std::wstring& key, bool def) -> bool {
        auto it = data.find(key);
        if (it == data.end()) return def;
        return it->second == L"true";
    };

    auto getInt = [&](const std::wstring& key, int def) -> int {
        auto it = data.find(key);
        if (it == data.end()) return def;
        try { return std::stoi(it->second); } catch (...) { return def; }
    };

    auto getVec = [&](const std::wstring& key) -> std::vector<std::wstring> {
        auto it = data.find(key);
        if (it == data.end() || it->second.empty()) return {};
        return SplitCsv(it->second);
    };

    // Project
    std::wstring val = get(L"project.name");
    if (!val.empty()) project_.name = val;
    val = get(L"project.type");
    if (!val.empty()) project_.type = val;
    val = get(L"project.architecture");
    if (!val.empty()) project_.architecture = val;
    val = get(L"project.output_dir");
    if (!val.empty()) project_.outputDir = val;

    // Compiler
    val = get(L"compiler.standard");
    if (!val.empty()) compiler_.standard = val;
    val = get(L"compiler.runtime");
    if (!val.empty()) compiler_.runtime = val;
    compiler_.warningLevel = getInt(L"compiler.warnings.level", 4);
    compiler_.warningsAsErrors = getBool(L"compiler.warnings.as_errors", false);
    compiler_.defines = getVec(L"compiler.defines");
    compiler_.disabledWarnings = getVec(L"compiler.warnings.disabled");
    compiler_.exceptions = getBool(L"compiler.exceptions", true);
    compiler_.permissive = getBool(L"compiler.conformance.permissive", false);
    compiler_.parallel = getBool(L"compiler.parallel", true);
    compiler_.bufferChecks = getBool(L"compiler.security.buffer_checks", true);
    compiler_.cfGuard = getBool(L"compiler.security.control_flow_guard", false);
    compiler_.rtti = getBool(L"compiler.rtti", true);
    val = get(L"compiler.floating_point");
    if (!val.empty()) compiler_.floatingPoint = val;
    val = get(L"compiler.calling_convention");
    if (!val.empty()) compiler_.callingConvention = val;
    val = get(L"compiler.char_set");
    if (!val.empty()) compiler_.charSet = val;
    compiler_.functionLevelLinking = getBool(L"compiler.function_level_linking", true);
    compiler_.stringPooling = getBool(L"compiler.string_pooling", true);

    // Linker
    linker_.libraries = getVec(L"linker.libraries");
    linker_.libraryPaths = getVec(L"linker.library_paths");
    val = get(L"linker.subsystem");
    if (!val.empty()) linker_.subsystem = val;
    linker_.aslr = getBool(L"linker.security.aslr", true);
    linker_.dep = getBool(L"linker.security.dep", true);
    linker_.cfgLinker = getBool(L"linker.security.cfg", false);
    val = get(L"linker.lto");
    linker_.lto = (val == L"on");
    val = get(L"linker.entry_point");
    if (!val.empty()) linker_.entryPoint = val;
    val = get(L"linker.def_file");
    if (!val.empty()) linker_.defFile = val;
    linker_.stackSize = getInt(L"linker.stack_size", 0);
    linker_.heapSize = getInt(L"linker.heap_size", 0);
    linker_.generateMap = getBool(L"linker.generate_map", false);
    linker_.generateDebugInfo = getBool(L"linker.generate_debug_info", true);

    // Sources
    auto inc = getVec(L"sources.include_dirs");
    if (!inc.empty()) sources_.includeDirs = inc;
    auto src = getVec(L"sources.source_dirs");
    if (!src.empty()) sources_.sourceDirs = src;
    sources_.excludePatterns = getVec(L"sources.exclude_patterns");
    sources_.externalDirs = getVec(L"sources.external_dirs");

    // Resources
    resources_.enabled = getBool(L"resources.enabled", false);
    resources_.files = getVec(L"resources.files");

    // Driver
    driver_.enabled = getBool(L"driver.enabled", false);
    val = get(L"driver.type");
    if (!val.empty()) driver_.type = val;
    val = get(L"driver.entry_point");
    if (!val.empty()) driver_.entryPoint = val;
    val = get(L"driver.target_os");
    if (!val.empty()) driver_.targetOs = val;
    driver_.minifilter = getBool(L"driver.minifilter", false);

    // PCH
    pch_.enabled = getBool(L"pch.enabled", false);
    val = get(L"pch.header");
    if (!val.empty()) pch_.header = val;
    val = get(L"pch.source");
    if (!val.empty()) pch_.source = val;

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
    file << L"    \"rtti\": " << (compiler_.rtti ? L"true" : L"false") << L",\n";
    file << L"    \"floating_point\": \"" << compiler_.floatingPoint << L"\",\n";
    file << L"    \"calling_convention\": \"" << compiler_.callingConvention << L"\",\n";
    file << L"    \"char_set\": \"" << compiler_.charSet << L"\",\n";
    file << L"    \"function_level_linking\": " << (compiler_.functionLevelLinking ? L"true" : L"false") << L",\n";
    file << L"    \"string_pooling\": " << (compiler_.stringPooling ? L"true" : L"false") << L",\n";
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
    file << L"    \"library_paths\": " << VectorToJson(linker_.libraryPaths) << L",\n";
    file << L"    \"subsystem\": \"" << linker_.subsystem << L"\",\n";
    if (!linker_.entryPoint.empty())
        file << L"    \"entry_point\": \"" << linker_.entryPoint << L"\",\n";
    if (!linker_.defFile.empty())
        file << L"    \"def_file\": \"" << EscapeJson(linker_.defFile) << L"\",\n";
    if (linker_.stackSize > 0)
        file << L"    \"stack_size\": " << linker_.stackSize << L",\n";
    if (linker_.heapSize > 0)
        file << L"    \"heap_size\": " << linker_.heapSize << L",\n";
    file << L"    \"generate_map\": " << (linker_.generateMap ? L"true" : L"false") << L",\n";
    file << L"    \"generate_debug_info\": " << (linker_.generateDebugInfo ? L"true" : L"false") << L",\n";
    file << L"    \"security\": {\n";
    file << L"      \"aslr\": " << (linker_.aslr ? L"true" : L"false") << L",\n";
    file << L"      \"dep\": " << (linker_.dep ? L"true" : L"false") << L",\n";
    file << L"      \"cfg\": " << (linker_.cfgLinker ? L"true" : L"false") << L"\n";
    file << L"    },\n";
    file << L"    \"lto\": \"" << (linker_.lto ? L"on" : L"auto") << L"\"\n";
    file << L"  },\n";

    file << L"  \"sources\": {\n";
    file << L"    \"include_dirs\": " << VectorToJson(sources_.includeDirs) << L",\n";
    file << L"    \"source_dirs\": " << VectorToJson(sources_.sourceDirs) << L",\n";
    file << L"    \"exclude_patterns\": " << VectorToJson(sources_.excludePatterns) << L",\n";
    file << L"    \"external_dirs\": " << VectorToJson(sources_.externalDirs) << L"\n";
    file << L"  },\n";

    file << L"  \"resources\": {\n";
    file << L"    \"enabled\": " << (resources_.enabled ? L"true" : L"false") << L",\n";
    file << L"    \"files\": " << VectorToJson(resources_.files) << L"\n";
    file << L"  },\n";

    file << L"  \"pch\": {\n";
    file << L"    \"enabled\": " << (pch_.enabled ? L"true" : L"false");
    if (!pch_.header.empty())
        file << L",\n    \"header\": \"" << EscapeJson(pch_.header) << L"\"";
    if (!pch_.source.empty())
        file << L",\n    \"source\": \"" << EscapeJson(pch_.source) << L"\"";
    file << L"\n  },\n";

    file << L"  \"driver\": {\n";
    file << L"    \"enabled\": " << (driver_.enabled ? L"true" : L"false") << L",\n";
    file << L"    \"type\": \"" << driver_.type << L"\",\n";
    file << L"    \"entry_point\": \"" << driver_.entryPoint << L"\",\n";
    file << L"    \"target_os\": \"" << driver_.targetOs << L"\",\n";
    file << L"    \"minifilter\": " << (driver_.minifilter ? L"true" : L"false") << L"\n";
    file << L"  }\n";

    file << L"}\n";

    modified_ = false;
    return true;
}

} // namespace vcbuild::gui
