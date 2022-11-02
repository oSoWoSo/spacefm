/**
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

#include <exception>

// #define XSET_MAP_TEST

class InvalidXSetName: virtual public std::exception
{
  protected:
    std::string error_message;

  public:
    explicit InvalidXSetName(const std::string& msg) : error_message(msg)
    {
    }

    virtual ~InvalidXSetName() throw()
    {
    }

    virtual const char*
    what() const throw()
    {
        return error_message.c_str();
    }
};

enum class XSetName
{
    // All custom XSets will have this,
    // also the only XSetName not implemented in xset_name_map
    // trying to lookup a XSet name for this will throw InvalidXSetName
    CUSTOM,

    // separator
    SEPARATOR,

    // dev menu
    DEV_MENU_REMOVE,
    DEV_MENU_UNMOUNT,
    DEV_MENU_OPEN,
    DEV_MENU_TAB,
    DEV_MENU_MOUNT,
    DEV_MENU_MARK,
    DEV_PROP,
    DEV_MENU_SETTINGS,

    // dev settings
    DEV_SHOW,
    DEV_SHOW_INTERNAL_DRIVES,
    DEV_SHOW_EMPTY,
    DEV_SHOW_PARTITION_TABLES,
    DEV_SHOW_NET,
    DEV_SHOW_FILE,
    DEV_IGNORE_UDISKS_HIDE,
    DEV_SHOW_HIDE_VOLUMES,
    DEV_DISPNAME,

    DEV_MENU_AUTO,
    DEV_AUTOMOUNT_OPTICAL,
    DEV_AUTOMOUNT_REMOVABLE,
    DEV_IGNORE_UDISKS_NOPOLICY,
    DEV_AUTOMOUNT_VOLUMES,
    DEV_AUTOMOUNT_DIRS,
    DEV_AUTO_OPEN,
    DEV_UNMOUNT_QUIT,

    DEV_EXEC,
    DEV_EXEC_FS,
    DEV_EXEC_AUDIO,
    DEV_EXEC_VIDEO,
    DEV_EXEC_INSERT,
    DEV_EXEC_UNMOUNT,
    DEV_EXEC_REMOVE,

    DEV_MOUNT_OPTIONS,
    DEV_CHANGE,
    DEV_FS_CNF,
    DEV_NET_CNF,

    // dev icons
    DEV_ICON,
    DEV_ICON_INTERNAL_MOUNTED,
    DEV_ICON_INTERNAL_UNMOUNTED,
    DEV_ICON_REMOVE_MOUNTED,
    DEV_ICON_REMOVE_UNMOUNTED,
    DEV_ICON_OPTICAL_MOUNTED,
    DEV_ICON_OPTICAL_MEDIA,
    DEV_ICON_OPTICAL_NOMEDIA,
    DEV_ICON_AUDIOCD,
    DEV_ICON_FLOPPY_MOUNTED,
    DEV_ICON_FLOPPY_UNMOUNTED,
    DEV_ICON_NETWORK,
    DEV_ICON_FILE,

    BOOK_OPEN,
    BOOK_SETTINGS,
    BOOK_ICON,
    BOOK_MENU_ICON,
    BOOK_SHOW,
    BOOK_ADD,
    MAIN_BOOK,

    // Rename/Move Dialog
    MOVE_NAME,
    MOVE_FILENAME,
    MOVE_PARENT,
    MOVE_PATH,
    MOVE_TYPE,
    MOVE_TARGET,
    MOVE_TEMPLATE,
    MOVE_OPTION,
    MOVE_COPY,
    MOVE_LINK,
    MOVE_COPYT,
    MOVE_LINKT,
    MOVE_AS_ROOT,
    MOVE_DLG_HELP,
    MOVE_DLG_CONFIRM_CREATE,

    // status bar
    STATUS_MIDDLE,
    STATUS_NAME,
    STATUS_PATH,
    STATUS_INFO,
    STATUS_HIDE,

    // MAIN WINDOW MENUS //

    // File //
    MAIN_NEW_WINDOW,
    MAIN_ROOT_WINDOW,
    MAIN_SEARCH,
    MAIN_TERMINAL,
    MAIN_ROOT_TERMINAL,
    MAIN_SAVE_SESSION,
    MAIN_SAVE_TABS,
    MAIN_EXIT,

    // VIEW //
    PANEL1_SHOW,
    PANEL2_SHOW,
    PANEL3_SHOW,
    PANEL4_SHOW,
    MAIN_PBAR,
    MAIN_FOCUS_PANEL,
    PANEL_PREV,
    PANEL_NEXT,
    PANEL_HIDE,
    PANEL_1,
    PANEL_2,
    PANEL_3,
    PANEL_4,

    MAIN_AUTO,
    AUTO_INST,
    EVT_START,
    EVT_EXIT,
    AUTO_WIN,

    EVT_WIN_NEW,
    EVT_WIN_FOCUS,
    EVT_WIN_MOVE,
    EVT_WIN_CLICK,
    EVT_WIN_KEY,
    EVT_WIN_CLOSE,

    AUTO_PNL,
    EVT_PNL_FOCUS,
    EVT_PNL_SHOW,
    EVT_PNL_SEL,

    AUTO_TAB,
    EVT_TAB_NEW,
    EVT_TAB_CHDIR,
    EVT_TAB_FOCUS,
    EVT_TAB_CLOSE,

    EVT_DEVICE,
    MAIN_TITLE,
    MAIN_ICON,
    MAIN_FULL,
    MAIN_DESIGN_MODE,
    MAIN_PREFS,
    MAIN_TOOL,
    ROOT_BAR,
    VIEW_THUMB,

    // Plugins //
    PLUG_INSTALL,
    PLUG_IFILE,
    PLUG_COPY,
    PLUG_CFILE,
    PLUG_CVERB,

    // Help //
    MAIN_ABOUT,
    MAIN_DEV,

    // Tasks //
    MAIN_TASKS,

    TASK_MANAGER,

    TASK_COLUMNS,
    TASK_COL_STATUS,
    TASK_COL_COUNT,
    TASK_COL_PATH,
    TASK_COL_FILE,
    TASK_COL_TO,
    TASK_COL_PROGRESS,
    TASK_COL_TOTAL,
    TASK_COL_STARTED,
    TASK_COL_ELAPSED,
    TASK_COL_CURSPEED,
    TASK_COL_CUREST,
    TASK_COL_AVGSPEED,
    TASK_COL_AVGEST,
    TASK_COL_REORDER,

    TASK_STOP,
    TASK_PAUSE,
    TASK_QUE,
    TASK_RESUME,
    TASK_SHOWOUT,

    TASK_ALL,
    TASK_STOP_ALL,
    TASK_PAUSE_ALL,
    TASK_QUE_ALL,
    TASK_RESUME_ALL,

    TASK_SHOW_MANAGER,
    TASK_HIDE_MANAGER,

    TASK_POPUPS,
    TASK_POP_ALL,
    TASK_POP_TOP,
    TASK_POP_ABOVE,
    TASK_POP_STICK,
    TASK_POP_DETAIL,
    TASK_POP_OVER,
    TASK_POP_ERR,

    TASK_ERRORS,
    TASK_ERR_FIRST,
    TASK_ERR_ANY,
    TASK_ERR_CONT,

    TASK_QUEUE,
    TASK_Q_NEW,
    TASK_Q_SMART,
    TASK_Q_PAUSE,

    // PANELS COMMON  //
    DATE_FORMAT,
    CON_OPEN,
    OPEN_EXECUTE,
    OPEN_EDIT,
    OPEN_EDIT_ROOT,
    OPEN_OTHER,
    OPEN_HAND,
    OPEN_ALL,
    OPEN_IN_TAB,

    OPENTAB_NEW,
    OPENTAB_PREV,
    OPENTAB_NEXT,
    OPENTAB_1,
    OPENTAB_2,
    OPENTAB_3,
    OPENTAB_4,
    OPENTAB_5,
    OPENTAB_6,
    OPENTAB_7,
    OPENTAB_8,
    OPENTAB_9,
    OPENTAB_10,

    OPEN_IN_PANEL,
    OPEN_IN_PANELPREV,
    OPEN_IN_PANELNEXT,
    OPEN_IN_PANEL1,
    OPEN_IN_PANEL2,
    OPEN_IN_PANEL3,
    OPEN_IN_PANEL4,

    ARC_EXTRACT,
    ARC_EXTRACTTO,
    ARC_LIST,

    ARC_DEFAULT,
    ARC_DEF_OPEN,
    ARC_DEF_EX,
    ARC_DEF_EXTO,
    ARC_DEF_LIST,
    ARC_DEF_PARENT,
    ARC_DEF_WRITE,
    ARC_CONF2,

    OPEN_NEW,
    NEW_FILE,
    NEW_DIRECTORY,
    NEW_LINK,
    NEW_ARCHIVE,
    TAB_NEW,
    TAB_NEW_HERE,
    NEW_BOOKMARK,
    ARC_DLG,

    NEW_APP,
    CON_GO,

    GO_REFRESH,
    GO_BACK,
    GO_FORWARD,
    GO_UP,
    GO_HOME,
    GO_DEFAULT,
    GO_SET_DEFAULT,
    EDIT_CANON,

    GO_FOCUS,
    FOCUS_PATH_BAR,
    FOCUS_FILELIST,
    FOCUS_DIRTREE,
    FOCUS_BOOK,
    FOCUS_DEVICE,

    GO_TAB,
    TAB_PREV,
    TAB_NEXT,
    TAB_RESTORE,
    TAB_CLOSE,
    TAB_1,
    TAB_2,
    TAB_3,
    TAB_4,
    TAB_5,
    TAB_6,
    TAB_7,
    TAB_8,
    TAB_9,
    TAB_10,

    CON_VIEW,
    VIEW_LIST_STYLE,
    VIEW_COLUMNS,
    VIEW_REORDER_COL,
    RUBBERBAND,

    VIEW_SORTBY,
    SORTBY_NAME,
    SORTBY_SIZE,
    SORTBY_TYPE,
    SORTBY_PERM,
    SORTBY_OWNER,
    SORTBY_DATE,
    SORTBY_ASCEND,
    SORTBY_DESCEND,
    SORTX_ALPHANUM,
    // SORTX_NATURAL,
    SORTX_CASE,
    SORTX_DIRECTORIES,
    SORTX_FILES,
    SORTX_MIX,
    SORTX_HIDFIRST,
    SORTX_HIDLAST,

    VIEW_REFRESH,
    PATH_SEEK,
    PATH_HAND,
    PATH_HELP,
    EDIT_CUT,
    EDIT_COPY,
    EDIT_PASTE,
    EDIT_RENAME,
    EDIT_DELETE,
    EDIT_TRASH,

    EDIT_SUBMENU,
    COPY_NAME,
    COPY_PARENT,
    COPY_PATH,
    PASTE_LINK,
    PASTE_TARGET,
    PASTE_AS,
    COPY_TO,
    COPY_LOC,
    COPY_LOC_LAST,

    COPY_TAB,
    COPY_TAB_PREV,
    COPY_TAB_NEXT,
    COPY_TAB_1,
    COPY_TAB_2,
    COPY_TAB_3,
    COPY_TAB_4,
    COPY_TAB_5,
    COPY_TAB_6,
    COPY_TAB_7,
    COPY_TAB_8,
    COPY_TAB_9,
    COPY_TAB_10,

    COPY_PANEL,
    COPY_PANEL_PREV,
    COPY_PANEL_NEXT,
    COPY_PANEL_1,
    COPY_PANEL_2,
    COPY_PANEL_3,
    COPY_PANEL_4,

    MOVE_TO,
    MOVE_LOC,
    MOVE_LOC_LAST,

    MOVE_TAB,
    MOVE_TAB_PREV,
    MOVE_TAB_NEXT,
    MOVE_TAB_1,
    MOVE_TAB_2,
    MOVE_TAB_3,
    MOVE_TAB_4,
    MOVE_TAB_5,
    MOVE_TAB_6,
    MOVE_TAB_7,
    MOVE_TAB_8,
    MOVE_TAB_9,
    MOVE_TAB_10,

    MOVE_PANEL,
    MOVE_PANEL_PREV,
    MOVE_PANEL_NEXT,
    MOVE_PANEL_1,
    MOVE_PANEL_2,
    MOVE_PANEL_3,
    MOVE_PANEL_4,

    EDIT_HIDE,
    SELECT_ALL,
    SELECT_UN,
    SELECT_INVERT,
    SELECT_PATT,
    EDIT_ROOT,
    ROOT_COPY_LOC,
    ROOT_MOVE2,
    ROOT_TRASH,
    ROOT_DELETE,

    // Properties //
    CON_PROP,
    PROP_INFO,
    PROP_PERM,
    PROP_QUICK,

    PERM_R,
    PERM_RW,
    PERM_RWX,
    PERM_R_R,
    PERM_RW_R,
    PERM_RW_RW,
    PERM_RWXR_X,
    PERM_RWXRWX,
    PERM_R_R_R,
    PERM_RW_R_R,
    PERM_RW_RW_RW,
    PERM_RWXR_R,
    PERM_RWXR_XR_X,
    PERM_RWXRWXRWX,
    PERM_RWXRWXRWT,
    PERM_UNSTICK,
    PERM_STICK,

    PERM_RECURS,
    PERM_GO_W,
    PERM_GO_RWX,
    PERM_UGO_W,
    PERM_UGO_RX,
    PERM_UGO_RWX,

    PROP_ROOT,
    RPERM_RW,
    RPERM_RWX,
    RPERM_RW_R,
    RPERM_RW_RW,
    RPERM_RWXR_X,
    RPERM_RWXRWX,
    RPERM_RW_R_R,
    RPERM_RW_RW_RW,
    RPERM_RWXR_R,
    RPERM_RWXR_XR_X,
    RPERM_RWXRWXRWX,
    RPERM_RWXRWXRWT,
    RPERM_UNSTICK,
    RPERM_STICK,

    RPERM_RECURS,
    RPERM_GO_W,
    RPERM_GO_RWX,
    RPERM_UGO_W,
    RPERM_UGO_RX,
    RPERM_UGO_RWX,

    RPERM_OWN,
    OWN_MYUSER,
    OWN_MYUSER_USERS,
    OWN_USER1,
    OWN_USER1_USERS,
    OWN_USER2,
    OWN_USER2_USERS,
    OWN_ROOT,
    OWN_ROOT_USERS,
    OWN_ROOT_MYUSER,
    OWN_ROOT_USER1,
    OWN_ROOT_USER2,

    OWN_RECURS,
    ROWN_MYUSER,
    ROWN_MYUSER_USERS,
    ROWN_USER1,
    ROWN_USER1_USERS,
    ROWN_USER2,
    ROWN_USER2_USERS,
    ROWN_ROOT,
    ROWN_ROOT_USERS,
    ROWN_ROOT_MYUSER,
    ROWN_ROOT_USER1,
    ROWN_ROOT_USER2,

    // PANELS //
    PANEL_SLIDERS,

    // panel1
    PANEL1_SHOW_TOOLBOX,
    PANEL1_SHOW_DEVMON,
    PANEL1_SHOW_DIRTREE,
    PANEL1_SHOW_BOOK,
    PANEL1_SHOW_SIDEBAR,
    PANEL1_SLIDER_POSITIONS,
    PANEL1_LIST_DETAILED,
    PANEL1_LIST_ICONS,
    PANEL1_LIST_COMPACT,
    PANEL1_LIST_LARGE,
    PANEL1_SHOW_HIDDEN,
    PANEL1_ICON_TAB,
    PANEL1_ICON_STATUS,
    PANEL1_DETCOL_NAME,
    PANEL1_DETCOL_SIZE,
    PANEL1_DETCOL_TYPE,
    PANEL1_DETCOL_PERM,
    PANEL1_DETCOL_OWNER,
    PANEL1_DETCOL_DATE,
    PANEL1_SORT_EXTRA,
    PANEL1_BOOK_FOL,
    PANEL1_TOOL_L,
    PANEL1_TOOL_R,
    PANEL1_TOOL_S,

    // panel2
    PANEL2_SHOW_TOOLBOX,
    PANEL2_SHOW_DEVMON,
    PANEL2_SHOW_DIRTREE,
    PANEL2_SHOW_BOOK,
    PANEL2_SHOW_SIDEBAR,
    PANEL2_SLIDER_POSITIONS,
    PANEL2_LIST_DETAILED,
    PANEL2_LIST_ICONS,
    PANEL2_LIST_COMPACT,
    PANEL2_LIST_LARGE,
    PANEL2_SHOW_HIDDEN,
    PANEL2_ICON_TAB,
    PANEL2_ICON_STATUS,
    PANEL2_DETCOL_NAME,
    PANEL2_DETCOL_SIZE,
    PANEL2_DETCOL_TYPE,
    PANEL2_DETCOL_PERM,
    PANEL2_DETCOL_OWNER,
    PANEL2_DETCOL_DATE,
    PANEL2_SORT_EXTRA,
    PANEL2_BOOK_FOL,
    PANEL2_TOOL_L,
    PANEL2_TOOL_R,
    PANEL2_TOOL_S,

    // panel3
    PANEL3_SHOW_TOOLBOX,
    PANEL3_SHOW_DEVMON,
    PANEL3_SHOW_DIRTREE,
    PANEL3_SHOW_BOOK,
    PANEL3_SHOW_SIDEBAR,
    PANEL3_SLIDER_POSITIONS,
    PANEL3_LIST_DETAILED,
    PANEL3_LIST_ICONS,
    PANEL3_LIST_COMPACT,
    PANEL3_LIST_LARGE,
    PANEL3_SHOW_HIDDEN,
    PANEL3_ICON_TAB,
    PANEL3_ICON_STATUS,
    PANEL3_DETCOL_NAME,
    PANEL3_DETCOL_SIZE,
    PANEL3_DETCOL_TYPE,
    PANEL3_DETCOL_PERM,
    PANEL3_DETCOL_OWNER,
    PANEL3_DETCOL_DATE,
    PANEL3_SORT_EXTRA,
    PANEL3_BOOK_FOL,
    PANEL3_TOOL_L,
    PANEL3_TOOL_R,
    PANEL3_TOOL_S,

    // panel4
    PANEL4_SHOW_TOOLBOX,
    PANEL4_SHOW_DEVMON,
    PANEL4_SHOW_DIRTREE,
    PANEL4_SHOW_BOOK,
    PANEL4_SHOW_SIDEBAR,
    PANEL4_SLIDER_POSITIONS,
    PANEL4_LIST_DETAILED,
    PANEL4_LIST_ICONS,
    PANEL4_LIST_COMPACT,
    PANEL4_LIST_LARGE,
    PANEL4_SHOW_HIDDEN,
    PANEL4_ICON_TAB,
    PANEL4_ICON_STATUS,
    PANEL4_DETCOL_NAME,
    PANEL4_DETCOL_SIZE,
    PANEL4_DETCOL_TYPE,
    PANEL4_DETCOL_PERM,
    PANEL4_DETCOL_OWNER,
    PANEL4_DETCOL_DATE,
    PANEL4_SORT_EXTRA,
    PANEL4_BOOK_FOL,
    PANEL4_TOOL_L,
    PANEL4_TOOL_R,
    PANEL4_TOOL_S,

    // panel modes

    // panel1

    // panel1 mode 0
    PANEL1_SHOW_TOOLBOX_0,
    PANEL1_SHOW_DEVMON_0,
    PANEL1_SHOW_DIRTREE_0,
    PANEL1_SHOW_BOOK_0,
    PANEL1_SHOW_SIDEBAR_0,
    PANEL1_SLIDER_POSITIONS_0,
    PANEL1_LIST_DETAILED_0,
    PANEL1_LIST_ICONS_0,
    PANEL1_LIST_COMPACT_0,
    PANEL1_LIST_LARGE_0,
    PANEL1_SHOW_HIDDEN_0,
    PANEL1_ICON_TAB_0,
    PANEL1_ICON_STATUS_0,
    PANEL1_DETCOL_NAME_0,
    PANEL1_DETCOL_SIZE_0,
    PANEL1_DETCOL_TYPE_0,
    PANEL1_DETCOL_PERM_0,
    PANEL1_DETCOL_OWNER_0,
    PANEL1_DETCOL_DATE_0,
    PANEL1_SORT_EXTRA_0,
    PANEL1_BOOK_FOL_0,
    PANEL1_TOOL_L_0,
    PANEL1_TOOL_R_0,
    PANEL1_TOOL_S_0,

    // panel1 mode 1
    PANEL1_SHOW_TOOLBOX_1,
    PANEL1_SHOW_DEVMON_1,
    PANEL1_SHOW_DIRTREE_1,
    PANEL1_SHOW_BOOK_1,
    PANEL1_SHOW_SIDEBAR_1,
    PANEL1_SLIDER_POSITIONS_1,
    PANEL1_LIST_DETAILED_1,
    PANEL1_LIST_ICONS_1,
    PANEL1_LIST_COMPACT_1,
    PANEL1_LIST_LARGE_1,
    PANEL1_SHOW_HIDDEN_1,
    PANEL1_ICON_TAB_1,
    PANEL1_ICON_STATUS_1,
    PANEL1_DETCOL_NAME_1,
    PANEL1_DETCOL_SIZE_1,
    PANEL1_DETCOL_TYPE_1,
    PANEL1_DETCOL_PERM_1,
    PANEL1_DETCOL_OWNER_1,
    PANEL1_DETCOL_DATE_1,
    PANEL1_SORT_EXTRA_1,
    PANEL1_BOOK_FOL_1,
    PANEL1_TOOL_L_1,
    PANEL1_TOOL_R_1,
    PANEL1_TOOL_S_1,

    // panel1 mode 2
    PANEL1_SHOW_TOOLBOX_2,
    PANEL1_SHOW_DEVMON_2,
    PANEL1_SHOW_DIRTREE_2,
    PANEL1_SHOW_BOOK_2,
    PANEL1_SHOW_SIDEBAR_2,
    PANEL1_SLIDER_POSITIONS_2,
    PANEL1_LIST_DETAILED_2,
    PANEL1_LIST_ICONS_2,
    PANEL1_LIST_COMPACT_2,
    PANEL1_LIST_LARGE_2,
    PANEL1_SHOW_HIDDEN_2,
    PANEL1_ICON_TAB_2,
    PANEL1_ICON_STATUS_2,
    PANEL1_DETCOL_NAME_2,
    PANEL1_DETCOL_SIZE_2,
    PANEL1_DETCOL_TYPE_2,
    PANEL1_DETCOL_PERM_2,
    PANEL1_DETCOL_OWNER_2,
    PANEL1_DETCOL_DATE_2,
    PANEL1_SORT_EXTRA_2,
    PANEL1_BOOK_FOL_2,
    PANEL1_TOOL_L_2,
    PANEL1_TOOL_R_2,
    PANEL1_TOOL_S_2,

    // panel1 mode 3
    PANEL1_SHOW_TOOLBOX_3,
    PANEL1_SHOW_DEVMON_3,
    PANEL1_SHOW_DIRTREE_3,
    PANEL1_SHOW_BOOK_3,
    PANEL1_SHOW_SIDEBAR_3,
    PANEL1_SLIDER_POSITIONS_3,
    PANEL1_LIST_DETAILED_3,
    PANEL1_LIST_ICONS_3,
    PANEL1_LIST_COMPACT_3,
    PANEL1_LIST_LARGE_3,
    PANEL1_SHOW_HIDDEN_3,
    PANEL1_ICON_TAB_3,
    PANEL1_ICON_STATUS_3,
    PANEL1_DETCOL_NAME_3,
    PANEL1_DETCOL_SIZE_3,
    PANEL1_DETCOL_TYPE_3,
    PANEL1_DETCOL_PERM_3,
    PANEL1_DETCOL_OWNER_3,
    PANEL1_DETCOL_DATE_3,
    PANEL1_SORT_EXTRA_3,
    PANEL1_BOOK_FOL_3,
    PANEL1_TOOL_L_3,
    PANEL1_TOOL_R_3,
    PANEL1_TOOL_S_3,

    // panel2

    // panel2 mode 0
    PANEL2_SHOW_TOOLBOX_0,
    PANEL2_SHOW_DEVMON_0,
    PANEL2_SHOW_DIRTREE_0,
    PANEL2_SHOW_BOOK_0,
    PANEL2_SHOW_SIDEBAR_0,
    PANEL2_SLIDER_POSITIONS_0,
    PANEL2_LIST_DETAILED_0,
    PANEL2_LIST_ICONS_0,
    PANEL2_LIST_COMPACT_0,
    PANEL2_LIST_LARGE_0,
    PANEL2_SHOW_HIDDEN_0,
    PANEL2_ICON_TAB_0,
    PANEL2_ICON_STATUS_0,
    PANEL2_DETCOL_NAME_0,
    PANEL2_DETCOL_SIZE_0,
    PANEL2_DETCOL_TYPE_0,
    PANEL2_DETCOL_PERM_0,
    PANEL2_DETCOL_OWNER_0,
    PANEL2_DETCOL_DATE_0,
    PANEL2_SORT_EXTRA_0,
    PANEL2_BOOK_FOL_0,
    PANEL2_TOOL_L_0,
    PANEL2_TOOL_R_0,
    PANEL2_TOOL_S_0,

    // panel2 mode 1
    PANEL2_SHOW_TOOLBOX_1,
    PANEL2_SHOW_DEVMON_1,
    PANEL2_SHOW_DIRTREE_1,
    PANEL2_SHOW_BOOK_1,
    PANEL2_SHOW_SIDEBAR_1,
    PANEL2_SLIDER_POSITIONS_1,
    PANEL2_LIST_DETAILED_1,
    PANEL2_LIST_ICONS_1,
    PANEL2_LIST_COMPACT_1,
    PANEL2_LIST_LARGE_1,
    PANEL2_SHOW_HIDDEN_1,
    PANEL2_ICON_TAB_1,
    PANEL2_ICON_STATUS_1,
    PANEL2_DETCOL_NAME_1,
    PANEL2_DETCOL_SIZE_1,
    PANEL2_DETCOL_TYPE_1,
    PANEL2_DETCOL_PERM_1,
    PANEL2_DETCOL_OWNER_1,
    PANEL2_DETCOL_DATE_1,
    PANEL2_SORT_EXTRA_1,
    PANEL2_BOOK_FOL_1,
    PANEL2_TOOL_L_1,
    PANEL2_TOOL_R_1,
    PANEL2_TOOL_S_1,

    // panel2 mode 2
    PANEL2_SHOW_TOOLBOX_2,
    PANEL2_SHOW_DEVMON_2,
    PANEL2_SHOW_DIRTREE_2,
    PANEL2_SHOW_BOOK_2,
    PANEL2_SHOW_SIDEBAR_2,
    PANEL2_SLIDER_POSITIONS_2,
    PANEL2_LIST_DETAILED_2,
    PANEL2_LIST_ICONS_2,
    PANEL2_LIST_COMPACT_2,
    PANEL2_LIST_LARGE_2,
    PANEL2_SHOW_HIDDEN_2,
    PANEL2_ICON_TAB_2,
    PANEL2_ICON_STATUS_2,
    PANEL2_DETCOL_NAME_2,
    PANEL2_DETCOL_SIZE_2,
    PANEL2_DETCOL_TYPE_2,
    PANEL2_DETCOL_PERM_2,
    PANEL2_DETCOL_OWNER_2,
    PANEL2_DETCOL_DATE_2,
    PANEL2_SORT_EXTRA_2,
    PANEL2_BOOK_FOL_2,
    PANEL2_TOOL_L_2,
    PANEL2_TOOL_R_2,
    PANEL2_TOOL_S_2,

    // panel2 mode 3
    PANEL2_SHOW_TOOLBOX_3,
    PANEL2_SHOW_DEVMON_3,
    PANEL2_SHOW_DIRTREE_3,
    PANEL2_SHOW_BOOK_3,
    PANEL2_SHOW_SIDEBAR_3,
    PANEL2_SLIDER_POSITIONS_3,
    PANEL2_LIST_DETAILED_3,
    PANEL2_LIST_ICONS_3,
    PANEL2_LIST_COMPACT_3,
    PANEL2_LIST_LARGE_3,
    PANEL2_SHOW_HIDDEN_3,
    PANEL2_ICON_TAB_3,
    PANEL2_ICON_STATUS_3,
    PANEL2_DETCOL_NAME_3,
    PANEL2_DETCOL_SIZE_3,
    PANEL2_DETCOL_TYPE_3,
    PANEL2_DETCOL_PERM_3,
    PANEL2_DETCOL_OWNER_3,
    PANEL2_DETCOL_DATE_3,
    PANEL2_SORT_EXTRA_3,
    PANEL2_BOOK_FOL_3,
    PANEL2_TOOL_L_3,
    PANEL2_TOOL_R_3,
    PANEL2_TOOL_S_3,

    // panel3

    // panel3 mode 0
    PANEL3_SHOW_TOOLBOX_0,
    PANEL3_SHOW_DEVMON_0,
    PANEL3_SHOW_DIRTREE_0,
    PANEL3_SHOW_BOOK_0,
    PANEL3_SHOW_SIDEBAR_0,
    PANEL3_SLIDER_POSITIONS_0,
    PANEL3_LIST_DETAILED_0,
    PANEL3_LIST_ICONS_0,
    PANEL3_LIST_COMPACT_0,
    PANEL3_LIST_LARGE_0,
    PANEL3_SHOW_HIDDEN_0,
    PANEL3_ICON_TAB_0,
    PANEL3_ICON_STATUS_0,
    PANEL3_DETCOL_NAME_0,
    PANEL3_DETCOL_SIZE_0,
    PANEL3_DETCOL_TYPE_0,
    PANEL3_DETCOL_PERM_0,
    PANEL3_DETCOL_OWNER_0,
    PANEL3_DETCOL_DATE_0,
    PANEL3_SORT_EXTRA_0,
    PANEL3_BOOK_FOL_0,
    PANEL3_TOOL_L_0,
    PANEL3_TOOL_R_0,
    PANEL3_TOOL_S_0,

    // panel3 mode 1
    PANEL3_SHOW_TOOLBOX_1,
    PANEL3_SHOW_DEVMON_1,
    PANEL3_SHOW_DIRTREE_1,
    PANEL3_SHOW_BOOK_1,
    PANEL3_SHOW_SIDEBAR_1,
    PANEL3_SLIDER_POSITIONS_1,
    PANEL3_LIST_DETAILED_1,
    PANEL3_LIST_ICONS_1,
    PANEL3_LIST_COMPACT_1,
    PANEL3_LIST_LARGE_1,
    PANEL3_SHOW_HIDDEN_1,
    PANEL3_ICON_TAB_1,
    PANEL3_ICON_STATUS_1,
    PANEL3_DETCOL_NAME_1,
    PANEL3_DETCOL_SIZE_1,
    PANEL3_DETCOL_TYPE_1,
    PANEL3_DETCOL_PERM_1,
    PANEL3_DETCOL_OWNER_1,
    PANEL3_DETCOL_DATE_1,
    PANEL3_SORT_EXTRA_1,
    PANEL3_BOOK_FOL_1,
    PANEL3_TOOL_L_1,
    PANEL3_TOOL_R_1,
    PANEL3_TOOL_S_1,

    // panel3 mode 2
    PANEL3_SHOW_TOOLBOX_2,
    PANEL3_SHOW_DEVMON_2,
    PANEL3_SHOW_DIRTREE_2,
    PANEL3_SHOW_BOOK_2,
    PANEL3_SHOW_SIDEBAR_2,
    PANEL3_SLIDER_POSITIONS_2,
    PANEL3_LIST_DETAILED_2,
    PANEL3_LIST_ICONS_2,
    PANEL3_LIST_COMPACT_2,
    PANEL3_LIST_LARGE_2,
    PANEL3_SHOW_HIDDEN_2,
    PANEL3_ICON_TAB_2,
    PANEL3_ICON_STATUS_2,
    PANEL3_DETCOL_NAME_2,
    PANEL3_DETCOL_SIZE_2,
    PANEL3_DETCOL_TYPE_2,
    PANEL3_DETCOL_PERM_2,
    PANEL3_DETCOL_OWNER_2,
    PANEL3_DETCOL_DATE_2,
    PANEL3_SORT_EXTRA_2,
    PANEL3_BOOK_FOL_2,
    PANEL3_TOOL_L_2,
    PANEL3_TOOL_R_2,
    PANEL3_TOOL_S_2,

    // panel3 mode 3
    PANEL3_SHOW_TOOLBOX_3,
    PANEL3_SHOW_DEVMON_3,
    PANEL3_SHOW_DIRTREE_3,
    PANEL3_SHOW_BOOK_3,
    PANEL3_SHOW_SIDEBAR_3,
    PANEL3_SLIDER_POSITIONS_3,
    PANEL3_LIST_DETAILED_3,
    PANEL3_LIST_ICONS_3,
    PANEL3_LIST_COMPACT_3,
    PANEL3_LIST_LARGE_3,
    PANEL3_SHOW_HIDDEN_3,
    PANEL3_ICON_TAB_3,
    PANEL3_ICON_STATUS_3,
    PANEL3_DETCOL_NAME_3,
    PANEL3_DETCOL_SIZE_3,
    PANEL3_DETCOL_TYPE_3,
    PANEL3_DETCOL_PERM_3,
    PANEL3_DETCOL_OWNER_3,
    PANEL3_DETCOL_DATE_3,
    PANEL3_SORT_EXTRA_3,
    PANEL3_BOOK_FOL_3,
    PANEL3_TOOL_L_3,
    PANEL3_TOOL_R_3,
    PANEL3_TOOL_S_3,

    // panel4

    // panel4 mode 0
    PANEL4_SHOW_TOOLBOX_0,
    PANEL4_SHOW_DEVMON_0,
    PANEL4_SHOW_DIRTREE_0,
    PANEL4_SHOW_BOOK_0,
    PANEL4_SHOW_SIDEBAR_0,
    PANEL4_SLIDER_POSITIONS_0,
    PANEL4_LIST_DETAILED_0,
    PANEL4_LIST_ICONS_0,
    PANEL4_LIST_COMPACT_0,
    PANEL4_LIST_LARGE_0,
    PANEL4_SHOW_HIDDEN_0,
    PANEL4_ICON_TAB_0,
    PANEL4_ICON_STATUS_0,
    PANEL4_DETCOL_NAME_0,
    PANEL4_DETCOL_SIZE_0,
    PANEL4_DETCOL_TYPE_0,
    PANEL4_DETCOL_PERM_0,
    PANEL4_DETCOL_OWNER_0,
    PANEL4_DETCOL_DATE_0,
    PANEL4_SORT_EXTRA_0,
    PANEL4_BOOK_FOL_0,
    PANEL4_TOOL_L_0,
    PANEL4_TOOL_R_0,
    PANEL4_TOOL_S_0,

    // panel4 mode 1
    PANEL4_SHOW_TOOLBOX_1,
    PANEL4_SHOW_DEVMON_1,
    PANEL4_SHOW_DIRTREE_1,
    PANEL4_SHOW_BOOK_1,
    PANEL4_SHOW_SIDEBAR_1,
    PANEL4_SLIDER_POSITIONS_1,
    PANEL4_LIST_DETAILED_1,
    PANEL4_LIST_ICONS_1,
    PANEL4_LIST_COMPACT_1,
    PANEL4_LIST_LARGE_1,
    PANEL4_SHOW_HIDDEN_1,
    PANEL4_ICON_TAB_1,
    PANEL4_ICON_STATUS_1,
    PANEL4_DETCOL_NAME_1,
    PANEL4_DETCOL_SIZE_1,
    PANEL4_DETCOL_TYPE_1,
    PANEL4_DETCOL_PERM_1,
    PANEL4_DETCOL_OWNER_1,
    PANEL4_DETCOL_DATE_1,
    PANEL4_SORT_EXTRA_1,
    PANEL4_BOOK_FOL_1,
    PANEL4_TOOL_L_1,
    PANEL4_TOOL_R_1,
    PANEL4_TOOL_S_1,

    // panel4 mode 2
    PANEL4_SHOW_TOOLBOX_2,
    PANEL4_SHOW_DEVMON_2,
    PANEL4_SHOW_DIRTREE_2,
    PANEL4_SHOW_BOOK_2,
    PANEL4_SHOW_SIDEBAR_2,
    PANEL4_SLIDER_POSITIONS_2,
    PANEL4_LIST_DETAILED_2,
    PANEL4_LIST_ICONS_2,
    PANEL4_LIST_COMPACT_2,
    PANEL4_LIST_LARGE_2,
    PANEL4_SHOW_HIDDEN_2,
    PANEL4_ICON_TAB_2,
    PANEL4_ICON_STATUS_2,
    PANEL4_DETCOL_NAME_2,
    PANEL4_DETCOL_SIZE_2,
    PANEL4_DETCOL_TYPE_2,
    PANEL4_DETCOL_PERM_2,
    PANEL4_DETCOL_OWNER_2,
    PANEL4_DETCOL_DATE_2,
    PANEL4_SORT_EXTRA_2,
    PANEL4_BOOK_FOL_2,
    PANEL4_TOOL_L_2,
    PANEL4_TOOL_R_2,
    PANEL4_TOOL_S_2,

    // panel4 mode 3
    PANEL4_SHOW_TOOLBOX_3,
    PANEL4_SHOW_DEVMON_3,
    PANEL4_SHOW_DIRTREE_3,
    PANEL4_SHOW_BOOK_3,
    PANEL4_SHOW_SIDEBAR_3,
    PANEL4_SLIDER_POSITIONS_3,
    PANEL4_LIST_DETAILED_3,
    PANEL4_LIST_ICONS_3,
    PANEL4_LIST_COMPACT_3,
    PANEL4_LIST_LARGE_3,
    PANEL4_SHOW_HIDDEN_3,
    PANEL4_ICON_TAB_3,
    PANEL4_ICON_STATUS_3,
    PANEL4_DETCOL_NAME_3,
    PANEL4_DETCOL_SIZE_3,
    PANEL4_DETCOL_TYPE_3,
    PANEL4_DETCOL_PERM_3,
    PANEL4_DETCOL_OWNER_3,
    PANEL4_DETCOL_DATE_3,
    PANEL4_SORT_EXTRA_3,
    PANEL4_BOOK_FOL_3,
    PANEL4_TOOL_L_3,
    PANEL4_TOOL_R_3,
    PANEL4_TOOL_S_3,

    // speed
    BOOK_NEWTAB,
    BOOK_SINGLE,
    DEV_NEWTAB,
    DEV_SINGLE,

    // dialog
    APP_DLG,
    CONTEXT_DLG,
    FILE_DLG,
    TEXT_DLG,

    // other
    CONFIG_VERSION,
    BOOK_NEW,
    DRAG_ACTION,
    EDITOR,
    ROOT_EDITOR,
    SU_COMMAND,

    // HANDLERS //

    // handlers arc
    HAND_ARC_7Z,
    HAND_ARC_GZ,
    HAND_ARC_RAR,
    HAND_ARC_TAR,
    HAND_ARC_TAR_BZ2,
    HAND_ARC_TAR_GZ,
    HAND_ARC_TAR_LZ4,
    HAND_ARC_TAR_XZ,
    HAND_ARC_TAR_ZST,
    HAND_ARC_LZ4,
    HAND_ARC_XZ,
    HAND_ARC_ZIP,
    HAND_ARC_ZST,

    // handlers file
    HAND_F_ISO,

    // handlers fs
    HAND_FS_DEF,
    HAND_FS_FUSEISO,
    HAND_FS_UDISO,

    // handlers net
    HAND_NET_FTP,
    HAND_NET_FUSE,
    HAND_NET_FUSESMB,
    HAND_NET_GPHOTO,
    HAND_NET_HTTP,
    HAND_NET_IFUSE,
    HAND_NET_MTP,
    HAND_NET_SSH,
    HAND_NET_UDEVIL,
    HAND_NET_UDEVILSMB,

    // other
    TOOL_L,
    TOOL_R,
    TOOL_S,
};

enum class XSetPanel
{
    SHOW,
    SHOW_TOOLBOX,
    SHOW_DEVMON,
    SHOW_DIRTREE,
    SHOW_BOOK,
    SHOW_SIDEBAR,
    SLIDER_POSITIONS,
    LIST_DETAILED,
    LIST_ICONS,
    LIST_COMPACT,
    LIST_LARGE,
    SHOW_HIDDEN,
    ICON_TAB,
    ICON_STATUS,
    DETCOL_NAME,
    DETCOL_SIZE,
    DETCOL_TYPE,
    DETCOL_PERM,
    DETCOL_OWNER,
    DETCOL_DATE,
    SORT_EXTRA,
    BOOK_FOL,
    TOOL_L,
    TOOL_R,
    TOOL_S,
};

enum class XSetVar
{
    S,
    B,
    X,
    Y,
    Z,
    KEY,
    KEYMOD,
    STYLE,
    DESC,
    TITLE,
    MENU_LABEL,
    MENU_LABEL_CUSTOM,
    ICN,
    ICON,
    SHARED_KEY,
    NEXT,
    PREV,
    PARENT,
    CHILD,
    CONTEXT,
    LINE,
    TOOL,
    TASK,
    TASK_POP,
    TASK_ERR,
    TASK_OUT,
    RUN_IN_TERMINAL,
    KEEP_TERMINAL,
    SCROLL_LOCK,
    DISABLE,
    OPENER,
};

#ifdef XSET_MAP_TEST
bool is_in_xset_map_test(XSetName name);
bool is_in_xset_map_test(std::string_view name);
#endif

XSetName xset_get_xsetname_from_name(std::string_view name);
const std::string xset_get_name_from_xsetname(XSetName name);

XSetName xset_get_xsetname_from_panel(panel_t panel, XSetPanel panel_var);
const std::string xset_get_name_from_panel(panel_t panel, XSetPanel name);

XSetName xset_get_xsetname_from_panel_mode(panel_t panel, XSetPanel name, char mode);
const std::string xset_get_name_from_panel_mode(panel_t panel, XSetPanel name, char mode);

XSetVar xset_get_xsetvar_from_name(std::string_view name);
const std::string xset_get_name_from_xsetvar(XSetVar name);
