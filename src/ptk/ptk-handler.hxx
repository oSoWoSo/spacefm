/**
 * Copyright (C) 2015 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2013-2014 OmegaPhil <OmegaPhil@startmail.com>
 * Copyright (C) 2014 IgnorantGuru <ignorantguru@gmx.com>
 * Copyright (C) 2006 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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

#pragma once

#include <string>
#include <string_view>

#include <vector>

#include <gtk/gtk.h>
#include <glib.h>

#include "xset/xset.hxx"

#include "settings.hxx"

#include "ptk/ptk-file-browser.hxx"

enum PtkHandlerArchive
{
    HANDLER_COMPRESS,
    HANDLER_EXTRACT,
    HANDLER_LIST
};

enum PtkHandlerMount
{
    HANDLER_MOUNT,
    HANDLER_UNMOUNT,
    HANDLER_PROP
};

enum PtkHandlerMode
{
    HANDLER_MODE_ARC,
    HANDLER_MODE_FS,
    HANDLER_MODE_NET,
    HANDLER_MODE_FILE
};

void ptk_handler_add_defaults(i32 mode, bool overwrite, bool add_missing);
void ptk_handler_import(i32 mode, GtkWidget* handler_dlg, xset_t set);
void ptk_handler_show_config(i32 mode, PtkFileBrowser* file_browser, xset_t def_handler_set);
bool ptk_handler_values_in_list(std::string_view list, const std::vector<std::string>& values,
                                std::string& msg);
bool ptk_handler_load_script(i32 mode, i32 cmd, xset_t handler_set, GtkTextView* view,
                             std::string& script, std::string& error_message);
bool ptk_handler_save_script(i32 mode, i32 cmd, xset_t handler_set, GtkTextView* view,
                             const std::string command, std::string& error_message);
char* ptk_handler_get_command(i32 mode, i32 cmd, xset_t handler_set);
bool ptk_handler_command_is_empty(std::string_view command);
const std::vector<xset_t> ptk_handler_file_has_handlers(i32 mode, i32 cmd, std::string_view path,
                                                        vfs::mime_type mime_type, bool test_cmd,
                                                        bool multiple, bool enabled_only);
