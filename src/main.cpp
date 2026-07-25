#include <iostream>
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
#include <sys/ioctl.h>
#include <unistd.h>

namespace fs = std::filesystem;

// ---------------------------------------------------------
// ANSI COLOR PALETTE
// ---------------------------------------------------------
struct Theme
{
    // Core
    std::string RESET    = "\033[0m";
    std::string BOLD     = "\033[1m";
    std::string ITALIC   = "\033[3m";
    std::string UNDER    = "\033[4m";
    std::string DIM      = "\033[38;5;240m";
    std::string DIM_MID  = "\033[38;5;244m";

    // Accent palette (vivid 256-colour)
    std::string WHITE    = "\033[38;5;255m";
    std::string LBLUE    = "\033[38;5;111m";   // soft lavender-blue  (path segments)
    std::string BLUE     = "\033[38;5;75m";    // code / docs
    std::string CYAN     = "\033[38;5;87m";    // primary chrome
    std::string MINT     = "\033[38;5;121m";   // green / shell / C#
    std::string YELLOW   = "\033[38;5;221m";   // build / Python / JS
    std::string PEACH    = "\033[38;5;216m";   // orange-warm / video / Blender
    std::string ROSE     = "\033[38;5;204m";   // magenta / image / C# alt
    std::string RED      = "\033[38;5;196m";   // error / archive / exec
    std::string VIOLET   = "\033[38;5;135m";   // special realm

    // Box chrome
    std::string CHROME   = "\033[38;5;60m";    // very dim indigo for tree lines
    std::string ACCENT   = "\033[38;5;87m";    // same as CYAN, alias for header

    // Badge background-style (bold + colour gives "pill" feel in mono terminals)
    std::string BG_HOME  = "\033[1;38;5;111m";
    std::string BG_ROOT  = "\033[1;38;5;38m";
    std::string BG_ARM   = "\033[1;38;5;196m";
};

// ---------------------------------------------------------
// BADGE
// ---------------------------------------------------------
struct Badge
{
    std::string icon;
    std::string text;
    std::string color;

    bool operator==(const Badge &other) const { return text == other.text; }
};

// Render a pill: « icon text »
static std::string pill(const Badge &b, const std::string &RESET)
{
    return b.color + " " + b.icon + " " + b.text + " " + RESET;
}

// ---------------------------------------------------------
// UTILITIES
// ---------------------------------------------------------
static int termWidth()
{
    struct winsize w{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0)
        return static_cast<int>(w.ws_col);
    return 100;
}

