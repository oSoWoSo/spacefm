/**
 * Copyright (C) 2005 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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

#include <filesystem>

#include <array>
#include <vector>

#include <glibmm.h>

#include <ztd/ztd.hxx>

#include <glib.h>
#include <gtk/gtk.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include <magic_enum.hpp>

enum VFSFileTaskType
{
    MOVE,
    COPY,
    TRASH,
    DELETE,
    LINK,
    CHMOD_CHOWN, // These two kinds of operation have lots in common,
                 // so put them together to reduce duplicated disk I/O
    EXEC,
    LAST,
};

enum ChmodActionType
{
    OWNER_R,
    OWNER_W,
    OWNER_X,
    GROUP_R,
    GROUP_W,
    GROUP_X,
    OTHER_R,
    OTHER_W,
    OTHER_X,
    SET_UID,
    SET_GID,
    STICKY,
};

inline constexpr std::array<std::filesystem::perms, 12> chmod_flags{
    // User
    std::filesystem::perms::owner_read,
    std::filesystem::perms::owner_write,
    std::filesystem::perms::owner_exec,
    // Group
    std::filesystem::perms::group_read,
    std::filesystem::perms::group_write,
    std::filesystem::perms::group_exec,

    // Other
    std::filesystem::perms::others_read,
    std::filesystem::perms::others_write,
    std::filesystem::perms::others_exec,

    // uid/gid
    std::filesystem::perms::set_uid,
    std::filesystem::perms::set_gid,

    // sticky bit
    std::filesystem::perms::sticky_bit,
};

enum VFSFileTaskState
{
    RUNNING,
    SIZE_TIMEOUT,
    QUERY_OVERWRITE,
    ERROR,
    PAUSE,
    QUEUE,
    FINISH,
};

enum VFSFileTaskOverwriteMode
{
    // do not reposition first four values
    OVERWRITE,     // Overwrite current dest file / Ask
    OVERWRITE_ALL, // Overwrite all existing files without prompt
    SKIP_ALL,      // Do not try to overwrite any files
    AUTO_RENAME,   // Assign a new unique name
    SKIP,          // Do not overwrite current file
    RENAME,        // Rename file
};

enum VFSExecType
{
    NORMAL,
    CUSTOM,
};

struct VFSFileTask;

using VFSFileTaskStateCallback = bool (*)(VFSFileTask* task, VFSFileTaskState state,
                                          void* state_data, void* user_data);

struct VFSFileTask
{
    VFSFileTaskType type;
    std::vector<std::string> src_paths; // All source files. This list will be freed
                                        // after file operation is completed.
    std::string dest_dir;               // Destinaton directory
    bool avoid_changes;
    GSList* devs{nullptr};

    VFSFileTaskOverwriteMode overwrite_mode;
    bool recursive; // Apply operation to all files under directories
                    // recursively. This is default to copy/delete,
                    // and should be set manually for chown/chmod.

    // For chown
    uid_t uid;
    gid_t gid;

    // For chmod
    unsigned char* chmod_actions; // If chmod is not needed, this should be nullptr

    off_t total_size; // Total size of the files to be processed, in bytes
    off_t progress;   // Total size of current processed files, in btytes
    i32 percent{0};   // progress (percentage)
    bool custom_percent{false};
    off_t last_speed{0};
    off_t last_progress{0};
    f64 last_elapsed{0.0};
    u32 current_item{0};

    ztd::timer timer;
    std::time_t start_time;

    std::string current_file; // copy of Current processed file
    std::string current_dest; // copy of Current destination file

    i32 err_count{0};
    i32 error{0};
    bool error_first{true};

    GThread* thread;
    VFSFileTaskState state;
    VFSFileTaskState state_pause{VFSFileTaskState::RUNNING};
    bool abort{false};
    GCond* pause_cond{nullptr};
    bool queue_start{false};

    VFSFileTaskStateCallback state_cb;
    void* state_cb_data;

    GMutex* mutex;

    // sfm write directly to gtk buffer for speed
    GtkTextBuffer* add_log_buf;
    GtkTextMark* add_log_end;

    // MOD run task
    VFSExecType exec_type{VFSExecType::NORMAL};
    std::string exec_action;
    std::string exec_command;
    bool exec_sync{true};
    bool exec_popup{false};
    bool exec_show_output{false};
    bool exec_show_error{false};
    bool exec_terminal{false};
    bool exec_keep_terminal{false};
    bool exec_export{false};
    bool exec_direct{false};
    std::vector<std::string> exec_argv; // for exec_direct, command ignored
                                        // for su commands, must use bash -c
                                        // as su does not execute binaries
    std::string exec_script;
    bool exec_keep_tmp{false}; // diagnostic to keep temp files
    void* exec_browser{nullptr};
    void* exec_desktop{nullptr};
    std::string exec_as_user;
    std::string exec_icon;
    Glib::Pid exec_pid;
    i32 exec_exit_status{0};
    u32 child_watch{0};
    bool exec_is_error{false};
    GIOChannel* exec_channel_out;
    GIOChannel* exec_channel_err;
    bool exec_scroll_lock{false};
    bool exec_checksum{false};
    void* exec_set{nullptr};
    GCond* exec_cond{nullptr};
    void* exec_ptask{nullptr};
};

VFSFileTask* vfs_task_new(VFSFileTaskType task_type, const std::vector<std::string>& src_files,
                          std::string_view dest_dir);

void vfs_file_task_lock(VFSFileTask* task);
void vfs_file_task_unlock(VFSFileTask* task);

/* Set some actions for chmod, this array will be copied
 * and stored in VFSFileTask */
void vfs_file_task_set_chmod(VFSFileTask* task, unsigned char* chmod_actions);

void vfs_file_task_set_chown(VFSFileTask* task, uid_t uid, gid_t gid);

void vfs_file_task_set_state_callback(VFSFileTask* task, VFSFileTaskStateCallback cb,
                                      void* user_data);

void vfs_file_task_set_recursive(VFSFileTask* task, bool recursive);

void vfs_file_task_set_overwrite_mode(VFSFileTask* task, VFSFileTaskOverwriteMode mode);

void vfs_file_task_run(VFSFileTask* task);

void vfs_file_task_try_abort(VFSFileTask* task);

void vfs_file_task_abort(VFSFileTask* task);

void vfs_file_task_free(VFSFileTask* task);

char* vfs_file_task_get_cpids(Glib::Pid pid);
void vfs_file_task_kill_cpids(char* cpids, i32 signal);
const std::string vfs_file_task_get_unique_name(std::string_view dest_dir,
                                                std::string_view base_name, std::string_view ext);
