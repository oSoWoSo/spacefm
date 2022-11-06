/**
 * Copyright (C) 2007 PCMan <pcman.tw@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/* Currently this library is NOT MT-safe */

#include <string>
#include <filesystem>

#include <vector>

#include <iostream>
#include <fstream>

#include <algorithm>
#include <ranges>

#include <memory>

#include <fcntl.h>

#include <fmt/format.h>

#include <glibmm.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "types.hxx"

#include "utils.hxx"

#include "vfs/vfs-user-dir.hxx"

#include "mime-type/mime-type.hxx"
#include "mime-type/mime-cache.hxx"

/* max extent used to checking text files */
inline constexpr i32 TEXT_MAX_EXTENT = 512;

/* Check if the specified mime_type is the subclass of the specified parent type */
static bool mime_type_is_subclass(std::string_view type, std::string_view parent);

std::vector<mime_cache_t> caches;

/* max magic extent of all caches */
static u32 mime_cache_max_extent = 0;

/* allocated buffer used for mime magic checking to
     prevent frequent memory allocation */
static char* mime_magic_buf = nullptr;
/* for MT safety, the buffer should be locked */
G_LOCK_DEFINE_STATIC(mime_magic_buf);

/* free all mime.cache files on the system */
static void mime_cache_free_all();

static bool mime_type_is_data_plain_text(const char* data, i32 len);

/*
 * Get mime-type of the specified file (quick, but less accurate):
 * Mime-type of the file is determined by cheking the filename only.
 */
static const std::string
mime_type_get_by_filename(std::string_view filename, std::filesystem::file_status status)
{
    const char* type = nullptr;
    const char* suffix_pos = nullptr;
    const char* prev_suffix_pos = (const char*)-1;

    if (std::filesystem::is_directory(status))
        return XDG_MIME_TYPE_DIRECTORY.data();

    for (mime_cache_t cache : caches)
    {
        type = cache->lookup_literal(filename);
        if (type)
            break;

        const char* _type = cache->lookup_suffix(filename, &suffix_pos);
        if (_type && suffix_pos < prev_suffix_pos)
        {
            type = _type;
            prev_suffix_pos = suffix_pos;

            if (type)
                break;
        }
    }

    if (!type) /* glob matching */
    {
        i32 max_glob_len = 0;
        i32 glob_len = 0;
        for (mime_cache_t cache : caches)
        {
            const char* matched_type;
            matched_type = cache->lookup_glob(filename, &glob_len);
            /* according to the mime.cache 1.0 spec, we should use the longest glob matched. */
            if (matched_type && glob_len > max_glob_len)
            {
                type = matched_type;
                max_glob_len = glob_len;
            }

            if (type)
                break;
        }
    }

    if (type && *type)
        return type;

    return XDG_MIME_TYPE_UNKNOWN.data();
}

/*
 * Get mime-type info of the specified file (slow, but more accurate):
 * To determine the mime-type of the file, mime_type_get_by_filename() is
 * tried first.  If the mime-type could not be determined, the content of
 * the file will be checked, which is much more time-consuming.
 * If statbuf is not nullptr, it will be used to determine if the file is a directory,
 * or if the file is an executable file; otherwise, the function will call stat()
 * to gather this info itself. So if you already have stat info of the file,
 * pass it to the function to prevent checking the file stat again.
 * If you have basename of the file, pass it to the function can improve the
 * efifciency, too. Otherwise, the function will try to get the basename of
 * the specified file again.
 */
