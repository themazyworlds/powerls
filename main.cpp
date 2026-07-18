#include <iostream>
#include <filesystem>
#include <string>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>

namespace fs = std::filesystem;

// ---------------------------------------------------------
// COMPONENT SYSTEM
// ---------------------------------------------------------
struct Badge
{
    std::string icon;
    std::string text;
    std::string color;

    bool operator==(const Badge &other) const
    {
        return text == other.text;
    }
};

bool hasFileUpwards(fs::path p, const std::string &target_name)
{
    while (true)
    {
        if (fs::exists(p / target_name))
            return true;
        if (p == p.parent_path())
            break;
        p = p.parent_path();
    }
    return false;
}

std::string formatSize(uintmax_t size)
{
    if (size < 1024)
        return std::to_string(size) + " B";
    if (size < 1024 * 1024)
        return std::to_string(size / 1024) + " KB";
    return std::to_string(size / (1024 * 1024)) + " MB";
}

std::string formatTime(const fs::file_time_type &time)
{
    auto tp = std::chrono::clock_cast<std::chrono::system_clock>(time);
    std::time_t tt = std::chrono::system_clock::to_time_t(tp);
    char buf[32] = {0};
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", std::localtime(&tt)))
        return std::string(buf);
    return "Unknown";
}

std::pair<size_t, size_t> countEntries(const fs::path &dir)
{
    size_t files = 0;
    size_t folders = 0;
    for (const auto &entry : fs::directory_iterator(dir))
    {
        if (entry.is_directory())
            folders++;
        else
            files++;
    }
    return {folders, files};
}