static bool hasFileUpwards(fs::path p, const std::string &target_name)
{
    while (true)
    {
        if (fs::exists(p / target_name)) return true;
        if (p == p.parent_path()) break;
        p = p.parent_path();
    }
    return false;
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

// 5-dot size indicator using uniform ● / ○ (each exactly 1 terminal cell wide)
static std::string sizeBar(uintmax_t size, uintmax_t maxSize, const Theme &t)
{
    if (maxSize == 0) return t.DIM + "○○○○○" + t.RESET;
    double ratio = static_cast<double>(size) / static_cast<double>(maxSize);
    int filled = static_cast<int>((ratio * 5.0) + 0.5);  // round without std::round ambiguity
    filled = std::max(0, std::min(5, filled));
    std::string bar;
    for (int i = 0; i < 5; i++)
        bar += (i < filled) ? (t.CYAN + "●") : (t.DIM + "○");
    bar += t.RESET;
    return bar;
}

static std::string formatTime(const fs::file_time_type &time)
{
    auto tp = std::chrono::clock_cast<std::chrono::system_clock>(time);
    std::time_t tt = std::chrono::system_clock::to_time_t(tp);
    char buf[32] = {0};
    if (std::strftime(buf, sizeof(buf), "%b %d %H:%M", std::localtime(&tt)))
        return std::string(buf);
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

static std::string getExt(const fs::path &p)
{
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

static std::string getTypeName(const fs::path &p)
{
    std::string ext = getExt(p);
    if (ext==".png"||ext==".jpg"||ext==".jpeg"||ext==".gif"||ext==".svg"||ext==".webp") return "Image";
    if (ext==".mp4"||ext==".mkv"||ext==".mov"||ext==".webm") return "Video";
    if (ext==".mp3"||ext==".wav"||ext==".flac"||ext==".ogg") return "Audio";
    if (ext==".pdf"||ext==".txt"||ext==".md"||ext==".doc"||ext==".docx") return "Document";
    if (ext==".zip"||ext==".tar"||ext==".gz"||ext==".rar"||ext==".7z") return "Archive";
    if (ext==".json"||ext==".toml"||ext==".yaml"||ext==".yml"||ext==".ini"||ext==".conf") return "Config";
    if (ext==".blend"||ext==".blend1") return "Blender";
    if (ext==".tscn"||ext==".godot"||ext==".tres") return "Asset";
    if (ext==".cpp"||ext==".c"||ext==".h"||ext==".hpp") return "C/C++";
    if (ext==".py") return "Python";
    if (ext==".js"||ext==".ts") return "JavaScript";
    if (ext==".sh"||ext==".bash") return "Shell";
    if (ext==".cs") return "C#";
    if (ext==".rs") return "Rust";
    if (ext==".go") return "Go";
    if (ext==".java") return "Java";
    if (ext==".rb") return "Ruby";
    if (ext==".lua") return "Lua";
    return "File";
}

// Permission helper (colored)
static std::string pStrColored(fs::perms p, fs::perms r, fs::perms w, fs::perms x, const Theme &t)
{
    std::string s;
    s += (p & r) != fs::perms::none ? t.MINT   + "r" + t.RESET : t.DIM + "-" + t.RESET;
    s += (p & w) != fs::perms::none ? t.YELLOW  + "w" + t.RESET : t.DIM + "-" + t.RESET;
    s += (p & x) != fs::perms::none ? t.RED     + "x" + t.RESET : t.DIM + "-" + t.RESET;
    return s;
}

// ---------------------------------------------------------
// BREADCRUMB (home-contracted path)
// ---------------------------------------------------------
static std::string breadcrumb(const fs::path &abs, const Theme &t)
{
    std::string s = abs.string();
    const char *home = std::getenv("HOME");
    if (home && s.rfind(home, 0) == 0)
        s = "~" + s.substr(std::string(home).size());

    // Split on '/'
    std::vector<std::string> parts;
    std::stringstream ss(s);
    std::string seg;
    while (std::getline(ss, seg, '/'))
        if (!seg.empty()) parts.push_back(seg);

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

// ---------------------------------------------------------
// BADGE DETECTORS
// ---------------------------------------------------------
void applyFolderBadges(const fs::path &p, std::vector<Badge> &badges,
                       std::string &icon, std::string &color, const Theme &t)
{
    std::string fn = p.filename().string();
    icon  = "󰉋";
    color = t.CYAN;

    if (fn=="Downloads") { icon="󰇚"; color=t.CYAN;  badges.push_back({icon,"Downloads",color}); }
    else if (fn=="Pictures") { icon="󰋩"; color=t.ROSE;  badges.push_back({icon,"Pictures",color}); }
    else if (fn=="Documents"){ icon="󰈙"; color=t.BLUE;  badges.push_back({icon,"Documents",color}); }
    else if (fn=="Videos")   { icon="󰕧"; color=t.PEACH; badges.push_back({icon,"Videos",color}); }
    else if (fn=="Music")    { icon="󰝚"; color=t.YELLOW; badges.push_back({icon,"Music",color}); }
    else if (fn=="Desktop")  { icon="󰧨"; color=t.LBLUE; badges.push_back({icon,"Desktop",color}); }
    else if (fn=="src"||fn=="source"||fn=="lib"||fn=="include") {
        icon="󱧼"; color=t.MINT;
    }
    else if (fn=="build"||fn=="dist"||fn=="out"||fn=="target") {
        icon="󰆫"; color=t.PEACH;
    }
    else if (fn=="test"||fn=="tests"||fn=="spec") {
        icon="󰙨"; color=t.VIOLET;
    }
    else if (fn==".git") {
        icon="󰊢"; color=t.PEACH;
    }
    else if (fn=="node_modules") {
        icon="󰎙"; color=t.MINT;
    }
    else if (fn==".config"||fn=="config"||fn=="configs"||fn=="settings") {
        icon="󰒓"; color=t.DIM_MID;
    }
}

void applyFileBadges(const fs::path &p, std::vector<Badge> &badges,
                     std::string &icon, std::string &color, const Theme &t)
{
    std::string ext = getExt(p);
    std::string fn  = p.filename().string();
    icon  = "󰈔";
    color = t.DIM_MID;

    if (ext==".cs")  { icon="󰌛"; color=t.MINT;   badges.push_back({icon,"C#",color}); }
    else if (ext==".cpp"||ext==".c"||ext==".h"||ext==".hpp")
                     { icon="󰙱"; color=t.BLUE;   badges.push_back({icon,"C/C++",color}); }
    else if (ext==".py")
                     { icon="󰌠"; color=t.YELLOW; badges.push_back({icon,"Python",color}); }
    else if (ext==".js")
                     { icon="󰌞"; color=t.YELLOW; badges.push_back({icon,"JS",color}); }
    else if (ext==".ts")
                     { icon=""; color=t.BLUE;   badges.push_back({icon,"TS",color}); }
    else if (ext==".sh"||ext==".bash")
                     { icon="󱆃"; color=t.MINT;   badges.push_back({icon,"Shell",color}); }
    else if (ext==".gd")
                     { icon="󰘦"; color=t.CYAN;   badges.push_back({icon,"GDScript",color}); }
    else if (ext==".rs")
                     { icon="󱘗"; color=t.PEACH;  badges.push_back({icon,"Rust",color}); }
    else if (ext==".go")
                     { icon="󰟓"; color=t.CYAN;   badges.push_back({icon,"Go",color}); }
    else if (ext==".java")
                     { icon="󰬷"; color=t.ROSE;   badges.push_back({icon,"Java",color}); }
    else if (ext==".rb")
                     { icon="󰴭"; color=t.RED;    badges.push_back({icon,"Ruby",color}); }
    else if (ext==".lua")
                     { icon="󰢱"; color=t.LBLUE;  badges.push_back({icon,"Lua",color}); }
    else if (ext==".png"||ext==".jpg"||ext==".jpeg"||ext==".gif"||ext==".svg"||ext==".webp")
                     { icon="󰋩"; color=t.ROSE;   badges.push_back({icon,"Image",color}); }
    else if (ext==".mp4"||ext==".mkv"||ext==".mov"||ext==".webm")
                     { icon="󰕧"; color=t.PEACH;  badges.push_back({icon,"Video",color}); }
    else if (ext==".mp3"||ext==".wav"||ext==".flac"||ext==".ogg")
                     { icon="󰝚"; color=t.YELLOW; badges.push_back({icon,"Audio",color}); }
    else if (ext==".pdf")
                     { icon="󰈦"; color=t.RED;    badges.push_back({icon,"PDF",color}); }
    else if (ext==".txt")
                     { icon="󰎞"; color=t.DIM_MID; badges.push_back({icon,"Text",color}); }
    else if (ext==".md")
                     { icon="󰍔"; color=t.LBLUE;  badges.push_back({icon,"Markdown",color}); }
    else if (ext==".doc"||ext==".docx")
                     { icon="󰈙"; color=t.BLUE;   badges.push_back({icon,"Document",color}); }
    else if (ext==".zip"||ext==".tar"||ext==".gz"||ext==".rar"||ext==".7z")
                     { icon="󰀼"; color=t.RED;    badges.push_back({icon,"Archive",color}); }
    else if (ext==".json")
                     { icon="󰘦"; color=t.YELLOW; badges.push_back({icon,"JSON",color}); }
    else if (ext==".toml")
                     { icon=""; color=t.PEACH;  badges.push_back({icon,"TOML",color}); }
    else if (ext==".yaml"||ext==".yml")
                     { icon="󰈙"; color=t.CYAN;   badges.push_back({icon,"YAML",color}); }
    else if (ext==".ini"||ext==".conf")
                     { icon="󰒓"; color=t.DIM_MID; badges.push_back({icon,"Config",color}); }
    else if (ext==".blend"||ext==".blend1")
                     { icon="󰆧"; color=t.PEACH;  badges.push_back({icon,"Blender",color}); }
    else if (ext==".tscn"||ext==".godot"||ext==".tres")
                     { icon="󰺿"; color=t.CYAN;   badges.push_back({icon,"Asset",color}); }
    else if (fn=="Makefile"||ext==".ninja"||ext==".cmake"||fn=="CMakeLists.txt")
                     { icon="󱁤"; color=t.YELLOW; badges.push_back({icon,"Build",color}); }
    else if (fn=="Dockerfile"||fn==".dockerignore")
                     { icon="󰡨"; color=t.BLUE;   badges.push_back({icon,"Docker",color}); }
    else if (fn=="LICENSE"||fn=="LICENSE.md"||fn=="LICENSE.txt")
                     { icon="󰿃"; color=t.MINT;   badges.push_back({icon,"License",color}); }
    else if (fn==".gitignore"||fn==".gitattributes")
                     { icon="󰊢"; color=t.PEACH;  badges.push_back({icon,"Git",color}); }
    else if (ext==".lock")
                     { icon="󰌾"; color=t.DIM;    badges.push_back({icon,"Lock",color}); }
}

// ---------------------------------------------------------
// REALM BADGES (environment)
// ---------------------------------------------------------
static void addRealmBadges(const std::string &abs_str, std::vector<Badge> &badges, const Theme &t)
{
    if (abs_str.starts_with("/home"))
        badges.push_back({"󰋜", "Home", t.BG_HOME});
    else if (abs_str.starts_with("/bin")||abs_str.starts_with("/sbin")||abs_str.starts_with("/usr/bin"))
        badges.push_back({"󰆍", "Armory", t.BG_ARM});
    else if (abs_str.starts_with("/etc"))
        badges.push_back({"󰦨", "Config", t.YELLOW});
    else if (abs_str.starts_with("/dev"))
        badges.push_back({"󰢛", "Hardware", t.BLUE});
    else
        badges.push_back({"󰣇", "Root", t.BG_ROOT});
}

// ---------------------------------------------------------
// HORIZONTAL RULE (dim)
// ---------------------------------------------------------
static std::string hRule(int w, const Theme &t)
{
    std::string line = t.CHROME;
    for (int i = 0; i < w - 4; i++) line += "─";
    return line + t.RESET;
}

// ---------------------------------------------------------
// MAIN
// ---------------------------------------------------------
int main(int argc, char *argv[])
{
    Theme t;

    // =========================================================
    // MODE 1: TARGET INSPECTOR
    // =========================================================
    if (argc == 2)
    {
        std::string arg_str = argv[1];
        if (!arg_str.empty() && arg_str.back() == '/') arg_str.pop_back();
        fs::path target = fs::current_path() / arg_str;

        if (!fs::exists(target))
        {
            std::cout << "\n"
                      << t.RED << "  󰩹  Error " << t.RESET
                      << t.DIM << "Target does not exist: " << t.RESET
                      << t.WHITE << arg_str << t.RESET << "\n\n";
            return 1;
        }

        std::string target_name = target.filename().string();
        std::string parent_path = target.parent_path().string();
        std::string abs_str     = fs::absolute(target).string();
        fs::path    scan_path   = fs::is_directory(target) ? target : target.parent_path();

        // Collect badges
        std::vector<Badge> raw_badges;
        addRealmBadges(abs_str, raw_badges, t);
        if (hasFileUpwards(scan_path, ".git"))
            raw_badges.push_back({"󰊢", "Git", t.PEACH});
        if (hasFileUpwards(scan_path, "project.godot"))
            raw_badges.push_back({"󰺿", "Godot", t.CYAN});

        std::string main_icon, main_color;
        if (fs::is_directory(target))
            applyFolderBadges(target, raw_badges, main_icon, main_color, t);
        else
            applyFileBadges(target, raw_badges, main_icon, main_color, t);

        // Deduplicate + render pills
        std::vector<Badge> unique_badges;
        std::string badge_row;
        for (const auto &b : raw_badges)
            if (std::find(unique_badges.begin(), unique_badges.end(), b) == unique_badges.end())
            {
                unique_badges.push_back(b);
                badge_row += pill(b, t.RESET) + "  ";
            }

        int tw = termWidth();

        // ── Header ──────────────────────────────────────────────
        std::cout << "\n";
        std::cout << t.ACCENT << " ╭" << hRule(tw - 2, t) << t.ACCENT << "╮" << t.RESET << "\n";
        std::cout << t.ACCENT << " │  " << t.RESET
                  << main_color << main_icon << " " << t.RESET
                  << t.BOLD << t.WHITE << target_name << t.RESET
                  << "   " << badge_row << "\n";
        std::cout << t.ACCENT << " │  " << t.RESET
                  << t.DIM << breadcrumb(fs::absolute(target), t) << t.RESET << "\n";
        std::cout << t.ACCENT << " ├" << hRule(tw - 2, t) << t.ACCENT << "┤" << t.RESET << "\n";

        // ── Permissions ─────────────────────────────────────────
        fs::perms perms = fs::status(target).permissions();
        std::string u_col = pStrColored(perms,fs::perms::owner_read,fs::perms::owner_write,fs::perms::owner_exec,t);
        std::string g_col = pStrColored(perms,fs::perms::group_read,fs::perms::group_write,fs::perms::group_exec,t);
        std::string o_col = pStrColored(perms,fs::perms::others_read,fs::perms::others_write,fs::perms::others_exec,t);

        const int LW = 10;
        auto row = [&](const std::string &icol, const std::string &icon,
                       const std::string &label, const std::string &value)
        {
            std::cout << t.ACCENT << " │  " << t.RESET
                      << icol << icon << " " << t.RESET
                      << t.DIM_MID << std::left << std::setw(LW) << label << t.RESET
                      << value << "\n";
        };

        row(t.LBLUE,  "󰉋", "Path",    t.DIM_MID + parent_path + t.RESET);
        row(t.CYAN,   "󰌾", "Perms",
            t.WHITE + "u:" + t.RESET + u_col +
            t.DIM + "  " + t.RESET +
            t.WHITE + "g:" + t.RESET + g_col +
            t.DIM + "  " + t.RESET +
            t.WHITE + "o:" + t.RESET + o_col);

        std::string footer_summary;
        if (fs::is_directory(target))
        {
            auto [subfolders, subfiles] = countEntries(target);
            footer_summary = std::to_string(subfolders) + " folders · " + std::to_string(subfiles) + " files";
            row(t.ROSE,   "󰑭", "Contents",
                t.WHITE + std::to_string(subfiles) + " files" + t.RESET +
                t.DIM + "  ·  " + t.RESET +
                t.WHITE + std::to_string(subfolders) + " folders" + t.RESET);
            row(t.YELLOW, "󰏖", "Kind",    t.WHITE + "Directory" + t.RESET);
        }
        else
        {
            std::string mass = "—";
            try { mass = formatSize(fs::file_size(target)); } catch (...) {}
            std::string type_label = getTypeName(target);
            footer_summary = mass + "  ·  " + type_label;
            row(t.ROSE,   "󰋊", "Size",   t.WHITE + mass + t.RESET);
            row(t.YELLOW, "󰏖", "Kind",   t.WHITE + type_label + t.RESET);
        }

        try
        {
            auto mod = fs::last_write_time(target);
            row(t.MINT,   "󰞱", "Modified", t.WHITE + formatTime(mod) + t.RESET);
        }
        catch (...) {}

        // ── Footer ──────────────────────────────────────────────
        std::cout << t.ACCENT << " ╰" << hRule(tw - 2, t) << t.ACCENT << "╯" << t.RESET << "\n";
        std::cout << "   " << t.DIM << footer_summary << t.RESET << "\n\n";
        return 0;
    }

    // =========================================================
    // MODE 2: DIRECTORY LISTING
    // =========================================================
    fs::path target_path = fs::current_path();
    fs::path abs_path    = fs::absolute(target_path);
    std::string path_str = abs_path.string();

    // Env badges
    std::vector<Badge> env_badges;
    addRealmBadges(path_str, env_badges, t);
    if (path_str.find("/Downloads") != std::string::npos) env_badges.push_back({"󰇚","Downloads",t.CYAN});
    if (path_str.find("/Pictures")  != std::string::npos) env_badges.push_back({"󰋩","Pictures",t.ROSE});
    if (path_str.find("/Documents") != std::string::npos) env_badges.push_back({"󰈙","Documents",t.BLUE});
    if (path_str.find("/Videos")    != std::string::npos) env_badges.push_back({"󰕧","Videos",t.PEACH});
    if (path_str.find("/Music")     != std::string::npos) env_badges.push_back({"󰝚","Music",t.YELLOW});
    if (path_str.find("/Desktop")   != std::string::npos) env_badges.push_back({"󰧨","Desktop",t.LBLUE});
    if (hasFileUpwards(abs_path, ".git"))        env_badges.push_back({"󰊢","Git",t.PEACH});
    if (hasFileUpwards(abs_path, "project.godot")) env_badges.push_back({"󰺿","Godot",t.CYAN});

    std::string dir_name = abs_path.filename().string();
    if (dir_name.empty()) dir_name = "/";

    int tw = termWidth();

    // ── Header ────────────────────────────────────────────────
    std::cout << "\n";
    std::cout << t.ACCENT << " ╭" << hRule(tw - 2, t) << t.ACCENT << "╮" << t.RESET << "\n";

    // Title row: icon + dir name + pills
    std::string badge_row;
    for (const auto &b : env_badges)
        badge_row += pill(b, t.RESET) + "  ";

    std::cout << t.ACCENT << " │  " << t.RESET
              << t.CYAN << "󰉋 " << t.RESET
              << t.BOLD << t.WHITE << dir_name << t.RESET
              << "   " << badge_row << "\n";

    // Breadcrumb row
    std::cout << t.ACCENT << " │  " << t.RESET
              << t.DIM << breadcrumb(abs_path, t) << t.RESET << "\n";

    std::cout << t.ACCENT << " ├" << hRule(tw - 2, t) << t.ACCENT << "┤" << t.RESET << "\n";

    // ── Gather entries ────────────────────────────────────────
    std::vector<fs::directory_entry> dirs_list, files_list;
    size_t hidden_count = 0;

    for (const auto &entry : fs::directory_iterator(target_path))
    {
        std::string fn = entry.path().filename().string();
        if (!fn.empty() && fn[0] == '.') hidden_count++;
        if (entry.is_directory())
            dirs_list.push_back(entry);
        else
            files_list.push_back(entry);
    }

    // Sort each group alphabetically, case-insensitive
    auto ci_sort = [](const fs::directory_entry &a, const fs::directory_entry &b) {
        std::string an = a.path().filename().string(), bn = b.path().filename().string();
        std::transform(an.begin(),an.end(),an.begin(),::tolower);
        std::transform(bn.begin(),bn.end(),bn.begin(),::tolower);
        return an < bn;
    };
    std::sort(dirs_list.begin(),  dirs_list.end(),  ci_sort);
    std::sort(files_list.begin(), files_list.end(), ci_sort);

    std::vector<fs::directory_entry> entries;
    entries.insert(entries.end(), dirs_list.begin(),  dirs_list.end());
    entries.insert(entries.end(), files_list.begin(), files_list.end());

    // Pre-compute max file size for size bars
    uintmax_t maxSize = 0;
    for (const auto &e : files_list)
        try { maxSize = std::max(maxSize, fs::file_size(e)); } catch (...) {}

    // Column widths
    size_t maxNameLen = 0;
    for (const auto &e : entries)
        maxNameLen = std::max(maxNameLen, e.path().filename().string().size());
    maxNameLen = std::max(maxNameLen, size_t(20));
    maxNameLen = std::min(maxNameLen, size_t(36));

    size_t total   = entries.size();
    size_t index   = 0;
    size_t folders = 0, files = 0;
    uintmax_t totalSize = 0;

    // ── Section: Directories ─────────────────────────────────
    if (!dirs_list.empty())
    {
        std::cout << t.ACCENT << " │" << t.RESET
                  << "  " << t.DIM << "  Directories" << t.RESET << "\n";
    }

    for (const auto &entry : entries)
    {
        bool isDir  = entry.is_directory();
        bool isLast = (index + 1 == total);
        bool firstFile = (isDir == false && index == dirs_list.size()); // first file entry

        // Print section separator between dirs and files
        if (!isDir && index == dirs_list.size() && !dirs_list.empty() && !files_list.empty())
        {
            std::cout << t.ACCENT << " │" << t.RESET << "\n";
            std::cout << t.ACCENT << " │" << t.RESET
                      << "  " << t.DIM << "  Files" << t.RESET << "\n";
        }

        std::string branch = isLast ? t.CHROME + " └─ " + t.RESET
                                    : t.CHROME + " ├─ " + t.RESET;
        index++;

        std::vector<Badge> item_badges;
        std::string main_icon, main_color;

        if (isDir)
        {
            Badge gitB  = {"󰊢","Git",t.PEACH};
            Badge godotB= {"󰺿","Godot",t.CYAN};
            if (fs::exists(entry.path()/".git") &&
                std::find(env_badges.begin(),env_badges.end(),gitB)==env_badges.end())
                item_badges.push_back(gitB);
            if (fs::exists(entry.path()/"project.godot") &&
                std::find(env_badges.begin(),env_badges.end(),godotB)==env_badges.end())
                item_badges.push_back(godotB);
            applyFolderBadges(entry.path(), item_badges, main_icon, main_color, t);
            folders++;
        }
        else
        {
            applyFileBadges(entry.path(), item_badges, main_icon, main_color, t);
            files++;
        }

        // Only keep special (non-type) badges: Git, Godot — shown in a fixed column
        // item_badges may contain type badge + special badges; we only want specials here.
        // Special badges are those added before applyFolderBadges/applyFileBadges.
        // We detect them by text.
        std::string special_badge_col;
        {
            // fixed width of 6 chars for special badge column (icon + space, or spaces)
            std::string special_icon;
            std::string special_color;
            for (const auto &b : item_badges)
            {
                if (b.text == "Git" || b.text == "Godot")
                {
                    special_icon  = b.icon;
                    special_color = b.color;
                    break;
                }
            }
            if (!special_icon.empty())
                special_badge_col = special_color + special_icon + t.RESET + " ";
            else
                special_badge_col = "  "; // 2 chars: icon (1) + space (1)
        }

        std::string name = entry.path().filename().string();
        bool hidden = (!name.empty() && name[0] == '.');

        // Name column
        std::string name_col;
        if (isDir)
            name_col = main_color + t.BOLD + name + t.RESET;
        else if (hidden)
            name_col = t.DIM + name + t.RESET;
        else
            name_col = t.WHITE + name + t.RESET;

        // Pad name to maxNameLen (visual chars only — no ANSI)
        size_t pad = (name.size() < maxNameLen) ? maxNameLen - name.size() : 0;

        std::cout << t.ACCENT << " │" << t.RESET << branch
                  << main_color << main_icon << " " << t.RESET
                  << name_col << std::string(pad + 2, ' ');

        if (isDir)
        {
            // Fixed-width item count column (8 chars: up to "999 items")
            try {
                auto [sf, ff] = countEntries(entry.path());
                size_t tot = sf + ff;
                std::string cnt = (tot > 0)
                    ? std::to_string(tot) + (tot == 1 ? " item" : " items")
                    : "empty";
                std::cout << t.DIM << std::left << std::setw(9) << cnt << t.RESET;
            } catch (...) {
                std::cout << std::string(9, ' ');
            }
            // 5-space placeholder where size bar would be (keeps timestamp aligned)
            std::cout << "         ";
        }
        else
        {
            // Fixed-width size column (8 chars) + 1 space + 5-dot bar + 1 space
            std::string size_str = "—";
            uintmax_t fsize = 0;
            try {
                fsize = fs::file_size(entry);
                totalSize += fsize;
                size_str = formatSize(fsize);
            } catch (...) {}
            std::cout << t.DIM_MID << std::right << std::setw(8) << size_str << t.RESET
                      << " " << sizeBar(fsize, maxSize, t) << " ";
        }

        // Special badge column (fixed 2 chars: icon+space or two spaces)
        std::cout << special_badge_col;

        // Timestamp — always in the same column now
        try {
            auto mod = fs::last_write_time(entry);
            std::cout << t.DIM << formatTime(mod) << t.RESET;
        } catch (...) {}

        std::cout << "\n";
    }

    // ── Footer ────────────────────────────────────────────────
    std::cout << t.ACCENT << " ├" << hRule(tw - 2, t) << t.ACCENT << "┤" << t.RESET << "\n";

    std::string total_size_str = formatSize(totalSize);

    std::cout << t.ACCENT << " │  " << t.RESET
              << t.CYAN << "󰉋 " << t.RESET
              << t.DIM_MID << folders << " " << (folders == 1 ? "folder" : "folders")
              << t.RESET
              << t.DIM << "  ·  " << t.RESET
              << t.DIM_MID << files << " " << (files == 1 ? "file" : "files")
              << t.RESET;

    if (totalSize > 0)
        std::cout << t.DIM << "  ·  " << t.RESET
                  << t.DIM_MID << total_size_str << " total" << t.RESET;

    if (hidden_count > 0)
        std::cout << t.DIM << "  ·  " << t.RESET
                  << t.DIM << hidden_count << " hidden" << t.RESET;

    std::cout << "\n";
    std::cout << t.ACCENT << " ╰" << hRule(tw - 2, t) << t.ACCENT << "╯" << t.RESET << "\n\n";
    return 0;
}