const std::string
mime_type_get_by_file(std::string_view filepath)
{
    const auto status = std::filesystem::status(filepath);

    if (!std::filesystem::exists(status))
        return XDG_MIME_TYPE_UNKNOWN.data();

    if (std::filesystem::is_other(status))
        return XDG_MIME_TYPE_UNKNOWN.data();

    if (std::filesystem::is_directory(status))
        return XDG_MIME_TYPE_DIRECTORY.data();

    const std::string basename = Glib::path_get_basename(filepath.data());
    const std::string filename_type = mime_type_get_by_filename(basename, status);
    if (!ztd::same(filename_type, XDG_MIME_TYPE_UNKNOWN))
        return filename_type;

    const char* type = nullptr;

    // check for reg or link due to hangs on fifo and chr dev
    const auto file_size = std::filesystem::file_size(filepath);
    if (file_size > 0 &&
        (std::filesystem::is_regular_file(status) || std::filesystem::is_symlink(status)))
    {
        char* data;

        /* Open the file and map it into memory */
        const i32 fd = open(filepath.data(), O_RDONLY, 0);
        if (fd != -1)
        {
            i32 len = mime_cache_max_extent < file_size ? mime_cache_max_extent : file_size;
            /*
             * FIXME: Can g_alloca() be used here? It is very fast, but is it safe?
             * Actually, we can allocate a block of memory with the size of mime_cache_max_extent,
             * then we do not need to  do dynamic allocation/free every time, but multi-threading
             * will be a nightmare, so...
             */
            /* try to lock the common buffer */
            if (G_TRYLOCK(mime_magic_buf))
                data = mime_magic_buf;
            else /* the buffer is in use, allocate new one */
                data = CHAR(g_malloc(len));

            len = read(fd, data, len);

            if (len == -1)
            {
                if (data == mime_magic_buf)
                    G_UNLOCK(mime_magic_buf);
                else
                    free(data);
                data = (char*)-1;
            }
            if (data != (char*)-1)
            {
                for (usize i = 0; !type && i < caches.size(); ++i)
                {
                    type = caches.at(i)->lookup_magic(data, len);
                }

                /* Check for executable file */
                if (!type && have_x_access(filepath))
                    type = XDG_MIME_TYPE_EXECUTABLE.data();

                /* fallback: check for plain text */
                if (!type)
                {
                    if (mime_type_is_data_plain_text(data,
                                                     len > TEXT_MAX_EXTENT ? TEXT_MAX_EXTENT : len))
                        type = XDG_MIME_TYPE_PLAIN_TEXT.data();
                }

                if (data == mime_magic_buf)
                    G_UNLOCK(mime_magic_buf); /* unlock the common buffer */
                else                          /* we use our own buffer */
                    free(data);
            }
            close(fd);
        }
    }
    else
    {
        /* empty file can be viewed as text file */
        type = XDG_MIME_TYPE_PLAIN_TEXT.data();
    }

    if (type && *type)
        return type;

    return XDG_MIME_TYPE_UNKNOWN.data();
}

static char*
parse_xml_icon(const char* buf, usize len, bool is_local)
{ // Note: This function modifies contents of buf
    char* icon_tag = nullptr;

    if (is_local)
    {
        //  "<icon name=.../>" is only used in user .local XML files
        // find <icon name=
        icon_tag = g_strstr_len(buf, len, "<icon name=");
        if (icon_tag)
        {
            icon_tag += 11;
            len -= 11;
        }
    }
    if (!icon_tag && !is_local)
    {
        // otherwise find <generic-icon name=
        icon_tag = g_strstr_len(buf, len, "<generic-icon name=");
        if (icon_tag)
        {
            icon_tag += 19;
            len -= 19;
        }
    }
    if (!icon_tag)
        return nullptr; // no icon found

    // find />
    char* end_tag = g_strstr_len(icon_tag, len, "/>");
    if (!end_tag)
        return nullptr;
    end_tag[0] = '\0';
    if (strchr(end_tag, '\n'))
        return nullptr; // linefeed in tag

    // remove quotes
    if (icon_tag[0] == '"')
        icon_tag++;
    if (end_tag[-1] == '"')
        end_tag[-1] = '\0';

    if (icon_tag == end_tag)
        return nullptr; // blank name

    return ztd::strdup(icon_tag);
}