std::string getExt(const fs::path &p)
{
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

std::string getTypeName(const fs::path &p)
{
    std::string ext = getExt(p);
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" || ext == ".svg" || ext == ".webp")
        return "Picture";
    if (ext == ".mp4" || ext == ".mkv" || ext == ".mov" || ext == ".webm")
        return "Video";
    if (ext == ".mp3" || ext == ".wav" || ext == ".flac" || ext == ".ogg")
        return "Audio";
    if (ext == ".pdf" || ext == ".txt" || ext == ".md" || ext == ".doc" || ext == ".docx")
        return "Document";
    if (ext == ".zip" || ext == ".tar" || ext == ".gz" || ext == ".rar" || ext == ".7z")
        return "Archive";
    if (ext == ".json" || ext == ".toml" || ext == ".yaml" || ext == ".yml" || ext == ".ini" || ext == ".conf")
        return "Config";
    if (ext == ".blend" || ext == ".blend1")
        return "Blender";
    if (ext == ".tscn" || ext == ".godot" || ext == ".tres")
        return "Asset";
    if (ext == ".cpp" || ext == ".c" || ext == ".h" || ext == ".hpp")
        return "C/C++";
    if (ext == ".py")
        return "Python";
    if (ext == ".js" || ext == ".ts")
        return "JavaScript";
    if (ext == ".sh" || ext == ".bash")
        return "Shell";
    if (ext == ".cs")
        return "C#";
    return "Standard File";
}

// Helper to convert filesystem permissions into a clean rwx string
std::string pStr(fs::perms p, fs::perms r, fs::perms w, fs::perms x)
{
    std::string s = "";
    s += ((p & r) != fs::perms::none ? "r" : "-");
    s += ((p & w) != fs::perms::none ? "w" : "-");
    s += ((p & x) != fs::perms::none ? "x" : "-");
    return s;
}

// Same as pStr, but each letter is colored by what it means:
// green = readable, yellow = writable, red = executable, dim = not granted.
// This is the only place color encodes real information rather than
// just category-branding, so it earns its own helper.
std::string pStrColored(fs::perms p, fs::perms r, fs::perms w, fs::perms x,
                        const std::string &GREEN, const std::string &YELLOW,
                        const std::string &RED, const std::string &DIM,
                        const std::string &RESET)
{
    std::string s = "";
    s += (p & r) != fs::perms::none ? GREEN + "r" + RESET : DIM + "-" + RESET;
    s += (p & w) != fs::perms::none ? YELLOW + "w" + RESET : DIM + "-" + RESET;
    s += (p & x) != fs::perms::none ? RED + "x" + RESET : DIM + "-" + RESET;
    return s;
}

// ---------------------------------------------------------
// GLOBAL DETECTORS
// ---------------------------------------------------------
void applyFolderBadges(const fs::path &p, std::vector<Badge> &badges, std::string &icon, std::string &color)
{
    std::string filename = p.filename().string();

    icon = "󰉋";
    color = "\033[38;5;81m";

    const std::string CYAN = "\033[38;5;81m", BLUE = "\033[38;5;75m", YELLOW = "\033[38;5;222m";
    const std::string MAGENTA = "\033[38;5;198m", ORANGE = "\033[38;5;208m";

    if (filename == "Downloads")
    {
        badges.push_back({"󰇚", "Downloads", CYAN});
        icon = "󰇚";
        color = CYAN;
    }
    else if (filename == "Pictures")
    {
        badges.push_back({"󰋩", "Pictures", MAGENTA});
        icon = "󰋩";
        color = MAGENTA;
    }
    else if (filename == "Documents")
    {
        badges.push_back({"󰈙", "Documents", BLUE});
        icon = "󰈙";
        color = BLUE;
    }
    else if (filename == "Videos")
    {
        badges.push_back({"󰕧", "Videos", ORANGE});
        icon = "󰕧";
        color = ORANGE;
    }
    else if (filename == "Music")
    {
        badges.push_back({"󰝚", "Music", YELLOW});
        icon = "󰝚";
        color = YELLOW;
    }
    else if (filename == "Desktop")
    {
        badges.push_back({"󰧨", "Desktop", CYAN});
        icon = "󰧨";
        color = CYAN;
    }
}

void applyFileBadges(const fs::path &p, std::vector<Badge> &badges, std::string &icon, std::string &color)
{
    std::string ext = getExt(p);
    std::string filename = p.filename().string();
    icon = "󰈔";
    color = "\033[0m";

    const std::string CYAN = "\033[38;5;81m", GREEN = "\033[38;5;121m", BLUE = "\033[38;5;75m";
    const std::string YELLOW = "\033[38;5;222m", MAGENTA = "\033[38;5;198m", ORANGE = "\033[38;5;208m";
    const std::string RED = "\033[38;5;196m", DIM = "\033[38;5;240m";

    if (ext == ".cs")
    {
        badges.push_back({"󰌛", "C#", GREEN});
        icon = "󰌛";
        color = GREEN;
    }
    else if (ext == ".cpp" || ext == ".c" || ext == ".h" || ext == ".hpp")
    {
        badges.push_back({"󰙱", "C/C++", BLUE});
        icon = "󰙱";
        color = BLUE;
    }
    else if (ext == ".py")
    {
        badges.push_back({"󰌠", "Python", YELLOW});
        icon = "󰌠";
        color = YELLOW;
    }
    else if (ext == ".js" || ext == ".ts")
    {
        badges.push_back({"󰌞", "JavaScript", YELLOW});
        icon = "󰌞";
        color = YELLOW;
    }
    else if (ext == ".sh" || ext == ".bash")
    {
        badges.push_back({"󱆃", "Shell", GREEN});
        icon = "󱆃";
        color = GREEN;
    }
    else if (ext == ".gd")
    {
        badges.push_back({"󰘦", "GDScript", CYAN});
        icon = "󰘦";
        color = CYAN;
    }

    else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" || ext == ".svg" || ext == ".webp")
    {
        badges.push_back({"󰋩", "Image", MAGENTA});
        icon = "󰋩";
        color = MAGENTA;
    }
    else if (ext == ".mp4" || ext == ".mkv" || ext == ".mov" || ext == ".webm")
    {
        badges.push_back({"󰕧", "Video", ORANGE});
        icon = "󰕧";
        color = ORANGE;
    }
    else if (ext == ".mp3" || ext == ".wav" || ext == ".flac" || ext == ".ogg")
    {
        badges.push_back({"󰝚", "Audio", YELLOW});
        icon = "󰝚";
        color = YELLOW;
    }

    else if (ext == ".pdf" || ext == ".txt" || ext == ".md" || ext == ".doc" || ext == ".docx")
    {
        badges.push_back({"󰈙", "Document", BLUE});
        icon = "󰈙";
        color = BLUE;
    }
    else if (ext == ".zip" || ext == ".tar" || ext == ".gz" || ext == ".rar" || ext == ".7z")
    {
        badges.push_back({"󰀼", "Archive", RED});
        icon = "󰀼";
        color = RED;
    }

    else if (ext == ".json" || ext == ".toml" || ext == ".yaml" || ext == ".ini" || ext == ".conf")
    {
        badges.push_back({"󰒓", "Config", DIM});
        icon = "󰒓";
        color = DIM;
    }
    else if (ext == ".blend" || ext == ".blend1")
    {
        badges.push_back({"󰆧", "Blender", ORANGE});
        icon = "󰆧";
        color = ORANGE;
    }
    else if (ext == ".tscn" || ext == ".godot" || ext == ".tres")
    {
        badges.push_back({"󰺿", "Asset", CYAN});
        icon = "󰺿";
        color = CYAN;
    }
    else if (filename == "Makefile" || ext == ".ninja" || ext == ".cmake")
    {
        badges.push_back({"󱁤", "Build", YELLOW});
        icon = "󱁤";
        color = YELLOW;
    }
}

// ---------------------------------------------------------
// MAIN EXECUTABLE
// ---------------------------------------------------------
int main(int argc, char *argv[])
{
    const std::string RESET = "\033[0m", BOLD = "\033[1m", ITALIC = "\033[3m", DIM = "\033[38;5;240m", DIM_IT = "\033[3;38;5;240m";
    const std::string BLUE = "\033[38;5;75m", CYAN = "\033[38;5;81m", MAGENTA = "\033[38;5;198m", YELLOW = "\033[38;5;222m", ORANGE = "\033[38;5;208m";
    const std::string GREEN = "\033[38;5;121m", RED = "\033[38;5;196m", WHITE = "\033[97m";

    // =========================================================
    // MODE 1: THE SPECIFIC TARGET FETCH UI
    // =========================================================
    if (argc == 2)
    {
        std::string arg_str = argv[1];
        if (!arg_str.empty() && arg_str.back() == '/')
            arg_str.pop_back();
        fs::path target = fs::current_path() / arg_str;

        if (!fs::exists(target))
        {
            std::cout << MAGENTA << "󰩹 Error: " << RESET << "Target does not exist.\n";
            return 1;
        }

        std::string target_name = target.filename().string();
        std::string parent_path = target.parent_path().string();
        std::string abs_str = fs::absolute(target).string();
        fs::path scan_path = fs::is_directory(target) ? target : target.parent_path();

        std::vector<Badge> raw_badges;

        // 1. Inherit Realms (Material Design Home restored: 󰋜)
        if (abs_str.starts_with("/home"))
            raw_badges.push_back({"󰋜", "Home", "\033[1;38;5;111m"});
        else if (abs_str.starts_with("/bin") || abs_str.starts_with("/sbin") || abs_str.starts_with("/usr/bin"))
            raw_badges.push_back({"󰆍", "Armory", "\033[38;5;196m"});
        else if (abs_str.starts_with("/etc"))
            raw_badges.push_back({"󰦨", "Config", YELLOW});
        else if (abs_str.starts_with("/dev"))
            raw_badges.push_back({"󰢛", "Hardware", BLUE});
        else
            raw_badges.push_back({"󰣇", "Root", "\033[1;38;5;38m"});

        if (hasFileUpwards(scan_path, ".git"))
            raw_badges.push_back({"󰊢", "Git", ORANGE});
        if (hasFileUpwards(scan_path, "project.godot"))
            raw_badges.push_back({"󰺿", "Godot", CYAN});

        // 2. Fetch Intrinsic
        std::string main_icon, main_color;
        if (fs::is_directory(target))
            applyFolderBadges(target, raw_badges, main_icon, main_color);
        else
            applyFileBadges(target, raw_badges, main_icon, main_color);

        // 3. Compile Floating Badges
        std::vector<Badge> unique_badges;
        std::string badge_str = "";
        for (const auto &b : raw_badges)
        {
            if (std::find(unique_badges.begin(), unique_badges.end(), b) == unique_badges.end())
            {
                unique_badges.push_back(b);
                badge_str += b.color + b.icon + " " + b.text + RESET + "   ";
            }
        }

        // 4. Fetch System Permissions
        fs::perms p = fs::status(target).permissions();
        std::string u_str = pStr(p, fs::perms::owner_read, fs::perms::owner_write, fs::perms::owner_exec);
        std::string g_str = pStr(p, fs::perms::group_read, fs::perms::group_write, fs::perms::group_exec);
        std::string o_str = pStr(p, fs::perms::others_read, fs::perms::others_write, fs::perms::others_exec);

        // 5. Render the Info Panel
        // Color rules, each chosen to mean something rather than just look
        // nice:
        //  - label ICON is colored by category (Path=blue, Perms=cyan,
        //    Size/Items=magenta, Type=yellow) -- same convention the badges
        //    already use, so an icon's color always tells you "what kind
        //    of fact is this".
        //  - label TEXT and VALUE text are plain bold/regular white --
        //    they're just content, not a status, so they don't need color.
        //  - "::" is dim -- it's punctuation, not data.
        //  - permission letters are the one place color IS the data:
        //    green=readable, yellow=writable, red=executable, dim=denied.
        const int LABEL_W = 7;
        std::cout << "\n";
        std::cout << CYAN << "╭─ " << RESET << BOLD << WHITE << target_name << RESET << "   " << badge_str << "\n\n";

        std::string u_col = pStrColored(p, fs::perms::owner_read, fs::perms::owner_write, fs::perms::owner_exec, GREEN, YELLOW, RED, DIM, RESET);
        std::string g_col = pStrColored(p, fs::perms::group_read, fs::perms::group_write, fs::perms::group_exec, GREEN, YELLOW, RED, DIM, RESET);
        std::string o_col = pStrColored(p, fs::perms::others_read, fs::perms::others_write, fs::perms::others_exec, GREEN, YELLOW, RED, DIM, RESET);

        std::cout << "│  " << BLUE << "󰉋 " << RESET << BOLD << WHITE << std::left << std::setw(LABEL_W) << "Path" << RESET << DIM << "  " << RESET << WHITE << parent_path << RESET << "\n";
        std::cout << "│  " << CYAN << "󰌾 " << RESET << BOLD << WHITE << std::left << std::setw(LABEL_W) << "Perms" << RESET << DIM << "  " << RESET
                  << WHITE << "u:" << RESET << u_col << WHITE << "  g:" << RESET << g_col << WHITE << "  o:" << RESET << o_col << "\n";

        std::string footer_summary;
        if (fs::is_directory(target))
        {
            auto [subfolders, subfiles] = countEntries(target);
            footer_summary = std::to_string(subfolders) + " folders, " + std::to_string(subfiles) + " files";
            std::cout << "│  " << MAGENTA << "󰑭 " << RESET << BOLD << WHITE << std::left << std::setw(LABEL_W) << "Items" << RESET << DIM << "  " << RESET << WHITE << subfiles << " files, " << subfolders << " folders" << RESET << "\n";
            std::cout << "│  " << YELLOW << "󰏖 " << RESET << BOLD << WHITE << std::left << std::setw(LABEL_W) << "Type" << RESET << DIM << "  " << RESET << WHITE << "Directory Realm" << RESET << "\n";
            if (fs::exists(target))
            {
                try
                {
                    auto modified = fs::last_write_time(target);
                    std::cout << "│  " << CYAN << "󰞱 " << RESET << BOLD << WHITE << std::left << std::setw(LABEL_W) << "Modified" << RESET << DIM << "  " << RESET << WHITE << formatTime(modified) << RESET << "\n";
                }
                catch (...) {}
            }
            std::cout << "\n";
        }
        else
        {
            std::string mass = "???";
            try
            {
                mass = formatSize(fs::file_size(target));
            }
            catch (...)
            {
            }
            std::string type_label = getTypeName(target);
            footer_summary = mass + " · " + type_label;
            std::cout << "│  " << MAGENTA << "󰑭 " << RESET << BOLD << WHITE << std::left << std::setw(LABEL_W) << "Mass" << RESET << DIM << "  " << RESET << WHITE << mass << RESET << "\n";
            std::cout << "│  " << YELLOW << "󰏖 " << RESET << BOLD << WHITE << std::left << std::setw(LABEL_W) << "Type" << RESET << DIM << "  " << RESET << WHITE << type_label << RESET << "\n";
            if (fs::exists(target))
            {
                try
                {
                    auto modified = fs::last_write_time(target);
                    std::cout << "│  " << CYAN << "󰞱 " << RESET << BOLD << WHITE << std::left << std::setw(LABEL_W) << "Modified" << RESET << DIM << "  " << RESET << WHITE << formatTime(modified) << RESET << "\n";
                }
                catch (...) {}
            }
            std::cout << "\n";
        }
        std::cout << CYAN << "╰─ " << RESET << DIM << footer_summary << RESET << "\n\n";
        return 0;
    }

    // =========================================================
    // MODE 2: STANDARD DIRECTORY LISTING
    // =========================================================
    fs::path target_path = fs::current_path();
    fs::path abs_path = fs::absolute(target_path);
    std::string path_str = abs_path.string();

    // 1. Gather Parent Badges
    std::vector<Badge> env_badges;
    if (path_str.starts_with("/home"))
        env_badges.push_back({"󰋜", "Home", "\033[1;38;5;111m"});
    else if (path_str.starts_with("/bin") || path_str.starts_with("/sbin") || path_str.starts_with("/usr/bin"))
        env_badges.push_back({"󰆍", "Armory", "\033[38;5;196m"});
    else if (path_str.starts_with("/etc"))
        env_badges.push_back({"󰦨", "Config", YELLOW});
    else if (path_str.starts_with("/dev"))
        env_badges.push_back({"󰢛", "Hardware", BLUE});
    else
        env_badges.push_back({"󰣇", "Root", "\033[1;38;5;38m"});

    if (path_str.find("/Downloads") != std::string::npos)
        env_badges.push_back({"󰇚", "Downloads", CYAN});
    if (path_str.find("/Pictures") != std::string::npos)
        env_badges.push_back({"󰋩", "Pictures", MAGENTA});
    if (path_str.find("/Documents") != std::string::npos)
        env_badges.push_back({"󰈙", "Documents", BLUE});
    if (path_str.find("/Videos") != std::string::npos)
        env_badges.push_back({"󰕧", "Videos", ORANGE});
    if (path_str.find("/Music") != std::string::npos)
        env_badges.push_back({"󰝚", "Music", YELLOW});
    if (path_str.find("/Desktop") != std::string::npos)
        env_badges.push_back({"󰧨", "Desktop", CYAN});

    if (hasFileUpwards(abs_path, ".git"))
        env_badges.push_back({"󰊢", "Git", ORANGE});
    if (hasFileUpwards(abs_path, "project.godot"))
        env_badges.push_back({"󰺿", "Godot", CYAN});

    std::string dir_display_name = abs_path.filename().string();
    if (dir_display_name.empty())
        dir_display_name = "/";

    // Header Output
    std::cout << "\n"
              << CYAN << "╭─ " << RESET << BOLD << WHITE << dir_display_name << RESET << "   ";
    for (const auto &badge : env_badges)
        std::cout << badge.color << badge.icon << " " << badge.text << RESET << "   ";
    std::cout << "\n\n";

    // 2. Process Files
    std::vector<fs::directory_entry> entries;
    for (const auto &entry : fs::directory_iterator(target_path))
        entries.push_back(entry);

    std::sort(entries.begin(), entries.end(), [](const fs::directory_entry &a, const fs::directory_entry &b)
              {
        if (a.is_directory() && !b.is_directory()) return true;
        if (!a.is_directory() && b.is_directory()) return false;
        return a.path().filename().string() < b.path().filename().string(); });

    size_t total = entries.size();
    size_t index = 0;
    size_t folders = 0;
    size_t files = 0;
    for (const auto &entry : entries)
    {
        std::string branch = (index + 1 == total) ? "└─ " : "├─ ";
        index++;
        std::vector<Badge> item_badges;
        std::string main_icon, main_color;

        if (entry.is_directory())
        {
            Badge gitBadge = {"󰊢", "Git", ORANGE};
            if (fs::exists(entry.path() / ".git") && std::find(env_badges.begin(), env_badges.end(), gitBadge) == env_badges.end())
                item_badges.push_back(gitBadge);

            Badge godotBadge = {"󰺿", "Godot", CYAN};
            if (fs::exists(entry.path() / "project.godot") && std::find(env_badges.begin(), env_badges.end(), godotBadge) == env_badges.end())
                item_badges.push_back(godotBadge);

            applyFolderBadges(entry.path(), item_badges, main_icon, main_color);
        }
        else
        {
            applyFileBadges(entry.path(), item_badges, main_icon, main_color);
        }

        std::string badge_str = "";
        for (const auto &b : item_badges)
            badge_str += b.color + b.icon + " " + b.text + RESET + "   ";

        if (!entry.is_directory() && (entry.path().filename().string() == "Makefile" || getExt(entry.path()) == ".ninja" || getExt(entry.path()) == ".cmake"))
        {
            main_icon = "󱁤";
            main_color = YELLOW;
            if (item_badges.empty())
                badge_str = YELLOW + std::string("󱁤 Build") + RESET + "   ";
        }

        if (entry.is_directory())
        {
            folders++;
            std::cout << branch << main_color << main_icon << " " << RESET << WHITE << std::left << std::setw(24) << entry.path().filename().string() << RESET
                      << badge_str << "\n";
        }
        else
        {
            files++;
            std::string size_str = "???";
            try
            {
                size_str = formatSize(fs::file_size(entry));
            }
            catch (...)
            {
            }
            std::cout << branch << main_color << main_icon << " " << RESET << WHITE << std::left << std::setw(24) << entry.path().filename().string() << RESET
                      << WHITE << std::left << std::setw(10) << size_str << RESET << badge_str << "\n";
        }
    }

    std::cout << "\n"
              << CYAN << "╰─ " << RESET << DIM << folders << " folders, " << files << " files" << RESET << "\n\n";
    return 0;
}