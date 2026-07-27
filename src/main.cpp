#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <cmath>
#include <sstream>
#include <numeric>
#include <unordered_map>
#include <clocale>
#include <cwchar>
#include <sys/ioctl.h>
#include <unistd.h>

namespace fs = std::filesystem;

// ---------------------------------------------------------
// MODES & CONFIGURATION
// ---------------------------------------------------------
enum class ColorMode { Auto, Always, Never };
enum class IconMode  { Nerd, Unicode, None };
enum class BoxMode   { Utf8, Ascii };

struct Options
{
    ColorMode color_mode = ColorMode::Auto;
    IconMode  icon_mode  = IconMode::Nerd;
    BoxMode   box_mode   = BoxMode::Utf8;
    bool      boxed      = false; // Default: Minimal UI
    std::string target_path = "";
    bool show_help = false;
    bool show_version = false;
};

// ---------------------------------------------------------
// ANSI COLOR PALETTE
// ---------------------------------------------------------
struct Theme
{
    bool enabled = true;

    // Core
    std::string RESET;
    std::string BOLD;
    std::string ITALIC;
    std::string UNDER;
    std::string DIM;
    std::string DIM_MID;

    // Accent palette (vivid 256-colour)
    std::string WHITE;
    std::string LBLUE;    // soft lavender-blue
    std::string BLUE;     // code / docs
    std::string CYAN;     // primary chrome
    std::string MINT;     // green / shell
    std::string YELLOW;   // build / Python / JS
    std::string PEACH;    // orange-warm / video
    std::string ROSE;     // magenta / image
    std::string RED;      // error / archive
    std::string VIOLET;   // special realm

    // Box chrome
    std::string CHROME;
    std::string ACCENT;

    // Badge background-style
    std::string BG_HOME;
    std::string BG_ROOT;
    std::string BG_ARM;

    void init(bool use_color)
    {
        enabled = use_color;
        if (!use_color) return;

        RESET    = "\033[0m";
        BOLD     = "\033[1m";
        ITALIC   = "\033[3m";
        UNDER    = "\033[4m";
        DIM      = "\033[38;5;240m";
        DIM_MID  = "\033[38;5;244m";

        WHITE    = "\033[38;5;255m";
        LBLUE    = "\033[38;5;111m";
        BLUE     = "\033[38;5;75m";
        CYAN     = "\033[38;5;87m";
        MINT     = "\033[38;5;121m";
        YELLOW   = "\033[38;5;221m";
        PEACH    = "\033[38;5;216m";
        ROSE     = "\033[38;5;204m";
        RED      = "\033[38;5;196m";
        VIOLET   = "\033[38;5;135m";

        CHROME   = "\033[38;5;60m";
        ACCENT   = "\033[38;5;87m";

        BG_HOME  = "\033[1;38;5;111m";
        BG_ROOT  = "\033[1;38;5;38m";
        BG_ARM   = "\033[1;38;5;196m";
    }
};

static std::string getColor(const Theme &t, const std::string &color_name)
{
    if (!t.enabled) return "";
    if (color_name == "CYAN") return t.CYAN;
    if (color_name == "ROSE") return t.ROSE;
    if (color_name == "BLUE") return t.BLUE;
    if (color_name == "PEACH") return t.PEACH;
    if (color_name == "YELLOW") return t.YELLOW;
    if (color_name == "MINT") return t.MINT;
    if (color_name == "RED") return t.RED;
    if (color_name == "VIOLET") return t.VIOLET;
    if (color_name == "LBLUE") return t.LBLUE;
    if (color_name == "WHITE") return t.WHITE;
    if (color_name == "DIM_MID") return t.DIM_MID;
    if (color_name == "DIM") return t.DIM;
    return t.WHITE;
}

// ---------------------------------------------------------
// BOX STYLES & PSEUDOGRAPHICS
// ---------------------------------------------------------
struct BoxStyle
{
    std::string tl;
    std::string tr;
    std::string bl;
    std::string br;
    std::string hl;
    std::string vl;
    std::string ml;
    std::string mr;
    std::string branch_last;
    std::string branch_mid;
    std::string bar_filled;
    std::string bar_empty;
    std::string ellipsis;

    static BoxStyle get(BoxMode mode)
    {
        if (mode == BoxMode::Ascii)
        {
            return {
                "+", "+", "+", "+", "-", "|", "+", "+",
                " \\- ", " |- ", "=", "-", "..."
            };
        }
        return {
            "╭", "╮", "╰", "╯", "─", "│", "├", "┤",
            " └─ ", " ├─ ", "━", "─", "…"
        };
    }
};

// ---------------------------------------------------------
// BADGES
// ---------------------------------------------------------
struct Badge
{
    std::string nerd_icon;
    std::string unicode_icon;
    std::string ascii_icon;
    std::string text;
    std::string color_name;

    bool operator==(const Badge &other) const { return text == other.text; }
};

static std::string pill(const Badge &b, const Theme &t, IconMode icon_mode)
{
    std::string icon;
    if (icon_mode == IconMode::Nerd) icon = b.nerd_icon;
    else if (icon_mode == IconMode::Unicode) icon = b.unicode_icon;

    std::string col = getColor(t, b.color_name);

    if (!t.enabled)
    {
        if (icon.empty()) return "[" + b.text + "]";
        return "[" + icon + " " + b.text + "]";
    }

    if (icon.empty())
        return col + " " + b.text + " " + t.RESET;
    return col + " " + icon + " " + b.text + " " + t.RESET;
}

// ---------------------------------------------------------
// FILE TYPES & ASSOCIATIONS DATA STRUCTURE
// ---------------------------------------------------------
struct FileTypeInfo
{
    std::string type;
    std::string nerd_icon;
    std::string unicode_icon;
    std::string ascii_icon;
    std::string color_name;
};