static char*
parse_xml_desc(const char* buf, usize len, const char* locale)
{
    const char* buf_end = buf + len;
    const char* comment = nullptr;
    const char* comment_end;
    const char* eng_comment;
    usize comment_len = 0;
    static const char end_comment_tag[] = "</comment>";

    eng_comment = g_strstr_len(buf, len, "<comment>"); /* default English comment */
    if (!eng_comment)                                  /* This xml file is invalid */
        return nullptr;
    len -= 9;
    eng_comment += 9;
    comment_end = g_strstr_len(eng_comment, len, end_comment_tag); /* find </comment> */
    if (!comment_end)
        return nullptr;
    usize eng_comment_len = comment_end - eng_comment;

    if (locale)
    {
        char target[64];
        i32 target_len = g_snprintf(target, 64, "<comment xml:lang=\"%s\">", locale);
        buf = comment_end + 10;
        len = (buf_end - buf);
        if ((comment = g_strstr_len(buf, len, target)))
        {
            len -= target_len;
            comment += target_len;
            comment_end = g_strstr_len(comment, len, end_comment_tag); /* find </comment> */
            if (comment_end)
                comment_len = (comment_end - comment);
            else
                comment = nullptr;
        }
    }
    if (comment)
        return strndup(comment, comment_len);
    return strndup(eng_comment, eng_comment_len);
}

static char*
_mime_type_get_desc_icon(std::string_view file_path, std::string_view locale, bool is_local,
                         char** icon_name)
{
    std::string line;
    std::ifstream file(file_path.data());
    if (!file.is_open())
        return nullptr;

    std::string buffer;
    while (std::getline(file, line))
    {
        buffer.append(line);
    }
    file.close();

    if (buffer.empty())
        return nullptr;

    const char* new_locale = locale.data();
    if (locale.empty())
    {
        const char* const* langs = g_get_language_names();
        char* dot = (char*)strchr(langs[0], '.');
        if (dot)
            new_locale = strndup(langs[0], (usize)(dot - langs[0]));
        else
            new_locale = langs[0];
    }
    char* desc = parse_xml_desc(buffer.data(), buffer.size(), new_locale);

    // only look for <icon /> tag in .local
    if (is_local && icon_name && *icon_name == nullptr)
        *icon_name = parse_xml_icon(buffer.data(), buffer.size(), is_local);

    return desc;
}

/* Get human-readable description and icon name of the mime-type
 * If locale is nullptr, current locale will be used.
 * The returned string should be freed when no longer used.
 * The icon_name will only be set if points to nullptr, and must be freed.
 *
 * Note: Spec is not followed for icon.  If icon tag is found in .local
 * xml file, it is used.  Otherwise vfs_mime_type_get_icon guesses the icon.
 * The Freedesktop spec /usr/share/mime/generic-icons is NOT parsed. */
const std::string
mime_type_get_desc_icon(std::string_view type, std::string_view locale, char** icon_name)
{
    /*  //sfm 0.7.7+ FIXED:
     * According to specs on freedesktop.org, user_data_dir has
     * higher priority than system_data_dirs, but in most cases, there was
     * no file, or very few files in user_data_dir, so checking it first will
     * result in many unnecessary open() system calls, yealding bad performance.
     * Since the spec really sucks, we do not follow it here.
     */

    const std::string file_path = fmt::format("{}/mime/{}.xml", vfs_user_data_dir(), type);
    if (faccessat(0, file_path.data(), F_OK, AT_EACCESS) != -1)
    {
        const char* desc = _mime_type_get_desc_icon(file_path, locale, true, icon_name);
        if (desc)
            return desc;
    }

    // look in system dirs
    for (std::string_view sys_dir : vfs_system_data_dir())
    {
        const std::string sys_file_path = fmt::format("{}/mime/{}.xml", sys_dir, type);
        if (faccessat(0, sys_file_path.data(), F_OK, AT_EACCESS) != -1)
        {
            const char* desc = _mime_type_get_desc_icon(sys_file_path, locale, false, icon_name);
            if (desc)
                return desc;
        }
    }
    return "";
}

void
mime_type_finalize()
{
    mime_cache_free_all();
}

// load all mime.cache files on the system,
// including /usr/share/mime/mime.cache,
// /usr/local/share/mime/mime.cache,
// and $HOME/.local/share/mime/mime.cache.
void
mime_type_init()
{
    const std::string filename = "/mime/mime.cache";

    const std::string path = Glib::build_filename(vfs_user_data_dir(), filename);
    mime_cache_t cache = std::make_shared<MimeCache>(path);
    caches.emplace_back(cache);

    if (cache->get_magic_max_extent() > mime_cache_max_extent)
        mime_cache_max_extent = cache->get_magic_max_extent();

    const std::vector<std::string> dirs = vfs_system_data_dir();
    for (std::string_view dir : dirs)
    {
        const std::string path2 = Glib::build_filename(dir.data(), filename);
        mime_cache_t dir_cache = std::make_shared<MimeCache>(path2);
        caches.emplace_back(dir_cache);

        if (dir_cache->get_magic_max_extent() > mime_cache_max_extent)
            mime_cache_max_extent = dir_cache->get_magic_max_extent();
    }

    mime_magic_buf = CHAR(g_malloc(mime_cache_max_extent));
}

