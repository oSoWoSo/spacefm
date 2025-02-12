/*
 *      mime-type.h
 *
 *      Copyright 2007 Houng Jen Yee (PCMan) <pcman.tw@gmail.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 3 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#pragma once

#include <sys/stat.h>
#include <sys/types.h>
#include <glib.h>

#include "mime-action.hxx"
#include "mime-cache.hxx"

extern const char xdg_mime_type_unknown[];
#define XDG_MIME_TYPE_UNKNOWN xdg_mime_type_unknown
extern const char xdg_mime_type_directory[];
#define XDG_MIME_TYPE_DIRECTORY xdg_mime_type_directory
extern const char xdg_mime_type_executable[];
#define XDG_MIME_TYPE_EXECUTABLE xdg_mime_type_executable
extern const char xdg_mime_type_plain_text[];
#define XDG_MIME_TYPE_PLAIN_TEXT xdg_mime_type_plain_text

/* Initialize the library */
void mime_type_init();

/* Finalize the library and free related resources */
void mime_type_finalize();

/* Get additional info of the specified mime-type */
// MimeInfo* mime_type_get_by_type( const char* type_name );

/*
 * Get mime-type info of the specified file (quick, but less accurate):
 * Mime-type of the file is determined by cheking the filename only.
 * If statbuf != nullptr, it will be used to determine if the file is a directory,
 * or if the file is an executable file.
 */
const char* mime_type_get_by_filename(const char* filename, struct stat* statbuf);

/*
 * Get mime-type info of the specified file (slow, but more accurate):
 * To determine the mime-type of the file, mime_type_get_by_filename() is
 * tried first.  If the mime-type couldn't be determined, the content of
 * the file will be checked, which is much more time-consuming.
 * If statbuf is not nullptr, it will be used to determine if the file is a directory,
 * or if the file is an executable file; otherwise, the function will call stat()
 * to gather this info itself. So if you already have stat info of the file,
 * pass it to the function to prevent checking the file stat again.
 * If you have basename of the file, pass it to the function can improve the
 * efifciency, too. Otherwise, the function will try to get the basename of
 * the specified file again.
 */
const char* mime_type_get_by_file(const char* filepath, struct stat* statbuf, const char* basename);

bool mime_type_is_text_file(const char* file_path, const char* mime_type);

bool mime_type_is_executable_file(const char* file_path, const char* mime_type);

/* Check if the specified mime_type is the subclass of the specified parent type */
// bool mime_type_is_subclass(const char* type, const char* parent);

/* Get human-readable description and icon name of the mime-type
 * If locale is nullptr, current locale will be used.
 * The returned string should be freed when no longer used.
 * The icon_name will only be set if points to nullptr, and must be freed. */
char* mime_type_get_desc_icon(const char* type, const char* locale, char** icon_name);

/*
 * Iterate through all mime caches
 * Can be used to hook file alteration monitor for the cache files to handle reloading.
 */
// void mime_cache_foreach(GFunc func, void* user_data);

/*
 * Get mime caches
 */
MimeCache** mime_type_get_caches(int* n);

/* max magic extent of all caches */
extern uint32_t mime_cache_max_extent;