class FileTypeRegistry
{
private:
    std::unordered_map<std::string, FileTypeInfo> ext_map;
    std::unordered_map<std::string, FileTypeInfo> name_map;
    std::unordered_map<std::string, FileTypeInfo> dir_map;
    FileTypeInfo default_file;
    FileTypeInfo default_dir;

public:
    FileTypeRegistry()
    {
        default_file = {"File", "󰈔", "📄", "f", "DIM_MID"};
        default_dir  = {"Directory", "󰉋", "📁", "d", "CYAN"};

        // Exact Filenames & Folders
        name_map["Makefile"]       = {"Build", "󱁤", "🛠️", "[BUILD]", "YELLOW"};
        name_map["CMakeLists.txt"] = {"Build", "󱁤", "🛠️", "[BUILD]", "YELLOW"};
        name_map["Dockerfile"]     = {"Docker", "󰡨", "🐳", "[DOCK]", "BLUE"};
        name_map[".dockerignore"]  = {"Docker", "󰡨", "🐳", "[DOCK]", "BLUE"};
        name_map["LICENSE"]        = {"License", "󰿃", "📜", "[LIC]", "MINT"};
        name_map["LICENSE.md"]     = {"License", "󰿃", "📜", "[LIC]", "MINT"};
        name_map["LICENSE.txt"]    = {"License", "󰿃", "📜", "[LIC]", "MINT"};
        name_map[".gitignore"]     = {"Git", "󰊢", "🌿", "[GIT]", "PEACH"};
        name_map[".gitattributes"] = {"Git", "󰊢", "🌿", "[GIT]", "PEACH"};
        name_map[".github"]        = {"GitHub", "󰊤", "🐙", "[GH]", "VIOLET"};
        name_map[".vscode"]        = {"VSCode", "󰨞", "💙", "[VSC]", "BLUE"};
        name_map["Cargo.toml"]     = {"Rust", "󱘗", "🦀", "[CARGO]", "PEACH"};
        name_map["package.json"]   = {"Node", "󰎙", "📦", "[NPM]", "MINT"};
        name_map["go.mod"]         = {"Go", "󰟓", "🐹", "[GO]", "CYAN"};

        // Extensions
        ext_map[".cs"]    = {"C#", "󰌛", "⚡", "[C#]", "MINT"};
        ext_map[".cpp"]   = {"C/C++", "󰙱", "⚙️", "[C++]", "BLUE"};
        ext_map[".c"]     = {"C/C++", "󰙱", "⚙️", "[C]", "BLUE"};
        ext_map[".h"]     = {"C/C++", "󰙱", "⚙️", "[H]", "BLUE"};
        ext_map[".hpp"]   = {"C/C++", "󰙱", "⚙️", "[HPP]", "BLUE"};
        ext_map[".cc"]    = {"C/C++", "󰙱", "⚙️", "[CC]", "BLUE"};
        ext_map[".cxx"]   = {"C/C++", "󰙱", "⚙️", "[CXX]", "BLUE"};
        ext_map[".py"]    = {"Python", "󰌠", "🐍", "[PY]", "YELLOW"};
        ext_map[".pyw"]   = {"Python", "󰌠", "🐍", "[PY]", "YELLOW"};
        ext_map[".js"]    = {"JavaScript", "󰌞", "📜", "[JS]", "YELLOW"};
        ext_map[".mjs"]   = {"JavaScript", "󰌞", "📜", "[JS]", "YELLOW"};
        ext_map[".cjs"]   = {"JavaScript", "󰌞", "📜", "[JS]", "YELLOW"};
        ext_map[".ts"]    = {"TypeScript", "󰛦", "📜", "[TS]", "BLUE"};
        ext_map[".tsx"]   = {"React", "󰌞", "⚛️", "[TSX]", "CYAN"};
        ext_map[".jsx"]   = {"React", "󰌞", "⚛️", "[JSX]", "CYAN"};
        ext_map[".sh"]    = {"Shell", "󱆃", "💻", "[SH]", "MINT"};
        ext_map[".bash"]  = {"Shell", "󱆃", "💻", "[SH]", "MINT"};
        ext_map[".zsh"]   = {"Shell", "󱆃", "💻", "[SH]", "MINT"};
        ext_map[".gd"]    = {"GDScript", "󰘦", "🎮", "[GD]", "CYAN"};
        ext_map[".rs"]    = {"Rust", "󱘗", "🦀", "[RS]", "PEACH"};
        ext_map[".go"]    = {"Go", "󰟓", "🐹", "[GO]", "CYAN"};
        ext_map[".java"]  = {"Java", "󰬷", "☕", "[JAVA]", "ROSE"};
        ext_map[".jar"]   = {"Java", "󰬷", "☕", "[JAR]", "ROSE"};
        ext_map[".rb"]    = {"Ruby", "󰴭", "💎", "[RB]", "RED"};
        ext_map[".lua"]   = {"Lua", "󰢱", "🌙", "[LUA]", "LBLUE"};
        ext_map[".php"]   = {"PHP", "󰌢", "🐘", "[PHP]", "VIOLET"};
        ext_map[".swift"] = {"Swift", "󰛥", "🕊️", "[SWIFT]", "PEACH"};
        ext_map[".kt"]    = {"Kotlin", "󱈙", "🎯", "[KT]", "VIOLET"};

        // Media & Files
        ext_map[".png"]   = {"Image", "󰋩", "🖼️", "[IMG]", "ROSE"};
        ext_map[".jpg"]   = {"Image", "󰋩", "🖼️", "[IMG]", "ROSE"};
        ext_map[".jpeg"]  = {"Image", "󰋩", "🖼️", "[IMG]", "ROSE"};
        ext_map[".gif"]   = {"Image", "󰋩", "🖼️", "[IMG]", "ROSE"};
        ext_map[".svg"]   = {"Image", "󰋩", "🖼️", "[IMG]", "ROSE"};
        ext_map[".webp"]  = {"Image", "󰋩", "🖼️", "[IMG]", "ROSE"};
        ext_map[".ico"]   = {"Image", "󰋩", "🖼️", "[IMG]", "ROSE"};
        ext_map[".mp4"]   = {"Video", "󰕧", "🎬", "[VID]", "PEACH"};
        ext_map[".mkv"]   = {"Video", "󰕧", "🎬", "[VID]", "PEACH"};
        ext_map[".mov"]   = {"Video", "󰕧", "🎬", "[VID]", "PEACH"};
        ext_map[".webm"]  = {"Video", "󰕧", "🎬", "[VID]", "PEACH"};
        ext_map[".mp3"]   = {"Audio", "󰝚", "🎵", "[AUD]", "YELLOW"};
        ext_map[".wav"]   = {"Audio", "󰝚", "🎵", "[AUD]", "YELLOW"};
        ext_map[".flac"]  = {"Audio", "󰝚", "🎵", "[AUD]", "YELLOW"};
        ext_map[".ogg"]   = {"Audio", "󰝚", "🎵", "[AUD]", "YELLOW"};
        ext_map[".pdf"]   = {"PDF", "󰈦", "📕", "[PDF]", "RED"};
        ext_map[".txt"]   = {"Text", "󰎞", "📄", "[TXT]", "DIM_MID"};
        ext_map[".md"]    = {"Markdown", "󰍔", "📝", "[MD]", "LBLUE"};
        ext_map[".doc"]   = {"Document", "󰈙", "📘", "[DOC]", "BLUE"};
        ext_map[".docx"]  = {"Document", "󰈙", "📘", "[DOC]", "BLUE"};
        ext_map[".zip"]   = {"Archive", "󰀼", "📦", "[ZIP]", "RED"};
        ext_map[".tar"]   = {"Archive", "󰀼", "📦", "[TAR]", "RED"};
        ext_map[".gz"]    = {"Archive", "󰀼", "📦", "[GZ]", "RED"};
        ext_map[".rar"]   = {"Archive", "󰀼", "📦", "[RAR]", "RED"};
        ext_map[".7z"]    = {"Archive", "󰀼", "📦", "[7Z]", "RED"};
        ext_map[".json"]  = {"JSON", "󰘦", "📋", "[JSON]", "YELLOW"};
        ext_map[".toml"]  = {"TOML", "󰅪", "📋", "[TOML]", "PEACH"};
        ext_map[".yaml"]  = {"YAML", "󰈙", "📋", "[YML]", "CYAN"};
        ext_map[".yml"]   = {"YAML", "󰈙", "📋", "[YML]", "CYAN"};
        ext_map[".ini"]   = {"Config", "󰒓", "⚙️", "[CFG]", "DIM_MID"};
        ext_map[".conf"]  = {"Config", "󰒓", "⚙️", "[CFG]", "DIM_MID"};
        ext_map[".blend"] = {"Blender", "󰆧", "🎨", "[BLEND]", "PEACH"};
        ext_map[".tscn"]  = {"Asset", "󰺿", "🎮", "[ASSET]", "CYAN"};
        ext_map[".godot"] = {"Asset", "󰺿", "🎮", "[ASSET]", "CYAN"};
        ext_map[".tres"]  = {"Asset", "󰺿", "🎮", "[ASSET]", "CYAN"};
        ext_map[".lock"]  = {"Lock", "󰌾", "🔒", "[LOCK]", "DIM"};
        ext_map[".html"]  = {"HTML", "󰌝", "🌐", "[HTML]", "PEACH"};
        ext_map[".css"]   = {"CSS", "󰌜", "🎨", "[CSS]", "BLUE"};
        ext_map[".sql"]   = {"SQL", "󰆼", "🗄️", "[SQL]", "PEACH"};

        // Directory Names
        dir_map["Downloads"]    = {"Downloads", "󰇚", "📥", "[DL]", "CYAN"};
        dir_map["Pictures"]     = {"Pictures", "󰋩", "🖼️", "[PIC]", "ROSE"};
        dir_map["Documents"]    = {"Documents", "󰈙", "📁", "[DOC]", "BLUE"};
        dir_map["Videos"]       = {"Videos", "󰕧", "🎬", "[VID]", "PEACH"};
        dir_map["Music"]        = {"Music", "󰝚", "🎵", "[MUS]", "YELLOW"};
        dir_map["Desktop"]      = {"Desktop", "󰧨", "🖥️", "[DESK]", "LBLUE"};
        dir_map["src"]          = {"Source", "󱧼", "📂", "[SRC]", "MINT"};
        dir_map["source"]       = {"Source", "󱧼", "📂", "[SRC]", "MINT"};
        dir_map["lib"]          = {"Library", "󱧼", "📂", "[LIB]", "MINT"};
        dir_map["include"]      = {"Headers", "󱧼", "📂", "[INC]", "MINT"};
        dir_map["build"]        = {"Build", "󰆫", "📦", "[BUILD]", "PEACH"};
        dir_map["dist"]         = {"Build", "󰆫", "📦", "[DIST]", "PEACH"};
        dir_map["out"]          = {"Build", "󰆫", "📦", "[OUT]", "PEACH"};
        dir_map["target"]       = {"Build", "󰆫", "📦", "[TARGET]", "PEACH"};
        dir_map["test"]         = {"Tests", "󰙨", "🧪", "[TEST]", "VIOLET"};
        dir_map["tests"]        = {"Tests", "󰙨", "🧪", "[TEST]", "VIOLET"};
        dir_map["spec"]         = {"Tests", "󰙨", "🧪", "[SPEC]", "VIOLET"};
        dir_map[".git"]         = {"Git", "󰊢", "🌿", "[GIT]", "PEACH"};
        dir_map[".github"]      = {"GitHub", "󰊤", "🐙", "[GH]", "VIOLET"};
        dir_map[".vscode"]      = {"VSCode", "󰨞", "💙", "[VSC]", "BLUE"};
        dir_map["images"]       = {"Image", "󰋩", "🖼️", "[IMG]", "ROSE"};
        dir_map["img"]          = {"Image", "󰋩", "🖼️", "[IMG]", "ROSE"};
        dir_map["node_modules"] = {"Node", "󰎙", "📦", "[NODE]", "MINT"};
        dir_map[".config"]      = {"Config", "󰒓", "⚙️", "[CFG]", "DIM_MID"};
        dir_map["config"]       = {"Config", "󰒓", "⚙️", "[CFG]", "DIM_MID"};
    }