/* free all mime.cache files on the system */
static void
mime_cache_free_all()
{
    caches.clear();

    mime_cache_max_extent = 0;

    free(mime_magic_buf);
    mime_magic_buf = nullptr;
}

void
mime_cache_reload(mime_cache_t cache)
{
    cache->reload();

    /* recalculate max magic extent */
    for (mime_cache_t mcache : caches)
    {
        if (mcache->get_magic_max_extent() > mime_cache_max_extent)
            mime_cache_max_extent = mcache->get_magic_max_extent();
    }

    G_LOCK(mime_magic_buf);

    mime_magic_buf = (char*)g_realloc(mime_magic_buf, mime_cache_max_extent);

    G_UNLOCK(mime_magic_buf);
}

static bool
mime_type_is_data_plain_text(const char* data, i32 len)
{
    if (len >= 0 && data)
    {
        for (i32 i = 0; i < len; ++i)
        {
            if (data[i] == '\0')
                return false;
        }
        return true;
    }
    return false;
}

bool
mime_type_is_text_file(std::string_view file_path, std::string_view mime_type)
{
    bool ret = false;

    if (!mime_type.empty())
    {
        if (ztd::same(mime_type, "application/pdf"))
            // seems to think this is XDG_MIME_TYPE_PLAIN_TEXT
            return false;
        if (mime_type_is_subclass(mime_type, XDG_MIME_TYPE_PLAIN_TEXT))
            return true;
        if (!ztd::startswith(mime_type, "text/") && !ztd::startswith(mime_type, "application/"))
            return false;
    }

    if (file_path.empty())
        return false;

    i32 file = open(file_path.data(), O_RDONLY);
    if (file != -1)
    {
        struct stat statbuf;
        if (fstat(file, &statbuf) != -1)
        {
            if (S_ISREG(statbuf.st_mode))
            {
                unsigned char data[TEXT_MAX_EXTENT];
                const i32 rlen = read(file, data, sizeof(data));
                ret = mime_type_is_data_plain_text((char*)data, rlen);
            }
        }
        close(file);
    }
    return ret;
}

bool
mime_type_is_executable_file(std::string_view file_path, std::string_view mime_type)
{
    if (mime_type.empty())
        mime_type = mime_type_get_by_file(file_path);

    /*
     * Only executable types can be executale.
     * Since some common types, such as application/x-shellscript,
     * are not in mime database, we have to add them ourselves.
     */
    if (!ztd::same(mime_type, XDG_MIME_TYPE_UNKNOWN) &&
        (mime_type_is_subclass(mime_type, XDG_MIME_TYPE_EXECUTABLE) ||
         mime_type_is_subclass(mime_type, "application/x-shellscript")))
    {
        if (!file_path.empty())
        {
            if (!have_x_access(file_path))
                return false;
        }
        return true;
    }
    return false;
}

/* Check if the specified mime_type is the subclass of the specified parent type */
static bool
mime_type_is_subclass(std::string_view type, std::string_view parent)
{
    /* special case, the type specified is identical to the parent type. */
    if (ztd::same(type, parent))
        return true;

    for (mime_cache_t cache : caches)
    {
        const std::vector<std::string> parents = cache->lookup_parents(type);
        for (std::string_view p : parents)
        {
            if (ztd::same(parent, p))
                return true;
        }
    }
    return false;
}

/*
 * Get mime caches
 */
const std::vector<mime_cache_t>&
mime_type_get_caches()
{
    return caches;
}

/*
 * Reload all mime caches
 */
void
mime_type_regen_all_caches()
{
    std::ranges::for_each(caches, mime_cache_reload);
}
