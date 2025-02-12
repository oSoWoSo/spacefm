/*
 *
 * License: See COPYING file
 *
 */

#include <string>
#include <vector>
#include <iostream>
#include <filesystem>

#include <fcntl.h>

#include "vendor/ztd/ztd.hxx"

#include "settings.hxx"
#include "extern.hxx"

#include "logger.hxx"
#include "utils.hxx"

void
print_command(const std::string& command)
{
    LOG_INFO("COMMAND={}", command);
}

void
print_task_command(const char* ptask, const char* cmd)
{
    LOG_INFO("TASK_COMMAND({:p})={}", ptask, cmd);
}

void
print_task_command_spawn(char* argv[], int pid)
{
    LOG_INFO("SPAWN=");
    int i = 0;
    while (argv[i])
    {
        LOG_INFO("  {}", argv[i]);
        i++;
    }

    LOG_INFO("    pid = {}", pid);
}

char*
randhex8()
{
    char hex[9];
    unsigned int n = mrand48();
    snprintf(hex, sizeof(hex), "%08x", n);
    // LOG_INFO("rand  : {}", n);
    // LOG_INFO("hex   : {}", hex);
    return g_strdup(hex);
}

char*
replace_line_subs(const char* line)
{
    const char* perc[] =
        {"%f", "%F", "%n", "%N", "%d", "%D", "%v", "%l", "%m", "%y", "%b", "%t", "%p", "%a"};
    const char* var[] = {"\"${fm_file}\"",
                         "\"${fm_files[@]}\"",
                         "\"${fm_filename}\"",
                         "\"${fm_filenames[@]}\"",
                         "\"${fm_pwd}\"",
                         "\"${fm_pwd}\"",
                         "\"${fm_device}\"",
                         "\"${fm_device_label}\"",
                         "\"${fm_device_mount_point}\"",
                         "\"${fm_device_fstype}\"",
                         "\"${fm_bookmark}\"",
                         "\"${fm_task_pwd}\"",
                         "\"${fm_task_pid}\"",
                         "\"${fm_value}\""};

    std::string s = line;
    int num = G_N_ELEMENTS(perc);
    int i;
    for (i = 0; i < num; i++)
    {
        if (strstr(line, perc[i]))
        {
            std::string old_s = s;
            s = ztd::replace(old_s, perc[i], var[i]);
        }
    }
    char* ss = const_cast<char*>(s.c_str());
    return ss;
}

bool
have_x_access(const char* path)
{
    if (!path)
        return false;
    return (faccessat(0, path, R_OK | X_OK, AT_EACCESS) == 0);
}

bool
have_rw_access(const char* path)
{
    if (!path)
        return false;
    return (faccessat(0, path, R_OK | W_OK, AT_EACCESS) == 0);
}

bool
dir_has_files(const char* path)
{
    if (!path)
        return false;

    if (!std::filesystem::exists(path))
        return false;

    if (!std::filesystem::is_directory(path))
        return false;

    for (const auto& file: std::filesystem::directory_iterator(path))
    {
        if (file.exists())
            return true;
    }

    return false;
}

char*
get_name_extension(const char* full_name, bool is_dir, char** ext)
{
    char* str;
    char* full = g_strdup(full_name);
    // get last dot
    char* dot;
    if (is_dir || !(dot = strrchr(full, '.')) || dot == full)
    {
        // dir or no dots or one dot first
        *ext = nullptr;
        return full;
    }
    dot[0] = '\0';
    char* final_ext = dot + 1;
    // get previous dot
    dot = strrchr(full, '.');
    unsigned int final_ext_len = strlen(final_ext);
    if (dot && !strcmp(dot + 1, "tar") && final_ext_len < 11 && final_ext_len)
    {
        // double extension
        final_ext[-1] = '.';
        *ext = g_strdup(dot + 1);
        dot[0] = '\0';
        str = g_strdup(full);
        g_free(full);
        return str;
    }
    // single extension, one or more dots
    if (final_ext_len < 11 && final_ext[0])
    {
        *ext = g_strdup(final_ext);
        str = g_strdup(full);
        g_free(full);
        return str;
    }
    else
    {
        // extension too long, probably part of name
        final_ext[-1] = '.';
        *ext = nullptr;
        return full;
    }
}

const std::string
get_prog_executable()
{
    return std::filesystem::read_symlink("/proc/self/exe");
}

void
open_in_prog(const char* path)
{
    const std::string exe = get_prog_executable();
    const std::string qpath = bash_quote(path);
    const std::string command = fmt::format("{} {}", exe, qpath);
    print_command(command);
    g_spawn_command_line_async(command.c_str(), nullptr);
}

std::string
bash_quote(const std::string& str)
{
    if (str.empty())
        return "\"\"";
    std::string s1 = ztd::replace(str, "\"", "\\\"");
    s1 = fmt::format("\"{}\"", s1);
    return s1;
}

char*
clean_label(const char* menu_label, bool kill_special, bool escape)
{
    std::string new_menu_label;

    if (menu_label && strstr(menu_label, "\\_"))
    {
        new_menu_label = ztd::replace(menu_label, "\\_", "@UNDERSCORE@");
        new_menu_label = ztd::replace(menu_label, "_", "");
        new_menu_label = ztd::replace(menu_label, "@UNDERSCORE@", "_");
    }
    else
        new_menu_label = ztd::replace(menu_label, "_", "");
    if (kill_special)
    {
        new_menu_label = ztd::replace(menu_label, "&", "");
        new_menu_label = ztd::replace(menu_label, " ", "-");
    }
    else if (escape)
        new_menu_label = g_markup_escape_text(menu_label, -1);

    char* s1 = g_strdup(new_menu_label.c_str());
    return s1;
}

void
string_copy_free(char** s, const char* src)
{
    char* discard = *s;
    *s = g_strdup(src);
    g_free(discard);
}

char*
unescape(const char* t)
{
    if (!t)
        return nullptr;

    char* s = g_strdup(t);

    int i = 0, j = 0;
    while (t[i])
    {
        switch (t[i])
        {
            case '\\':
                switch (t[++i])
                {
                    case 'n':
                        s[j] = '\n';
                        break;
                    case 't':
                        s[j] = '\t';
                        break;
                    case '\\':
                        s[j] = '\\';
                        break;
                    case '\"':
                        s[j] = '\"';
                        break;
                    default:
                        // copy
                        s[j++] = '\\';
                        s[j] = t[i];
                }
                break;
            default:
                s[j] = t[i];
        }
        ++i;
        ++j;
    }
    s[j] = t[i]; // null char
    return s;
}

char*
get_valid_su() // may return nullptr
{
    unsigned int i;
    char* use_su = nullptr;

    use_su = g_strdup(xset_get_s("su_command"));

    if (use_su)
    {
        // is Prefs use_su in list of valid su commands?
        for (i = 0; i < G_N_ELEMENTS(su_commands); i++)
        {
            if (!strcmp(su_commands[i], use_su))
                break;
        }
        if (i == G_N_ELEMENTS(su_commands))
        {
            // not in list - invalid
            g_free(use_su);
            use_su = nullptr;
        }
    }
    if (!use_su)
    {
        // discovery
        for (i = 0; i < G_N_ELEMENTS(su_commands); i++)
        {
            if ((use_su = g_find_program_in_path(su_commands[i])))
                break;
        }
        if (!use_su)
            use_su = g_strdup(su_commands[0]);
        xset_set("su_command", "s", use_su);
    }
    char* su_path = g_find_program_in_path(use_su);
    g_free(use_su);
    return su_path;
}