    FileTypeInfo getInfo(const fs::path &p) const
    {
        std::string fn = p.filename().string();
        std::string fn_lower = fn;
        std::transform(fn_lower.begin(), fn_lower.end(), fn_lower.begin(), ::tolower);

        if (fs::is_directory(p))
        {
            auto it = dir_map.find(fn);
            if (it != dir_map.end()) return it->second;

            // Pattern checks for directory names
            if (fn_lower.find("image") != std::string::npos || fn_lower.find("img") != std::string::npos || fn_lower.find("pic") != std::string::npos)
                return {"Image", "󰋩", "🖼️", "[IMG]", "ROSE"};
            if (fn_lower.find("github") != std::string::npos)
                return {"GitHub", "󰊤", "🐙", "[GH]", "VIOLET"};
            if (fn_lower.find("git") != std::string::npos)
                return {"Git", "󰊢", "🌿", "[GIT]", "PEACH"};
            if (fn_lower.find("vscode") != std::string::npos)
                return {"VSCode", "󰨞", "💙", "[VSC]", "BLUE"};
            if (fn_lower.find("build") != std::string::npos || fn_lower.find("dist") != std::string::npos)
                return {"Build", "󰆫", "📦", "[BUILD]", "PEACH"};
            if (fn_lower.find("src") != std::string::npos || fn_lower.find("source") != std::string::npos)
                return {"Source", "󱧼", "📂", "[SRC]", "MINT"};
            if (fn_lower.find("test") != std::string::npos || fn_lower.find("spec") != std::string::npos)
                return {"Tests", "󰙨", "🧪", "[TEST]", "VIOLET"};
            if (fn_lower.find("doc") != std::string::npos)
                return {"Docs", "󰈙", "📘", "[DOC]", "BLUE"};

            return default_dir;
        }

        // Exact filename check
        auto it_name = name_map.find(fn);
        if (it_name != name_map.end()) return it_name->second;

        // Pattern checks for exact filenames
        if (fn_lower.find("github") != std::string::npos)
            return {"GitHub", "󰊤", "🐙", "[GH]", "VIOLET"};
        if (fn_lower.find("image") != std::string::npos || fn_lower.find("img") != std::string::npos || fn_lower.find("icon") != std::string::npos)
            return {"Image", "󰋩", "🖼️", "[IMG]", "ROSE"};
        if (fn_lower.find("docker") != std::string::npos)
            return {"Docker", "󰡨", "🐳", "[DOCK]", "BLUE"};

        // Extension check
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        auto it_ext = ext_map.find(ext);
        if (it_ext != ext_map.end()) return it_ext->second;

        return default_file;
    }
};

static std::string getIconStr(const FileTypeInfo &info, IconMode mode)
{
    if (mode == IconMode::Nerd) return info.nerd_icon;
    if (mode == IconMode::Unicode) return info.unicode_icon;
    return info.ascii_icon;
}

// ---------------------------------------------------------
// PARENT BADGE INHERITANCE ENGINE & PATTERN MATCHERS
// ---------------------------------------------------------

// Collects all inherited badges along parent directory tree from / down to target_path
static std::vector<Badge> getInheritedBadges(const fs::path &target_path)
{
    std::vector<Badge> badges;
    fs::path abs_path = fs::absolute(target_path);
    std::string abs_str = abs_path.string();

    // Linux Root Filesystem Badges
    if (abs_str.starts_with("/home"))
        badges.push_back({"󰋜", "🏠", "[HOME]", "Home", "BG_HOME"});
    else if (abs_str.starts_with("/root"))
        badges.push_back({"󰣇", "⚡", "[ROOT]", "Root", "RED"});
    else if (abs_str.starts_with("/bin") || abs_str.starts_with("/usr/bin"))
        badges.push_back({"󰆍", "⚔️", "[BIN]", "Bin", "PEACH"});
    else if (abs_str.starts_with("/sbin") || abs_str.starts_with("/usr/sbin"))
        badges.push_back({"󰆍", "⚙️", "[SBIN]", "Sbin", "YELLOW"});
    else if (abs_str.starts_with("/etc"))
        badges.push_back({"󰦨", "⚙️", "[ETC]", "Config", "YELLOW"});
    else if (abs_str.starts_with("/dev"))
        badges.push_back({"󰢛", "🔌", "[DEV]", "Hardware", "BLUE"});
    else if (abs_str.starts_with("/var"))
        badges.push_back({"󰈔", "📦", "[VAR]", "Var", "MINT"});
    else if (abs_str.starts_with("/opt"))
        badges.push_back({"󰏖", "📦", "[OPT]", "Opt", "ROSE"});
    else if (abs_str.starts_with("/usr"))
        badges.push_back({"󰉋", "👤", "[USR]", "Usr", "CYAN"});
    else if (abs_str.starts_with("/tmp"))
        badges.push_back({"󰌩", "🗑️", "[TMP]", "Tmp", "DIM_MID"});
    else if (abs_str.starts_with("/boot"))
        badges.push_back({"󰐥", "🚀", "[BOOT]", "Boot", "RED"});
    else if (abs_str.starts_with("/media") || abs_str.starts_with("/mnt"))
        badges.push_back({"󰋊", "💾", "[MNT]", "Storage", "VIOLET"});
    else if (abs_str.starts_with("/sys") || abs_str.starts_with("/proc"))
        badges.push_back({"󰘚", "🧠", "[SYS]", "System", "VIOLET"});

    // Ancestor Directory Markers Traversal
    fs::path cur = abs_path;
    std::vector<fs::path> ancestors;
    while (true)
    {
        ancestors.push_back(cur);
        if (cur == cur.parent_path()) break;
        cur = cur.parent_path();
    }
    std::reverse(ancestors.begin(), ancestors.end());

    for (const auto &p : ancestors)
    {
        std::string fn = p.filename().string();
        std::string fn_lower = fn;
        std::transform(fn_lower.begin(), fn_lower.end(), fn_lower.begin(), ::tolower);

        if (fs::exists(p / ".git"))
            badges.push_back({"󰊢", "🌿", "[GIT]", "Git", "PEACH"});
        if (fs::exists(p / ".github") || fn_lower == ".github" || fn_lower.find("github") != std::string::npos)
            badges.push_back({"󰊤", "🐙", "[GH]", "GitHub", "VIOLET"});
        if (fs::exists(p / ".vscode") || fn_lower == ".vscode")
            badges.push_back({"󰨞", "💙", "[VSC]", "VSCode", "BLUE"});
        if (fs::exists(p / "project.godot"))
            badges.push_back({"󰺿", "🎮", "[GODOT]", "Godot", "CYAN"});
        if (fs::exists(p / "Cargo.toml"))
            badges.push_back({"󱘗", "🦀", "[RUST]", "Rust", "PEACH"});
        if (fs::exists(p / "package.json"))
            badges.push_back({"󰎙", "📦", "[NODE]", "Node", "MINT"});
        if (fs::exists(p / "CMakeLists.txt") || fs::exists(p / "Makefile"))
            badges.push_back({"󱁤", "🛠️", "[BUILD]", "Build", "YELLOW"});
        if (fs::exists(p / "go.mod"))
            badges.push_back({"󰟓", "🐹", "[GO]", "Go", "CYAN"});

        if (fn_lower == "src" || fn_lower == "source")
            badges.push_back({"󱧼", "📂", "[SRC]", "Source", "MINT"});
        else if (fn_lower == "build" || fn_lower == "dist" || fn_lower == "target")
            badges.push_back({"󰆫", "📦", "[BUILD]", "Build", "PEACH"});
        else if (fn_lower == "test" || fn_lower == "tests" || fn_lower == "spec")
            badges.push_back({"󰙨", "🧪", "[TEST]", "Test", "VIOLET"});
        else if (fn_lower == "doc" || fn_lower == "docs")
            badges.push_back({"󰈙", "📘", "[DOC]", "Docs", "BLUE"});
        else if (fn_lower == "images" || fn_lower == "img" || fn_lower == "pictures")
            badges.push_back({"󰋩", "🖼️", "[IMG]", "Image", "ROSE"});
    }

    // Deduplicate
    std::vector<Badge> unique_badges;
    for (const auto &b : badges)
    {
        if (std::find(unique_badges.begin(), unique_badges.end(), b) == unique_badges.end())
            unique_badges.push_back(b);
    }
    return unique_badges;
}

// Collects item-specific badges based on registry + pattern matching
static std::vector<Badge> getItemBadges(const fs::path &item_path, const FileTypeRegistry &registry)
{
    std::vector<Badge> badges;
    std::string fn = item_path.filename().string();
    std::string fn_lower = fn;
    std::transform(fn_lower.begin(), fn_lower.end(), fn_lower.begin(), ::tolower);

    FileTypeInfo info = registry.getInfo(item_path);

    // RULE: For FILES (non-directories), return STRICTLY 1 unique badge corresponding to its extension / file type!
    if (!fs::is_directory(item_path))
    {
        if (!info.type.empty() && info.type != "File")
        {
            badges.push_back({info.nerd_icon, info.unicode_icon, info.ascii_icon, info.type, info.color_name});
        }
        return badges;
    }

    // For DIRECTORIES, collect folder type + pattern matchers
    if (!info.type.empty() && info.type != "Directory")
    {
        badges.push_back({info.nerd_icon, info.unicode_icon, info.ascii_icon, info.type, info.color_name});
    }

    auto contains = [&](const std::string &sub) {
        return fn_lower.find(sub) != std::string::npos;
    };

    if (contains("image") || contains("img") || contains("picture") || contains("photo") || contains("pic") || contains("icon"))
        badges.push_back({"󰋩", "🖼️", "[IMG]", "Image", "ROSE"});
    if (contains("github"))
        badges.push_back({"󰊤", "🐙", "[GH]", "GitHub", "VIOLET"});
    else if (contains("git"))
        badges.push_back({"󰊢", "🌿", "[GIT]", "Git", "PEACH"});
    if (contains("vscode"))
        badges.push_back({"󰨞", "💙", "[VSC]", "VSCode", "BLUE"});
    if (contains("build") || contains("dist") || contains("compile"))
        badges.push_back({"󰆫", "📦", "[BUILD]", "Build", "PEACH"});
    if (contains("test") || contains("spec"))
        badges.push_back({"󰙨", "🧪", "[TEST]", "Test", "VIOLET"});
    if (contains("doc") || contains("readme") || contains("guide"))
        badges.push_back({"󰈙", "📘", "[DOC]", "Docs", "BLUE"});
    if (contains("script") || contains("bash") || contains("sh"))
        badges.push_back({"󱆃", "💻", "[SH]", "Script", "MINT"});
    if (contains("config") || contains("setting") || contains("env"))
        badges.push_back({"󰒓", "⚙️", "[CFG]", "Config", "DIM_MID"});
    if (contains("data") || contains("db") || contains("sql"))
        badges.push_back({"󰆼", "🗄️", "[DB]", "Data", "YELLOW"});
    if (contains("docker") || contains("container"))
        badges.push_back({"󰡨", "🐳", "[DOCK]", "Docker", "BLUE"});

    // Deduplicate
    std::vector<Badge> unique_badges;
    for (const auto &b : badges)
    {
        if (std::find(unique_badges.begin(), unique_badges.end(), b) == unique_badges.end())
            unique_badges.push_back(b);
    }
    return unique_badges;
}

// Filters out badges that are already shown in the common header badges
static std::vector<Badge> getUniqueItemBadges(const std::vector<Badge> &item_badges, const std::vector<Badge> &header_badges)
{
    std::vector<Badge> result;
    for (const auto &b : item_badges)
    {
        if (std::find(header_badges.begin(), header_badges.end(), b) == header_badges.end())
        {
            result.push_back(b);
        }
    }
    return result;
}

// ---------------------------------------------------------
// UTF-8 DISPLAY WIDTH & TRUNCATION UTILITIES
// ---------------------------------------------------------
static std::string stripAnsi(const std::string &str)
{
    std::string result;
    bool in_esc = false;
    for (size_t i = 0; i < str.size(); ++i)
    {
        if (str[i] == '\033')
        {
            in_esc = true;
        }
        else if (in_esc)
        {
            if ((str[i] >= 'A' && str[i] <= 'Z') || (str[i] >= 'a' && str[i] <= 'z'))
                in_esc = false;
        }
        else
        {
            result += str[i];
        }
    }
    return result;
}

static size_t displayWidth(const std::string &str)
{
    std::string clean = stripAnsi(str);
    size_t width = 0;
    const char *ptr = clean.c_str();
    size_t len = clean.size();
    wchar_t wc;

    while (len > 0)
    {
        int bytes = std::mbtowc(&wc, ptr, len);
        if (bytes <= 0)
        {
            ptr++;
            len--;
            width += 1;
            continue;
        }
        int w = wcwidth(wc);
        if (w > 0) width += static_cast<size_t>(w);
        else if (w == 0) { /* combining character */ }
        else width += 1; // non-printable fallback
        ptr += bytes;
        len -= bytes;
    }
    return width;
}

static std::string truncateDisplayWidth(const std::string &str, size_t max_width, const std::string &ellipsis = "…")
{
    size_t total_w = displayWidth(str);
    if (total_w <= max_width) return str;

    size_t ell_w = displayWidth(ellipsis);
    if (max_width <= ell_w) return ellipsis.substr(0, max_width);

    size_t target_w = max_width - ell_w;
    size_t cur_w = 0;
    std::string result;

    const char *ptr = str.c_str();
    size_t len = str.size();
    wchar_t wc;

    while (len > 0)
    {
        if (*ptr == '\033')
        {
            size_t esc_len = 0;
            if (len >= 2 && ptr[1] == '[')
            {
                esc_len = 2;
                while (esc_len < len && !((ptr[esc_len] >= 'A' && ptr[esc_len] <= 'Z') || (ptr[esc_len] >= 'a' && ptr[esc_len] <= 'z')))
                {
                    esc_len++;
                }
                if (esc_len < len) esc_len++;
            }
            else
            {
                esc_len = 1;
            }
            result.append(ptr, esc_len);
            ptr += esc_len;
            len -= esc_len;
            continue;
        }

        int bytes = std::mbtowc(&wc, ptr, len);
        if (bytes <= 0)
        {
            if (cur_w + 1 > target_w) break;
            result.append(ptr, 1);
            cur_w += 1;
            ptr++;
            len--;
            continue;
        }
        int w = wcwidth(wc);
        size_t char_w = (w > 0) ? static_cast<size_t>(w) : (w == 0 ? 0 : 1);
        if (cur_w + char_w > target_w) break;
        result.append(ptr, bytes);
        cur_w += char_w;
        ptr += bytes;
        len -= bytes;
    }

    result += ellipsis;
    return result;
}

// ---------------------------------------------------------
// SYSTEM UTILITIES & GIT INTEGRATION
// ---------------------------------------------------------
static int termWidth()
{
    struct winsize w{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0)
        return static_cast<int>(w.ws_col);
    return 100;
}

static std::string getGitBranch(const fs::path &p)
{
    fs::path cur = p;
    while (true)
    {
        fs::path head_file = cur / ".git" / "HEAD";
        if (fs::exists(head_file))
        {
            std::ifstream ifs(head_file);
            if (ifs.is_open())
            {
                std::string line;
                if (std::getline(ifs, line))
                {
                    while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' '))
                        line.pop_back();

                    if (line.rfind("ref: refs/heads/", 0) == 0)
                        return line.substr(16);
                    else if (line.size() >= 7)
                        return line.substr(0, 7);
                }
            }
            break;
        }
        if (cur == cur.parent_path()) break;
        cur = cur.parent_path();
    }
    return "";
}

static std::string formatSize(uintmax_t size)
{
    if (size < 1024)
        return std::to_string(size) + " B";
    if (size < 1024 * 1024)
    {
        double k = size / 1024.0;
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << k << " KB";
        return oss.str();
    }
    double m = size / (1024.0 * 1024.0);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << m << " MB";
    return oss.str();
}

// 5-segment sleek line size indicator bar (displayWidth = 5 columns)
static std::string sizeBar(uintmax_t size, uintmax_t maxSize, const Theme &t, const BoxStyle &box)
{
    if (maxSize == 0)
    {
        std::string bar;
        for (int i = 0; i < 5; i++) bar += box.bar_empty;
        return t.DIM + bar + t.RESET;
    }
    double ratio = static_cast<double>(size) / static_cast<double>(maxSize);
    int filled = static_cast<int>((ratio * 5.0) + 0.5);
    filled = std::max(0, std::min(5, filled));
    std::string bar;
    for (int i = 0; i < 5; i++)
    {
        bar += (i < filled) ? (t.CYAN + box.bar_filled) : (t.DIM + box.bar_empty);
    }
    bar += t.RESET;
    return bar;
}

static std::string formatTime(const fs::file_time_type &time)
{
    try {
        auto tp = std::chrono::clock_cast<std::chrono::system_clock>(time);
        std::time_t tt = std::chrono::system_clock::to_time_t(tp);
        char buf[32] = {0};
        if (std::strftime(buf, sizeof(buf), "%b %d %H:%M", std::localtime(&tt)))
            return std::string(buf);
    } catch (...) {}
    return "Unknown";
}

static std::pair<size_t, size_t> countEntries(const fs::path &dir)
{
    size_t files = 0, folders = 0;
    try {
        for (const auto &e : fs::directory_iterator(dir))
            e.is_directory() ? folders++ : files++;
    } catch (...) {}
    return {folders, files};
}

static std::string permsToOctal(fs::perms p)
{
    int mode = 0;
    if ((p & fs::perms::owner_read)   != fs::perms::none) mode |= 0400;
    if ((p & fs::perms::owner_write)  != fs::perms::none) mode |= 0200;
    if ((p & fs::perms::owner_exec)   != fs::perms::none) mode |= 0100;
    if ((p & fs::perms::group_read)   != fs::perms::none) mode |= 0040;
    if ((p & fs::perms::group_write)  != fs::perms::none) mode |= 0020;
    if ((p & fs::perms::group_exec)   != fs::perms::none) mode |= 0010;
    if ((p & fs::perms::others_read)  != fs::perms::none) mode |= 0004;
    if ((p & fs::perms::others_write) != fs::perms::none) mode |= 0002;
    if ((p & fs::perms::others_exec)  != fs::perms::none) mode |= 0001;

    std::ostringstream oss;
    oss << std::oct << mode;
    return oss.str();
}

static std::string pStrColored(fs::perms p, fs::perms r, fs::perms w, fs::perms x, const Theme &t)
{
    std::string s;
    s += (p & r) != fs::perms::none ? t.MINT   + "r" + t.RESET : t.DIM + "-" + t.RESET;
    s += (p & w) != fs::perms::none ? t.YELLOW + "w" + t.RESET : t.DIM + "-" + t.RESET;
    s += (p & x) != fs::perms::none ? t.RED    + "x" + t.RESET : t.DIM + "-" + t.RESET;
    return s;
}

static std::string breadcrumb(const fs::path &abs, const Theme &t)
{
    std::string s = abs.string();
    const char *home = std::getenv("HOME");
    if (home && s.rfind(home, 0) == 0)
        s = "~" + s.substr(std::string(home).size());

    std::vector<std::string> parts;
    std::stringstream ss(s);
    std::string seg;
    while (std::getline(ss, seg, '/'))
        if (!seg.empty()) parts.push_back(seg);

    if (parts.empty()) parts.push_back("/");

    std::string out;
    for (size_t i = 0; i < parts.size(); i++)
    {
        bool last = (i + 1 == parts.size());
        if (last)
            out += t.BOLD + t.WHITE + parts[i] + t.RESET;
        else
            out += t.DIM_MID + parts[i] + t.RESET + t.DIM + " / " + t.RESET;
    }
    return out;
}

static std::string hRule(int w, const Theme &t, const BoxStyle &box)
{
    if (w <= 0) return "";
    std::string line = t.CHROME;
    for (int i = 0; i < w; i++) line += box.hl;
    return line + t.RESET;
}

// ---------------------------------------------------------
// CLI ARGUMENT PARSER
// ---------------------------------------------------------
static void printHelp(const char *prog)
{
    std::cout << "powerls - Minimal & Powerful Terminal Directory Lister\n\n"
              << "Usage: " << prog << " [OPTIONS] [PATH]\n\n"
              << "Options:\n"
              << "  -h, --help            Show this help message and exit\n"
              << "  -v, --version         Show program version\n"
              << "  --boxed               Use boxed frame UI layout (classic mode)\n"
              << "  --color[=WHEN]        Control colored output: 'auto', 'always', 'never'\n"
              << "  --no-color            Disable colors (same as --color=never)\n"
              << "  --icons[=MODE]        Control icons: 'nerd', 'unicode', 'none'\n"
              << "  --no-icons            Disable icons (same as --icons=none)\n"
              << "  --unicode             Use standard Unicode emojis for icons\n"
              << "  --ascii               Force ASCII box drawing and plain text\n";
}

static Options parseArgs(int argc, char *argv[])
{
    Options opts;
    bool icon_set_explicitly = false;
    bool box_set_explicitly  = false;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            opts.show_help = true;
            return opts;
        }
        else if (arg == "-v" || arg == "--version")
        {
            opts.show_version = true;
            return opts;
        }
        else if (arg == "--boxed")
        {
            opts.boxed = true;
        }
        else if (arg == "--no-color")
        {
            opts.color_mode = ColorMode::Never;
        }
        else if (arg.rfind("--color", 0) == 0)
        {
            if (arg == "--color" || arg == "--color=auto") opts.color_mode = ColorMode::Auto;
            else if (arg == "--color=always") opts.color_mode = ColorMode::Always;
            else if (arg == "--color=never")  opts.color_mode = ColorMode::Never;
        }
        else if (arg == "--no-icons")
        {
            opts.icon_mode = IconMode::None;
            icon_set_explicitly = true;
        }
        else if (arg == "--unicode")
        {
            opts.icon_mode = IconMode::Unicode;
            icon_set_explicitly = true;
        }
        else if (arg.rfind("--icons", 0) == 0)
        {
            if (arg == "--icons=nerd")        { opts.icon_mode = IconMode::Nerd; icon_set_explicitly = true; }
            else if (arg == "--icons=unicode"){ opts.icon_mode = IconMode::Unicode; icon_set_explicitly = true; }
            else if (arg == "--icons=none")   { opts.icon_mode = IconMode::None; icon_set_explicitly = true; }
        }
        else if (arg == "--ascii")
        {
            opts.box_mode = BoxMode::Ascii;
            box_set_explicitly = true;
            if (!icon_set_explicitly) opts.icon_mode = IconMode::None;
        }
        else if (!arg.empty() && arg[0] != '-')
        {
            opts.target_path = arg;
        }
    }

    // Auto-detect color support
    if (opts.color_mode == ColorMode::Auto)
    {
        bool is_tty = (isatty(STDOUT_FILENO) != 0);
        const char *no_color = std::getenv("NO_COLOR");
        const char *term = std::getenv("TERM");
        bool dumb = (term && std::string(term) == "dumb");

        if (!is_tty || (no_color && std::string(no_color).length() > 0) || dumb)
            opts.color_mode = ColorMode::Never;
        else
            opts.color_mode = ColorMode::Always;
    }

    // Auto-detect TTY / locale support
    const char *loc = std::setlocale(LC_ALL, "");
    bool is_utf8_locale = false;
    if (loc)
    {
        std::string loc_str = loc;
        std::transform(loc_str.begin(), loc_str.end(), loc_str.begin(), ::tolower);
        if (loc_str.find("utf-8") != std::string::npos || loc_str.find("utf8") != std::string::npos)
            is_utf8_locale = true;
    }

    if (!is_utf8_locale && !box_set_explicitly)
    {
        opts.box_mode = BoxMode::Ascii;
    }

    if (!isatty(STDOUT_FILENO))
    {
        if (!icon_set_explicitly) opts.icon_mode = IconMode::None;
        if (!box_set_explicitly)  opts.box_mode  = BoxMode::Ascii;
    }

    return opts;
}

// ---------------------------------------------------------
// MAIN
// ---------------------------------------------------------
int main(int argc, char *argv[])
{
    std::setlocale(LC_ALL, "");

    Options opts = parseArgs(argc, argv);
    if (opts.show_help)
    {
        printHelp(argv[0]);
        return 0;
    }
    if (opts.show_version)
    {
        std::cout << "powerls version 1.1.0\n";
        return 0;
    }

    Theme t;
    t.init(opts.color_mode == ColorMode::Always);
    BoxStyle box = BoxStyle::get(opts.box_mode);
    FileTypeRegistry registry;

    // Determine target path
    fs::path target_path = fs::current_path();
    bool target_specified = !opts.target_path.empty();

    if (target_specified)
    {
        std::string arg_str = opts.target_path;
        if (!arg_str.empty() && arg_str.size() > 1 && arg_str.back() == '/') arg_str.pop_back();
        fs::path p(arg_str);
        if (p.is_relative()) target_path = fs::current_path() / p;
        else target_path = p;
    }

    if (!fs::exists(target_path))
    {
        std::cout << "\n"
                  << t.RED << "  Error " << t.RESET
                  << t.DIM << "Target does not exist: " << t.RESET
                  << t.WHITE << target_path.string() << t.RESET << "\n\n";
        return 1;
    }

    // =========================================================
    // MODE 1: TARGET INSPECTOR (Explicit File Inspection)
    // =========================================================
    if (target_specified && !fs::is_directory(target_path))
    {
        fs::path target = target_path;
        std::string target_name = target.filename().string();
        std::string parent_path = target.parent_path().string();

        std::vector<Badge> inherited_badges = getInheritedBadges(target);
        std::vector<Badge> item_badges      = getItemBadges(target, registry);

        FileTypeInfo file_info = registry.getInfo(target);
        std::string main_icon  = getIconStr(file_info, opts.icon_mode);
        std::string main_color = getColor(t, file_info.color_name);

        std::vector<Badge> all_badges = inherited_badges;
        for (const auto &b : item_badges)
            if (std::find(all_badges.begin(), all_badges.end(), b) == all_badges.end())
                all_badges.push_back(b);

        std::string badge_row;
        for (const auto &b : all_badges)
            badge_row += pill(b, t, opts.icon_mode) + "  ";

        int tw = termWidth();

        fs::perms perms = fs::status(target).permissions();
        std::string u_col = pStrColored(perms, fs::perms::owner_read, fs::perms::owner_write, fs::perms::owner_exec, t);
        std::string g_col = pStrColored(perms, fs::perms::group_read, fs::perms::group_write, fs::perms::group_exec, t);
        std::string o_col = pStrColored(perms, fs::perms::others_read, fs::perms::others_write, fs::perms::others_exec, t);
        std::string mass = "—";
        try { mass = formatSize(fs::file_size(target)); } catch (...) {}
        std::string type_label = file_info.type;

        if (opts.boxed)
        {
            int box_width = std::max(40, tw - 2);
            std::cout << "\n";
            std::cout << t.ACCENT << " " << box.tl << hRule(box_width - 2, t, box) << t.ACCENT << box.tr << t.RESET << "\n";
            std::string icon_str = main_icon.empty() ? "" : (main_color + main_icon + " " + t.RESET);
            std::cout << t.ACCENT << " " << box.vl << "  " << t.RESET
                      << icon_str << t.BOLD << t.WHITE << truncateDisplayWidth(target_name, tw - 25, box.ellipsis) << t.RESET
                      << "   " << badge_row << "\n";
            std::cout << t.ACCENT << " " << box.vl << "  " << t.RESET
                      << t.DIM << truncateDisplayWidth(breadcrumb(fs::absolute(target), t), tw - 10, box.ellipsis) << t.RESET << "\n";
            std::cout << t.ACCENT << " " << box.ml << hRule(box_width - 2, t, box) << t.ACCENT << box.mr << t.RESET << "\n";

            const int LW = 10;
            auto row = [&](const std::string &icol, const std::string &icon, const std::string &label, const std::string &value)
            {
                std::string ic = (opts.icon_mode == IconMode::None || icon.empty()) ? "" : (icon + " ");
                std::cout << t.ACCENT << " " << box.vl << "  " << t.RESET
                          << icol << ic << t.RESET << t.DIM_MID << std::left << std::setw(LW) << label << t.RESET
                          << value << "\n";
            };

            row(t.LBLUE,  (opts.icon_mode == IconMode::Unicode ? "📁" : "󰉋"), "Path",    t.DIM_MID + truncateDisplayWidth(parent_path, tw - 20, box.ellipsis) + t.RESET);
            row(t.CYAN,   (opts.icon_mode == IconMode::Unicode ? "🔒" : "󰌾"), "Perms",   t.WHITE + "u:" + t.RESET + u_col + t.DIM + "  g:" + t.RESET + g_col + t.DIM + "  o:" + t.RESET + o_col);
            row(t.ROSE,   (opts.icon_mode == IconMode::Unicode ? "📦" : "󰋊"), "Size",   t.WHITE + mass + t.RESET);
            row(t.YELLOW, (opts.icon_mode == IconMode::Unicode ? "⚙️" : "󰏖"), "Kind",   t.WHITE + type_label + t.RESET);
            try { auto mod = fs::last_write_time(target); row(t.MINT, (opts.icon_mode == IconMode::Unicode ? "🕒" : "󰞱"), "Modified", t.WHITE + formatTime(mod) + t.RESET); } catch (...) {}
            std::cout << t.ACCENT << " " << box.bl << hRule(box_width - 2, t, box) << t.ACCENT << box.br << t.RESET << "\n\n";
        }
        else
        {
            // MINIMAL DESIGN
            std::cout << "\n";
            std::string icon_str = main_icon.empty() ? "" : (main_color + main_icon + " " + t.RESET);
            std::cout << "  " << icon_str << t.BOLD << t.WHITE << target_name << t.RESET;
            if (!badge_row.empty()) std::cout << "   " << badge_row;
            std::cout << "\n\n";

            const int LW = 11;
            auto row = [&](const std::string &label, const std::string &value)
            {
                std::cout << "  " << t.DIM_MID << std::left << std::setw(LW) << label << t.RESET
                          << value << "\n";
            };

            row("Path",     t.WHITE + parent_path + t.RESET);
            row("Perms",    t.WHITE + "u:" + t.RESET + u_col + t.DIM + "  g:" + t.RESET + g_col + t.DIM + "  o:" + t.RESET + o_col + t.DIM + "  (" + permsToOctal(perms) + ")" + t.RESET);
            row("Size",     t.WHITE + mass + t.RESET);
            row("Kind",     t.WHITE + type_label + t.RESET);
            try {
                auto mod = fs::last_write_time(target);
                row("Modified", t.WHITE + formatTime(mod) + t.RESET);
            } catch (...) {}
            std::cout << "\n";
        }
        return 0;
    }

    // =========================================================
    // MODE 2: DIRECTORY LISTING
    // =========================================================
    fs::path abs_path  = fs::absolute(target_path);

    // Parent Badge Inheritance & Linux Root Badges
    std::vector<Badge> inherited_badges = getInheritedBadges(abs_path);

    std::string dir_name = abs_path.filename().string();
    if (dir_name.empty()) dir_name = "/";

    int tw = termWidth();

    // Gather entries
    std::vector<fs::directory_entry> dirs_list, files_list;
    size_t hidden_count = 0;

    try {
        for (const auto &entry : fs::directory_iterator(target_path))
        {
            std::string fn = entry.path().filename().string();
            if (!fn.empty() && fn[0] == '.') hidden_count++;
            if (entry.is_directory())
                dirs_list.push_back(entry);
            else
                files_list.push_back(entry);
        }
    } catch (...) {}

    auto ci_sort = [](const fs::directory_entry &a, const fs::directory_entry &b) {
        std::string an = a.path().filename().string(), bn = b.path().filename().string();
        std::transform(an.begin(), an.end(), an.begin(), ::tolower);
        std::transform(bn.begin(), bn.end(), bn.begin(), ::tolower);
        return an < bn;
    };
    std::sort(dirs_list.begin(),  dirs_list.end(),  ci_sort);
    std::sort(files_list.begin(), files_list.end(), ci_sort);

    std::vector<fs::directory_entry> entries;
    entries.insert(entries.end(), dirs_list.begin(),  dirs_list.end());
    entries.insert(entries.end(), files_list.begin(), files_list.end());

    uintmax_t maxSize = 0;
    for (const auto &e : files_list)
        try { maxSize = std::max(maxSize, fs::file_size(e)); } catch (...) {}

    // Middle area fixed display width: Exactly 15 columns for BOTH directories and files!
    // Directories: cnt (9) + 6 spaces = 15 cols.
    // Files: size_str (8) + 1 space + sizeBar (5) + 1 space = 15 cols.
    // Therefore, date column lands on the EXACT SAME HORIZONTAL COLUMN across all rows!
    size_t max_avail_name_w = (tw > 50) ? static_cast<size_t>(tw - 35) : 15;
    size_t maxNameLen = 0;
    for (const auto &e : entries)
    {
        std::string fn = e.path().filename().string();
        maxNameLen = std::max(maxNameLen, displayWidth(fn));
    }
    maxNameLen = std::max(maxNameLen, size_t(15));
    maxNameLen = std::min(maxNameLen, max_avail_name_w);

    size_t total   = entries.size();
    size_t index   = 0;
    size_t folders = 0, files = 0;
    uintmax_t totalSize = 0;

    FileTypeInfo dir_type_info = registry.getInfo(abs_path);
    std::string dir_icon = getIconStr(dir_type_info, opts.icon_mode);
    std::string dir_icon_str = dir_icon.empty() ? "" : (t.CYAN + dir_icon + " " + t.RESET);

    std::string badge_row;
    for (const auto &b : inherited_badges)
        badge_row += pill(b, t, opts.icon_mode) + "  ";

    std::string git_branch = getGitBranch(abs_path);
    std::string git_str = git_branch.empty() ? "" : (t.PEACH + (opts.icon_mode == IconMode::Unicode ? "🌿 " : (opts.icon_mode == IconMode::Nerd ? "󰊢 " : "git:")) + git_branch + t.RESET + "  ");

    if (opts.boxed)
    {
        // CLASSIC BOXED UI
        int box_width = std::max(40, tw - 2);
        std::cout << "\n";
        std::cout << t.ACCENT << " " << box.tl << hRule(box_width - 2, t, box) << t.ACCENT << box.tr << t.RESET << "\n";
        std::cout << t.ACCENT << " " << box.vl << "  " << t.RESET
                  << dir_icon_str
                  << t.BOLD << t.WHITE << truncateDisplayWidth(dir_name, tw - 25, box.ellipsis) << t.RESET
                  << "   " << badge_row << "\n";
        std::cout << t.ACCENT << " " << box.vl << "  " << t.RESET
                  << t.DIM << truncateDisplayWidth(breadcrumb(abs_path, t), tw - 10, box.ellipsis) << t.RESET << "\n";
        std::cout << t.ACCENT << " " << box.ml << hRule(box_width - 2, t, box) << t.ACCENT << box.mr << t.RESET << "\n";

        if (!dirs_list.empty())
        {
            std::cout << t.ACCENT << " " << box.vl << t.RESET
                      << "  " << t.DIM << "  Directories" << t.RESET << "\n";
        }

        for (const auto &entry : entries)
        {
            bool isDir  = entry.is_directory();
            bool isLast = (index + 1 == total);
            if (!isDir && index == dirs_list.size() && !dirs_list.empty() && !files_list.empty())
            {
                std::cout << t.ACCENT << " " << box.vl << t.RESET << "\n";
                std::cout << t.ACCENT << " " << box.vl << t.RESET
                          << "  " << t.DIM << "  Files" << t.RESET << "\n";
            }

            std::string branch = isLast ? (t.CHROME + box.branch_last + t.RESET)
                                        : (t.CHROME + box.branch_mid + t.RESET);
            index++;

            FileTypeInfo info = registry.getInfo(entry.path());
            std::string main_icon  = getIconStr(info, opts.icon_mode);
            std::string main_color = getColor(t, info.color_name);

            if (isDir) folders++;
            else       files++;

            std::string raw_name = entry.path().filename().string();
            bool hidden = (!raw_name.empty() && raw_name[0] == '.');

            std::string truncated_name = truncateDisplayWidth(raw_name, maxNameLen, box.ellipsis);
            size_t name_disp_w = displayWidth(truncated_name);
            size_t pad = (name_disp_w < maxNameLen) ? (maxNameLen - name_disp_w) : 0;

            std::string name_col;
            if (isDir) name_col = main_color + t.BOLD + truncated_name + t.RESET;
            else if (hidden) name_col = t.DIM + truncated_name + t.RESET;
            else name_col = t.WHITE + truncated_name + t.RESET;

            std::string icon_prefix = main_icon.empty() ? "" : (main_color + main_icon + " " + t.RESET);

            std::vector<Badge> raw_item_badges = getItemBadges(entry.path(), registry);
            std::vector<Badge> unique_item_badges = getUniqueItemBadges(raw_item_badges, inherited_badges);
            std::string item_badge_str;
            for (const auto &ub : unique_item_badges)
                item_badge_str += "  " + pill(ub, t, opts.icon_mode);

            std::cout << t.ACCENT << " " << box.vl << t.RESET << branch
                      << icon_prefix << name_col << std::string(pad + 2, ' ');

            if (isDir)
            {
                try {
                    auto [sf, ff] = countEntries(entry.path());
                    size_t tot = sf + ff;
                    std::string cnt = (tot > 0) ? std::to_string(tot) + (tot == 1 ? " item" : " items") : "empty";
                    std::cout << t.DIM << std::left << std::setw(9) << cnt << t.RESET;
                } catch (...) { std::cout << std::string(9, ' '); }
                std::cout << "      "; // 6 spaces placeholder -> total 15 cols
            }
            else
            {
                std::string size_str = "—";
                uintmax_t fsize = 0;
                try { fsize = fs::file_size(entry); totalSize += fsize; size_str = formatSize(fsize); } catch (...) {}
                std::cout << t.DIM_MID << std::right << std::setw(8) << size_str << t.RESET
                          << " " << sizeBar(fsize, maxSize, t, box) << " "; // 8 + 1 + 5 + 1 = 15 cols
            }

            try {
                auto mod = fs::last_write_time(entry);
                std::cout << t.DIM << formatTime(mod) << t.RESET;
            } catch (...) {}

            std::cout << item_badge_str;
            std::cout << "\n";
        }

        std::cout << t.ACCENT << " " << box.ml << hRule(box_width - 2, t, box) << t.ACCENT << box.mr << t.RESET << "\n";
        std::cout << t.ACCENT << " " << box.vl << "  " << t.RESET
                  << t.DIM_MID << folders << " folders" << t.RESET << t.DIM << "  ·  " << t.RESET
                  << t.DIM_MID << files << " files" << t.RESET;
        if (totalSize > 0) std::cout << t.DIM << "  ·  " << t.RESET << t.DIM_MID << formatSize(totalSize) << " total" << t.RESET;
        if (hidden_count > 0) std::cout << t.DIM << "  ·  " << t.RESET << t.DIM << hidden_count << " hidden" << t.RESET;
        std::cout << "\n";
        std::cout << t.ACCENT << " " << box.bl << hRule(box_width - 2, t, box) << t.ACCENT << box.br << t.RESET << "\n\n";
    }
    else
    {
        // MODERN MINIMAL UI
        std::cout << "\n";
        std::cout << "  " << dir_icon_str << breadcrumb(abs_path, t);
        if (!git_str.empty()) std::cout << "   " << git_str;
        if (!badge_row.empty()) std::cout << "  " << badge_row;
        std::cout << "\n\n";

        if (!dirs_list.empty())
        {
            std::cout << "  " << t.DIM << "Directories" << t.RESET << "\n";
        }

        for (const auto &entry : entries)
        {
            bool isDir = entry.is_directory();
            if (!isDir && index == dirs_list.size() && !dirs_list.empty() && !files_list.empty())
            {
                std::cout << "\n  " << t.DIM << "Files" << t.RESET << "\n";
            }
            index++;

            FileTypeInfo info = registry.getInfo(entry.path());
            std::string main_icon  = getIconStr(info, opts.icon_mode);
            std::string main_color = getColor(t, info.color_name);

            if (isDir) folders++;
            else       files++;

            std::string raw_name = entry.path().filename().string();
            bool hidden = (!raw_name.empty() && raw_name[0] == '.');

            std::string truncated_name = truncateDisplayWidth(raw_name, maxNameLen, box.ellipsis);
            size_t name_disp_w = displayWidth(truncated_name);
            size_t pad = (name_disp_w < maxNameLen) ? (maxNameLen - name_disp_w) : 0;

            std::string name_col;
            if (isDir) name_col = main_color + t.BOLD + truncated_name + t.RESET;
            else if (hidden) name_col = t.DIM + truncated_name + t.RESET;
            else name_col = t.WHITE + truncated_name + t.RESET;

            std::string icon_prefix = main_icon.empty() ? "" : (main_color + main_icon + " " + t.RESET);

            std::vector<Badge> raw_item_badges = getItemBadges(entry.path(), registry);
            std::vector<Badge> unique_item_badges = getUniqueItemBadges(raw_item_badges, inherited_badges);
            std::string item_badge_str;
            for (const auto &ub : unique_item_badges)
                item_badge_str += "  " + pill(ub, t, opts.icon_mode);

            std::cout << "  " << icon_prefix
                      << name_col << std::string(pad + 2, ' ');

            if (isDir)
            {
                try {
                    auto [sf, ff] = countEntries(entry.path());
                    size_t tot = sf + ff;
                    std::string cnt = (tot > 0) ? std::to_string(tot) + (tot == 1 ? " item" : " items") : "empty";
                    std::cout << t.DIM << std::left << std::setw(9) << cnt << t.RESET;
                } catch (...) { std::cout << std::string(9, ' '); }
                std::cout << "      "; // 6 spaces placeholder -> total 15 cols
            }
            else
            {
                std::string size_str = "—";
                uintmax_t fsize = 0;
                try { fsize = fs::file_size(entry); totalSize += fsize; size_str = formatSize(fsize); } catch (...) {}
                std::cout << t.DIM_MID << std::right << std::setw(8) << size_str << t.RESET
                          << " " << sizeBar(fsize, maxSize, t, box) << " "; // 8 + 1 + 5 + 1 = 15 cols
            }

            try {
                auto mod = fs::last_write_time(entry);
                std::cout << t.DIM << formatTime(mod) << t.RESET;
            } catch (...) {}

            std::cout << item_badge_str;
            std::cout << "\n";
        }

        std::cout << "\n  " << t.DIM
                  << folders << " " << (folders == 1 ? "folder" : "folders") << " · "
                  << files << " " << (files == 1 ? "file" : "files");
        if (totalSize > 0) std::cout << " · " << formatSize(totalSize) << " total";
        if (hidden_count > 0) std::cout << " · " << hidden_count << " hidden";
        std::cout << t.RESET << "\n\n";
    }

    return 0;
}