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

#include <string>
#include <string_view>

#include <format>

#include <filesystem>

#include <array>
#include <vector>

#include <sstream>

#include <chrono>

#include <optional>

#include <glibmm.h>
#include <glibmm/convert.h>

// #include <gdk/gdkx.h>
// #include <X11/Xatom.h>

#include <malloc.h>

#include <cassert>

#include <fmt/format.h>

#include <ztd/ztd.hxx>
#include <ztd/ztd_logger.hxx>

#include "types.hxx"

#include "ptk/ptk-location-view.hxx"

#include "window-reference.hxx"
#include "main-window.hxx"

#include "ptk/ptk-builder.hxx"
#include "ptk/ptk-error.hxx"
#include "ptk/ptk-keyboard.hxx"

#include "pref-dialog.hxx"
#include "ptk/ptk-file-menu.hxx"

#include "xset/xset.hxx"
#include "xset/xset-context.hxx"
#include "xset/xset-custom.hxx"
#include "xset/xset-dialog.hxx"
#include "xset/xset-event-handler.hxx"
#include "xset/xset-plugins.hxx"

#include "settings/app.hxx"
#include "settings/disk-format.hxx"

#include "bookmarks.hxx"
#include "settings.hxx"
#include "item-prop.hxx"
#include "find-files.hxx"

#include "autosave.hxx"

#include "vfs/vfs-user-dirs.hxx"
#include "vfs/vfs-utils.hxx"
#include "vfs/vfs-file-task.hxx"

#include "ptk/ptk-bookmark-view.hxx"
#include "ptk/ptk-clipboard.hxx"
#include "ptk/ptk-handler.hxx"

static void rebuild_menus(MainWindow* main_window);
static void main_window_preference(MainWindow* main_window);

static void main_window_class_init(MainWindowClass* klass);
static void main_window_init(MainWindow* main_window);
static void main_window_finalize(GObject* obj);
static void main_window_get_property(GObject* obj, u32 prop_id, GValue* value, GParamSpec* pspec);
static void main_window_set_property(GObject* obj, u32 prop_id, const GValue* value,
                                     GParamSpec* pspec);
static gboolean main_window_delete_event(GtkWidget* widget, GdkEventAny* event);

static gboolean main_window_window_state_event(GtkWidget* widget, GdkEventWindowState* event);

static void on_folder_notebook_switch_pape(GtkNotebook* notebook, GtkWidget* page, u32 page_num,
                                           void* user_data);
static void on_file_browser_begin_chdir(PtkFileBrowser* file_browser, MainWindow* main_window);
static void on_file_browser_open_item(PtkFileBrowser* file_browser,
                                      const std::filesystem::path& path, PtkOpenAction action,
                                      MainWindow* main_window);
static void on_file_browser_after_chdir(PtkFileBrowser* file_browser, MainWindow* main_window);
static void on_file_browser_content_change(PtkFileBrowser* file_browser, MainWindow* main_window);
static void on_file_browser_sel_change(PtkFileBrowser* file_browser, MainWindow* main_window);
static void on_file_browser_panel_change(PtkFileBrowser* file_browser, MainWindow* main_window);
static bool on_tab_drag_motion(GtkWidget* widget, GdkDragContext* drag_context, i32 x, i32 y,
                               u32 time, PtkFileBrowser* file_browser);
static bool on_main_window_focus(GtkWidget* main_window, GdkEventFocus* event, void* user_data);

static bool on_main_window_keypress(MainWindow* main_window, GdkEventKey* event, xset_t known_set);
static bool on_main_window_keypress_found_key(MainWindow* main_window, xset_t set);
static bool on_window_button_press_event(GtkWidget* widget, GdkEventButton* event,
                                         MainWindow* main_window); // sfm
static void on_new_window_activate(GtkMenuItem* menuitem, void* user_data);
static void main_window_close(MainWindow* main_window);

static void show_panels(GtkMenuItem* item, MainWindow* main_window);

static GtkWidget* main_task_view_new(MainWindow* main_window);
static void on_task_popup_show(GtkMenuItem* item, MainWindow* main_window, const char* name2);
static bool main_tasks_running(MainWindow* main_window);
static void on_task_stop(GtkMenuItem* item, GtkWidget* view, xset_t set2, PtkFileTask* ptask2);
static void on_preference_activate(GtkMenuItem* menuitem, void* user_data);
static void main_task_prepare_menu(MainWindow* main_window, GtkWidget* menu,
                                   GtkAccelGroup* accel_group);
static void on_task_columns_changed(GtkWidget* view, void* user_data);
static PtkFileTask* get_selected_task(GtkWidget* view);
static void main_window_update_status_bar(MainWindow* main_window, PtkFileBrowser* file_browser);
static void set_window_title(MainWindow* main_window, PtkFileBrowser* file_browser);
static void on_task_column_selected(GtkMenuItem* item, GtkWidget* view);
static void on_task_popup_errset(GtkMenuItem* item, MainWindow* main_window, const char* name2);
static void show_task_dialog(GtkWidget* widget, GtkWidget* view);
static void on_about_activate(GtkMenuItem* menuitem, void* user_data);
static void update_window_title(GtkMenuItem* item, MainWindow* main_window);
static void on_fullscreen_activate(GtkMenuItem* menuitem, MainWindow* main_window);
static bool delayed_focus(GtkWidget* widget);
static bool delayed_focus_file_browser(PtkFileBrowser* file_browser);
static bool idle_set_task_height(MainWindow* main_window);

static GtkWindowClass* parent_class = nullptr;

static i32 n_windows = 0;

static std::vector<MainWindow*> all_windows;

static std::map<panel_t, std::vector<std::filesystem::path>> closed_tabs_restore;

//  Drag & Drop/Clipboard targets
static GtkTargetEntry drag_targets[] = {{ztd::strdup("text/uri-list"), 0, 0}};

#define FM_TYPE_MAIN_WINDOW (main_window_get_type())

GType
main_window_get_type()
{
    static GType type = G_TYPE_INVALID;
    if (type == G_TYPE_INVALID)
    {
        static const GTypeInfo info = {
            sizeof(MainWindowClass),
            nullptr,
            nullptr,
            (GClassInitFunc)main_window_class_init,
            nullptr,
            nullptr,
            sizeof(MainWindow),
            0,
            (GInstanceInitFunc)main_window_init,
            nullptr,
        };
        type = g_type_register_static(GTK_TYPE_WINDOW,
                                      "MainWindow",
                                      &info,
                                      GTypeFlags::G_TYPE_FLAG_NONE);
    }
    return type;
}

static void
main_window_class_init(MainWindowClass* klass)
{
    GObjectClass* object_class;
    GtkWidgetClass* widget_class;

    object_class = (GObjectClass*)klass;
    parent_class = (GtkWindowClass*)g_type_class_peek_parent(klass);

    object_class->set_property = main_window_set_property;
    object_class->get_property = main_window_get_property;
    object_class->finalize = main_window_finalize;

    widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->delete_event = main_window_delete_event;
    widget_class->window_state_event = main_window_window_state_event;

    /*  this works but desktop_window does not
    g_signal_new ( "task-notify",
                       G_TYPE_FROM_CLASS ( klass ),
                       GSignalMatchType::G_SIGNAL_RUN_FIRST,
                       0,
                       nullptr, nullptr,
                       g_cclosure_marshal_VOID__POINTER,
                       G_TYPE_NONE, 1, G_TYPE_POINTER );
    */
}

static bool
on_configure_evt_timer(MainWindow* main_window)
{
    if (all_windows.empty())
    {
        return false;
    }

    // verify main_window still valid
    for (MainWindow* window : all_windows)
    {
        if (window == main_window)
        {
            break;
        }
    }

    if (main_window->configure_evt_timer)
    {
        g_source_remove(main_window->configure_evt_timer);
        main_window->configure_evt_timer = 0;
    }
    main_window_event(main_window,
                      event_handler->win_move,
                      xset::name::evt_win_move,
                      0,
                      0,
                      nullptr,
                      0,
                      0,
                      0,
                      true);
    return false;
}

static bool
on_window_configure_event(GtkWindow* window, GdkEvent* event, MainWindow* main_window)
{
    (void)window;
    (void)event;
    // use timer to prevent rapid events during resize
    if ((event_handler->win_move->s || event_handler->win_move->ob2_data) &&
        !main_window->configure_evt_timer)
    {
        main_window->configure_evt_timer =
            g_timeout_add(200, (GSourceFunc)on_configure_evt_timer, main_window);
    }
    return false;
}

static void
on_plugin_install(GtkMenuItem* item, MainWindow* main_window, xset_t set2)
{
    xset_t set;
    std::filesystem::path path;
    std::string msg;
    PluginJob job = PluginJob::INSTALL;

    if (!item)
    {
        set = set2;
    }
    else
    {
        set = xset_get(static_cast<const char*>(g_object_get_data(G_OBJECT(item), "set")));
    }
    if (!set)
    {
        return;
    }

    if (ztd::endswith(set->name, "cfile") || ztd::endswith(set->name, "curl"))
    {
        job = PluginJob::COPY;
    }

    if (ztd::endswith(set->name, "file"))
    {
        std::optional<std::string> default_path = std::nullopt;

        // get file path
        xset_t save = xset_get(xset::name::plug_ifile);
        if (save->s)
        { //&& std::filesystem::is_directory(save->s)
            default_path = xset_get_s(save);
        }
        else
        {
            default_path = xset_get_s(xset::name::go_set_default);
            if (!default_path)
            {
                default_path = "/";
            }
        }
        const auto file = xset_file_dialog(GTK_WIDGET(main_window),
                                           GtkFileChooserAction::GTK_FILE_CHOOSER_ACTION_OPEN,
                                           "Choose Plugin File",
                                           default_path.value(),
                                           std::nullopt);
        if (!file)
        {
            return;
        }
        path = file.value();
        save->s = std::filesystem::path(path).filename();
    }

    std::filesystem::path plug_dir;
    switch (job)
    {
        case PluginJob::INSTALL:
        {
            // install job
            char* filename = ztd::strdup(path.filename());
            char* ext = strstr(filename, ".spacefm-plugin");
            if (!ext)
            {
                ext = strstr(filename, ".tar.gz");
            }
            if (ext)
            {
                ext[0] = '\0';
            }
            char* plug_dir_name = filename;
            if (ext)
            {
                ext[0] = '.';
            }
            if (!plug_dir_name)
            {
                msg = "This plugin's filename is invalid.  Please rename it using "
                      "alpha-numeric ASCII characters and try again.";
                xset_msg_dialog(GTK_WIDGET(main_window),
                                GtkMessageType::GTK_MESSAGE_ERROR,
                                "Invalid Plugin Filename",
                                GtkButtonsType::GTK_BUTTONS_OK,
                                msg);

                std::free(plug_dir_name);
                return;
            }

            plug_dir = std::filesystem::path() / DATADIR / PACKAGE_NAME / "plugins" / plug_dir_name;

            if (std::filesystem::exists(plug_dir))
            {
                msg = std::format(
                    "There is already a plugin installed as '{}'.  Overwrite ?\n\nTip: You can "
                    "also rename this plugin file to install it under a different name.",
                    plug_dir_name);
                const i32 response = xset_msg_dialog(GTK_WIDGET(main_window),
                                                     GtkMessageType::GTK_MESSAGE_WARNING,
                                                     "Overwrite Plugin ?",
                                                     GtkButtonsType::GTK_BUTTONS_YES_NO,
                                                     msg);

                if (response != GtkResponseType::GTK_RESPONSE_YES)
                {
                    std::free(plug_dir_name);
                    return;
                }
            }
            std::free(plug_dir_name);
            break;
        }
        case PluginJob::COPY:
        {
            // copy job
            const auto user_tmp = vfs::user_dirs->program_tmp_dir();
            if (std::filesystem::is_directory(user_tmp))
            {
                xset_msg_dialog(GTK_WIDGET(main_window),
                                GtkMessageType::GTK_MESSAGE_ERROR,
                                "Error Creating Temp Directory",
                                GtkButtonsType::GTK_BUTTONS_OK,
                                "Unable to create temporary directory");
                return;
            }
            while (true)
            {
                plug_dir = user_tmp / ztd::randhex();
                if (!std::filesystem::exists(plug_dir))
                {
                    break;
                }
            }
            break;
        }
        case PluginJob::REMOVE:
        default:
            break;
    }

    install_plugin_file(main_window, nullptr, path, plug_dir, job, nullptr);
}

static GtkWidget*
create_plugins_menu(MainWindow* main_window)
{
    PtkFileBrowser* file_browser =
        PTK_FILE_BROWSER_REINTERPRET(main_window_get_current_file_browser(main_window));
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    GtkWidget* plug_menu = gtk_menu_new();
    if (!file_browser)
    {
        return plug_menu;
    }

    std::vector<xset_t> plugins;

    xset_t set;

    set = xset_get(xset::name::plug_ifile);
    xset_set_cb(set, (GFunc)on_plugin_install, main_window);
    xset_set_ob1(set, "set", set);
    set = xset_get(xset::name::plug_cfile);
    xset_set_cb(set, (GFunc)on_plugin_install, main_window);
    xset_set_ob1(set, "set", set);

    set = xset_get(xset::name::plug_install);
    xset_add_menuitem(file_browser, plug_menu, accel_group, set);
    set = xset_get(xset::name::plug_copy);
    xset_add_menuitem(file_browser, plug_menu, accel_group, set);

    GtkWidget* item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(plug_menu), item);

    plugins = xset_get_plugins();
    for (xset_t plugin : plugins)
    {
        assert(plugin != nullptr);
        xset_add_menuitem(file_browser, plug_menu, accel_group, plugin);
    }
    if (!plugins.empty())
    {
        xset_clear_plugins(plugins);
    }

    gtk_widget_show_all(plug_menu);
    return plug_menu;
}

static void
on_devices_show(GtkMenuItem* item, MainWindow* main_window)
{
    (void)item;
    PtkFileBrowser* file_browser =
        PTK_FILE_BROWSER_REINTERPRET(main_window_get_current_file_browser(main_window));
    if (!file_browser)
    {
        return;
    }
    const xset::main_window_panel mode = main_window->panel_context[file_browser->mypanel - 1];

    xset_set_b_panel_mode(file_browser->mypanel,
                          xset::panel::show_devmon,
                          mode,
                          !file_browser->side_dev);
    update_views_all_windows(nullptr, file_browser);
    if (file_browser->side_dev)
    {
        gtk_widget_grab_focus(GTK_WIDGET(file_browser->side_dev));
    }
}

static GtkWidget*
create_devices_menu(MainWindow* main_window)
{
    PtkFileBrowser* file_browser =
        PTK_FILE_BROWSER_REINTERPRET(main_window_get_current_file_browser(main_window));
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    GtkWidget* dev_menu = gtk_menu_new();
    if (!file_browser)
    {
        return dev_menu;
    }

    xset_t set;

    set = xset_get(xset::name::main_dev);
    xset_set_cb(set, (GFunc)on_devices_show, main_window);
    set->b = file_browser->side_dev ? xset::b::xtrue : xset::b::unset;
    xset_add_menuitem(file_browser, dev_menu, accel_group, set);

    set = xset_get(xset::name::separator);
    xset_add_menuitem(file_browser, dev_menu, accel_group, set);

    ptk_location_view_dev_menu(GTK_WIDGET(file_browser), file_browser, dev_menu);

    set = xset_get(xset::name::separator);
    xset_add_menuitem(file_browser, dev_menu, accel_group, set);

    set = xset_get(xset::name::dev_menu_settings);
    xset_add_menuitem(file_browser, dev_menu, accel_group, set);

    // show all
    gtk_widget_show_all(dev_menu);

    return dev_menu;
}

static void
on_open_url(GtkWidget* widget, MainWindow* main_window)
{
    (void)widget;
    PtkFileBrowser* file_browser =
        PTK_FILE_BROWSER_REINTERPRET(main_window_get_current_file_browser(main_window));
    const auto url = xset_get_s(xset::name::main_save_session);
    if (file_browser && url)
    {
        ptk_location_view_mount_network(file_browser, url.value(), true, true);
    }
}

static void
on_find_file_activate(GtkMenuItem* menuitem, void* user_data)
{
    (void)menuitem;
    MainWindow* main_window = MAIN_WINDOW(user_data);
    PtkFileBrowser* file_browser =
        PTK_FILE_BROWSER_REINTERPRET(main_window_get_current_file_browser(main_window));
    const auto cwd = ptk_file_browser_get_cwd(file_browser);

    const std::vector<std::filesystem::path> search_dirs{cwd};

    find_files(search_dirs);
}

static void
on_open_current_folder_as_root(GtkMenuItem* menuitem, void* user_data)
{
    (void)menuitem;
    MainWindow* main_window = MAIN_WINDOW(user_data);
    PtkFileBrowser* file_browser =
        PTK_FILE_BROWSER_REINTERPRET(main_window_get_current_file_browser(main_window));
    if (!file_browser)
    {
        return;
    }
    // root task
    PtkFileTask* ptask = ptk_file_exec_new("Open Root Window",
                                           ptk_file_browser_get_cwd(file_browser),
                                           GTK_WIDGET(file_browser),
                                           file_browser->task_view);
    const std::string exe = ztd::program::exe();
    const std::string cwd = ztd::shell::quote(ptk_file_browser_get_cwd(file_browser).string());
    ptask->task->exec_command = std::format("HOME=/root {} {}", exe, cwd);
    ptask->task->exec_as_user = "root";
    ptask->task->exec_sync = false;
    ptask->task->exec_export = false;
    ptask->task->exec_browser = nullptr;
    ptk_file_task_run(ptask);
}

static void
main_window_open_terminal(MainWindow* main_window, bool as_root)
{
    PtkFileBrowser* file_browser =
        PTK_FILE_BROWSER_REINTERPRET(main_window_get_current_file_browser(main_window));
    if (!file_browser)
    {
        return;
    }
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(file_browser));
    auto main_term = xset_get_s(xset::name::main_terminal);
    if (!main_term)
    {
        ptk_show_error(GTK_WINDOW(parent),
                       "Terminal Not Available",
                       "Please set your terminal program in View|Preferences|Advanced");
        edit_preference(GTK_WINDOW(parent), PrefDlgPage::PREF_ADVANCED);
        main_term = xset_get_s(xset::name::main_terminal);
        if (!main_term)
        {
            return;
        }
    }

    // task
    PtkFileTask* ptask = ptk_file_exec_new("Open Terminal",
                                           ptk_file_browser_get_cwd(file_browser),
                                           GTK_WIDGET(file_browser),
                                           file_browser->task_view);

    const std::string terminal = Glib::find_program_in_path(main_term.value());
    if (terminal.empty())
    {
        ztd::logger::warn("Cannot locate terminal in $PATH : {}", main_term.value());
        return;
    }

    ptask->task->exec_command = terminal;
    if (as_root)
    {
        ptask->task->exec_as_user = "root";
    }
    ptask->task->exec_sync = false;
    ptask->task->exec_export = true;
    ptask->task->exec_browser = file_browser;
    ptk_file_task_run(ptask);
}

static void
on_open_terminal_activate(GtkMenuItem* menuitem, void* user_data)
{
    (void)menuitem;
    MainWindow* main_window = MAIN_WINDOW(user_data);
    main_window_open_terminal(main_window, false);
}

static void
on_open_root_terminal_activate(GtkMenuItem* menuitem, void* user_data)
{
    (void)menuitem;
    MainWindow* main_window = MAIN_WINDOW(user_data);
    main_window_open_terminal(main_window, true);
}

static void
on_quit_activate(GtkMenuItem* menuitem, void* user_data)
{
    (void)menuitem;
    main_window_delete_event(GTK_WIDGET(user_data), nullptr);
    // main_window_close( GTK_WIDGET( user_data ) );
}

void
main_window_rubberband_all()
{
    for (MainWindow* window : all_windows)
    {
        for (const panel_t p : PANELS)
        {
            GtkWidget* notebook = window->panel[p - 1];
            const i32 num_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
            for (const auto i : ztd::range(num_pages))
            {
                PtkFileBrowser* a_browser = PTK_FILE_BROWSER_REINTERPRET(
                    gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), i));
                if (a_browser->view_mode == PtkFBViewMode::PTK_FB_LIST_VIEW)
                {
                    gtk_tree_view_set_rubber_banding(GTK_TREE_VIEW(a_browser->folder_view),
                                                     xset_get_b(xset::name::rubberband));
                }
            }
        }
    }
}

void
main_window_refresh_all()
{
    for (MainWindow* window : all_windows)
    {
        for (const panel_t p : PANELS)
        {
            const i64 notebook = (i64)window->panel[p - 1];
            const i32 num_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
            for (const auto i : ztd::range(num_pages))
            {
                PtkFileBrowser* a_browser = PTK_FILE_BROWSER_REINTERPRET(
                    gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), i));
                ptk_file_browser_refresh(nullptr, a_browser);
            }
        }
    }
}

static void
update_window_icon(GtkWindow* window, GtkIconTheme* theme)
{
    std::string name;
    GError* error = nullptr;

    xset_t set = xset_get(xset::name::main_icon);
    if (set->icon)
    {
        name = set->icon.value();
    }
    else
    {
        name = "spacefm";
    }

    GdkPixbuf* icon =
        gtk_icon_theme_load_icon(theme, name.c_str(), 48, (GtkIconLookupFlags)0, &error);
    if (icon)
    {
        gtk_window_set_icon(window, icon);
        g_object_unref(icon);
    }
    else if (error != nullptr)
    {
        // An error occured on loading the icon
        ztd::logger::error("Unable to load the window icon '{}' in - update_window_icon - {}",
                           name,
                           error->message);
        g_error_free(error);
    }
}

static void
on_main_icon()
{
    GtkIconTheme* icon_theme = gtk_icon_theme_get_default();

    for (MainWindow* window : all_windows)
    {
        update_window_icon(GTK_WINDOW(window), icon_theme);
    }
}

static void
main_design_mode(GtkMenuItem* menuitem, MainWindow* main_window)
{
    (void)menuitem;
    xset_msg_dialog(
        GTK_WIDGET(main_window),
        GtkMessageType::GTK_MESSAGE_INFO,
        "Design Mode Help",
        GtkButtonsType::GTK_BUTTONS_OK,
        "Design Mode allows you to change the name, shortcut key and icon of menu, toolbar and "
        "bookmark items, show help for an item, and add your own custom commands and "
        "applications.\n\nTo open the Design Menu, simply right-click on a menu item, bookmark, "
        "or toolbar item.  To open the Design Menu for a submenu, first close the submenu (by "
        "clicking on it).\n\nFor more information, click the Help button below.");
}

void
main_window_close_all_invalid_tabs()
{
    // do all windows all panels all tabs
    for (MainWindow* window : all_windows)
    {
        for (const panel_t p : PANELS)
        {
            GtkWidget* notebook = window->panel[p - 1];
            const i32 pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
            for (const auto cur_tabx : ztd::range(pages))
            {
                PtkFileBrowser* browser = PTK_FILE_BROWSER_REINTERPRET(
                    gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), cur_tabx));

                // will close all tabs that no longer exist on the filesystem
                ptk_file_browser_refresh(nullptr, browser);
            }
        }
    }
}

void
main_window_refresh_all_tabs_matching(const std::filesystem::path& path)
{
    (void)path;
    // This function actually closes the tabs because refresh does not work.
    // dir objects have multiple refs and unreffing them all would not finalize
    // the dir object for unknown reason.

    // This breaks auto open of tabs on automount
}

void
main_window_rebuild_all_toolbars(PtkFileBrowser* file_browser)
{
    // ztd::logger::info("main_window_rebuild_all_toolbars");

    // do this browser first
    if (file_browser)
    {
        ptk_file_browser_rebuild_toolbars(file_browser);
    }

    // do all windows all panels all tabs
    for (MainWindow* window : all_windows)
    {
        for (const panel_t p : PANELS)
        {
            GtkWidget* notebook = window->panel[p - 1];
            const i32 pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
            for (const auto cur_tabx : ztd::range(pages))
            {
                PtkFileBrowser* a_browser = PTK_FILE_BROWSER_REINTERPRET(
                    gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), cur_tabx));
                if (a_browser != file_browser)
                {
                    ptk_file_browser_rebuild_toolbars(a_browser);
                }
            }
        }
    }
    autosave_request_add();
}

void
update_views_all_windows(GtkWidget* item, PtkFileBrowser* file_browser)
{
    (void)item;
    // ztd::logger::info("update_views_all_windows");
    // do this browser first
    if (!file_browser)
    {
        return;
    }
    const panel_t p = file_browser->mypanel;

    ptk_file_browser_update_views(nullptr, file_browser);

    // do other windows
    for (MainWindow* window : all_windows)
    {
        if (gtk_widget_get_visible(window->panel[p - 1]))
        {
            GtkWidget* notebook = window->panel[p - 1];
            const i32 cur_tabx = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
            if (cur_tabx != -1)
            {
                PtkFileBrowser* a_browser = PTK_FILE_BROWSER_REINTERPRET(
                    gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), cur_tabx));
                if (a_browser != file_browser)
                {
                    ptk_file_browser_update_views(nullptr, a_browser);
                }
            }
        }
    }
    autosave_request_add();
}

void
main_window_toggle_thumbnails_all_windows()
{
    // toggle
    app_settings.set_show_thumbnail(!app_settings.get_show_thumbnail());

    // update all windows/all panels/all browsers
    for (MainWindow* window : all_windows)
    {
        for (const panel_t p : PANELS)
        {
            GtkNotebook* notebook = GTK_NOTEBOOK(window->panel[p - 1]);
            const i32 num_pages = gtk_notebook_get_n_pages(notebook);
            for (const auto i : ztd::range(num_pages))
            {
                PtkFileBrowser* file_browser =
                    PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(notebook, i));
                ptk_file_browser_show_thumbnails(
                    file_browser,
                    app_settings.get_show_thumbnail() ? app_settings.get_max_thumb_size() : 0);
            }
        }
    }

    /* Ensuring free space at the end of the heap is freed to the OS,
     * mainly to deal with the possibility thousands of large thumbnails
     * have been freed but the memory not actually released by SpaceFM */
#if defined(__GLIBC__)
    malloc_trim(0);
#endif
}

void
focus_panel(GtkMenuItem* item, void* mw, panel_t p)
{
    panel_t panel;
    panel_t hidepanel;
    panel_t panel_num;

    MainWindow* main_window = MAIN_WINDOW(mw);

    if (item)
    {
        panel_num = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "panel_num"));
    }
    else
    {
        panel_num = p;
    }

    switch (panel_num)
    {
        case panel_control_code_prev:
            // prev
            panel = main_window->curpanel - 1;
            do
            {
                if (panel < 1)
                {
                    panel = 4;
                }
                if (xset_get_b_panel(panel, xset::panel::show))
                {
                    break;
                }
                panel--;
            } while (panel != main_window->curpanel - 1);
            break;
        case panel_control_code_next:
            // next
            panel = main_window->curpanel + 1;
            do
            {
                if (!valid_panel(panel))
                {
                    panel = 1;
                }
                if (xset_get_b_panel(panel, xset::panel::show))
                {
                    break;
                }
                panel++;
            } while (panel != main_window->curpanel + 1);
            break;
        case panel_control_code_hide:
            // hide
            hidepanel = main_window->curpanel;
            panel = main_window->curpanel + 1;
            do
            {
                if (!valid_panel(panel))
                {
                    panel = 1;
                }
                if (xset_get_b_panel(panel, xset::panel::show))
                {
                    break;
                }
                panel++;
            } while (panel != hidepanel);
            if (panel == hidepanel)
            {
                panel = 0;
            }
            break;
        default:
            panel = panel_num;
            break;
    }

    if (panel > 0 && panel < 5)
    {
        if (gtk_widget_get_visible(main_window->panel[panel - 1]))
        {
            gtk_widget_grab_focus(GTK_WIDGET(main_window->panel[panel - 1]));
            main_window->curpanel = panel;
            main_window->notebook = main_window->panel[panel - 1];
            PtkFileBrowser* file_browser =
                PTK_FILE_BROWSER_REINTERPRET(main_window_get_current_file_browser(main_window));
            if (file_browser)
            {
                gtk_widget_grab_focus(GTK_WIDGET(file_browser->folder_view));
                set_panel_focus(main_window, file_browser);
            }
        }
        else if (panel_num != panel_control_code_hide)
        {
            xset_set_b_panel(panel, xset::panel::show, true);
            show_panels_all_windows(nullptr, main_window);
            gtk_widget_grab_focus(GTK_WIDGET(main_window->panel[panel - 1]));
            main_window->curpanel = panel;
            main_window->notebook = main_window->panel[panel - 1];
            PtkFileBrowser* file_browser =
                PTK_FILE_BROWSER_REINTERPRET(main_window_get_current_file_browser(main_window));
            if (file_browser)
            {
                gtk_widget_grab_focus(GTK_WIDGET(file_browser->folder_view));
                set_panel_focus(main_window, file_browser);
            }
        }
        if (panel_num == panel_control_code_hide)
        {
            xset_set_b_panel(hidepanel, xset::panel::show, false);
            show_panels_all_windows(nullptr, main_window);
        }
    }
}

void
show_panels_all_windows(GtkMenuItem* item, MainWindow* main_window)
{
    (void)item;
    // do this window first
    main_window->panel_change = true;
    show_panels(nullptr, main_window);

    // do other windows
    main_window->panel_change = false; // do not save columns for other windows
    for (MainWindow* window : all_windows)
    {
        if (main_window != window)
        {
            show_panels(nullptr, window);
        }
    }

    autosave_request_add();
}

static void
show_panels(GtkMenuItem* item, MainWindow* main_window)
{
    (void)item;
    i32 cur_tabx;
    std::array<bool, 5> show; // start at 1 for clarity

    // save column widths and side sliders of visible panels
    if (main_window->panel_change)
    {
        for (const panel_t p : PANELS)
        {
            if (gtk_widget_get_visible(GTK_WIDGET(main_window->panel[p - 1])))
            {
                cur_tabx = gtk_notebook_get_current_page(GTK_NOTEBOOK(main_window->panel[p - 1]));
                if (cur_tabx != -1)
                {
                    PtkFileBrowser* file_browser = PTK_FILE_BROWSER_REINTERPRET(
                        gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_window->panel[p - 1]),
                                                  cur_tabx));
                    if (file_browser)
                    {
                        if (file_browser->view_mode == PtkFBViewMode::PTK_FB_LIST_VIEW)
                        {
                            ptk_file_browser_save_column_widths(
                                GTK_TREE_VIEW(file_browser->folder_view),
                                file_browser);
                        }
                        // ptk_file_browser_slider_release( nullptr, nullptr, file_browser );
                    }
                }
            }
        }
    }

    // which panels to show
    for (const panel_t p : PANELS)
    {
        show[p] = xset_get_b_panel(p, xset::panel::show);
    }

    // TODO - write and move this to MainWindow constructor
    if (main_window->panel_context.empty())
    {
        main_window->panel_context = {
            {panel_1, xset::main_window_panel::panel_neither},
            {panel_2, xset::main_window_panel::panel_neither},
            {panel_3, xset::main_window_panel::panel_neither},
            {panel_4, xset::main_window_panel::panel_neither},
        };
    }

    bool horiz;
    bool vert;
    for (const panel_t p : PANELS)
    {
        // panel context - how panels share horiz and vert space with other panels
        switch (p)
        {
            case 1:
                horiz = show[panel_2];
                vert = show[panel_3] || show[panel_4];
                break;
            case 2:
                horiz = show[panel_1];
                vert = show[panel_3] || show[panel_4];
                break;
            case 3:
                horiz = show[panel_4];
                vert = show[panel_1] || show[panel_2];
                break;
            default:
                horiz = show[panel_3];
                vert = show[panel_1] || show[panel_2];
                break;
        }

        if (horiz && vert)
        {
            main_window->panel_context.at(p) = xset::main_window_panel::panel_both;
        }
        else if (horiz)
        {
            main_window->panel_context.at(p) = xset::main_window_panel::panel_horiz;
        }
        else if (vert)
        {
            main_window->panel_context.at(p) = xset::main_window_panel::panel_vert;
        }
        else
        {
            main_window->panel_context.at(p) = xset::main_window_panel::panel_neither;
        }

        if (show[p])
        {
            // shown
            // test if panel and mode exists
            xset_t set;

            const xset::main_window_panel mode = main_window->panel_context.at(p);

            set =
                xset_is(xset::get_xsetname_from_panel_mode(p, xset::panel::slider_positions, mode));
            if (!set)
            {
                // ztd::logger::warn("no config for {}, {}", p, INT(mode));

                xset_set_b_panel_mode(p,
                                      xset::panel::show_toolbox,
                                      mode,
                                      xset_get_b_panel(p, xset::panel::show_toolbox));
                xset_set_b_panel_mode(p,
                                      xset::panel::show_devmon,
                                      mode,
                                      xset_get_b_panel(p, xset::panel::show_devmon));
                xset_set_b_panel_mode(p,
                                      xset::panel::show_dirtree,
                                      mode,
                                      xset_get_b_panel(p, xset::panel::show_dirtree));
                xset_set_b_panel_mode(p,
                                      xset::panel::show_sidebar,
                                      mode,
                                      xset_get_b_panel(p, xset::panel::show_sidebar));
                xset_set_b_panel_mode(p,
                                      xset::panel::detcol_name,
                                      mode,
                                      xset_get_b_panel(p, xset::panel::detcol_name));
                xset_set_b_panel_mode(p,
                                      xset::panel::detcol_size,
                                      mode,
                                      xset_get_b_panel(p, xset::panel::detcol_size));
                xset_set_b_panel_mode(p,
                                      xset::panel::detcol_type,
                                      mode,
                                      xset_get_b_panel(p, xset::panel::detcol_type));
                xset_set_b_panel_mode(p,
                                      xset::panel::detcol_perm,
                                      mode,
                                      xset_get_b_panel(p, xset::panel::detcol_perm));
                xset_set_b_panel_mode(p,
                                      xset::panel::detcol_owner,
                                      mode,
                                      xset_get_b_panel(p, xset::panel::detcol_owner));
                xset_set_b_panel_mode(p,
                                      xset::panel::detcol_date,
                                      mode,
                                      xset_get_b_panel(p, xset::panel::detcol_date));
                const xset_t set_old = xset_get_panel(p, xset::panel::slider_positions);
                set = xset_get_panel_mode(p, xset::panel::slider_positions, mode);
                set->x = set_old->x ? set_old->x : "0";
                set->y = set_old->y ? set_old->y : "0";
                set->s = set_old->s ? set_old->s : "0";
            }
            // load dynamic slider positions for this panel context
            main_window->panel_slide_x[p - 1] = set->x ? std::stoi(set->x.value()) : 0;
            main_window->panel_slide_y[p - 1] = set->y ? std::stoi(set->y.value()) : 0;
            main_window->panel_slide_s[p - 1] = set->s ? std::stoi(set->s.value()) : 0;
            // ztd::logger::info("loaded panel {}", p);
            if (!gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_window->panel[p - 1])))
            {
                main_window->notebook = main_window->panel[p - 1];
                main_window->curpanel = p;
                // load saved tabs
                bool tab_added = false;
                set = xset_get_panel(p, xset::panel::show);
                if ((set->s && app_settings.get_load_saved_tabs()) || set->ob1)
                {
                    // set->ob1 is preload path

                    const std::string tabs_add = std::format(
                        "{}{}{}",
                        set->s && app_settings.get_load_saved_tabs() ? set->s.value() : "",
                        set->ob1 ? CONFIG_FILE_TABS_DELIM : "",
                        set->ob1 ? set->ob1 : "");

                    const std::vector<std::string> tab_dirs =
                        ztd::split(tabs_add, CONFIG_FILE_TABS_DELIM);

                    for (const std::string_view tab_dir : tab_dirs)
                    {
                        if (tab_dir.empty())
                        {
                            continue;
                        }

                        std::filesystem::path folder_path;
                        if (std::filesystem::is_directory(tab_dir))
                        {
                            folder_path = tab_dir;
                        }
                        else
                        {
                            folder_path = vfs::user_dirs->home_dir();
                        }
                        main_window_add_new_tab(main_window, folder_path);
                        tab_added = true;
                    }
                    if (set->x && !set->ob1)
                    {
                        // set current tab
                        cur_tabx = std::stoi(set->x.value());
                        if (cur_tabx >= 0 && cur_tabx < gtk_notebook_get_n_pages(GTK_NOTEBOOK(
                                                            main_window->panel[p - 1])))
                        {
                            gtk_notebook_set_current_page(GTK_NOTEBOOK(main_window->panel[p - 1]),
                                                          cur_tabx);
                            PtkFileBrowser* file_browser = PTK_FILE_BROWSER_REINTERPRET(
                                gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_window->panel[p - 1]),
                                                          cur_tabx));
                            // if (file_browser->folder_view)
                            //      gtk_widget_grab_focus(file_browser->folder_view);
                            // ztd::logger::info("call delayed (showpanels) #{} {:p} window={:p}",
                            // cur_tabx, fmt::ptr(file_browser->folder_view),
                            // fmt::ptr(main_window));
                            g_idle_add((GSourceFunc)delayed_focus, file_browser->folder_view);
                        }
                    }
                    std::free(set->ob1);
                    set->ob1 = nullptr;
                }
                if (!tab_added)
                {
                    // open default tab
                    std::filesystem::path folder_path;
                    const auto default_path = xset_get_s(xset::name::go_set_default);
                    if (default_path)
                    {
                        folder_path = default_path.value();
                    }
                    else
                    {
                        folder_path = vfs::user_dirs->home_dir();
                    }
                    main_window_add_new_tab(main_window, folder_path);
                }
            }
            if ((event_handler->pnl_show->s || event_handler->pnl_show->ob2_data) &&
                !gtk_widget_get_visible(GTK_WIDGET(main_window->panel[p - 1])))
            {
                main_window_event(main_window,
                                  event_handler->pnl_show,
                                  xset::name::evt_pnl_show,
                                  p,
                                  0,
                                  nullptr,
                                  0,
                                  0,
                                  0,
                                  true);
            }
            gtk_widget_show(GTK_WIDGET(main_window->panel[p - 1]));
        }
        else
        {
            // not shown
            if ((event_handler->pnl_show->s || event_handler->pnl_show->ob2_data) &&
                gtk_widget_get_visible(GTK_WIDGET(main_window->panel[p - 1])))
            {
                main_window_event(main_window,
                                  event_handler->pnl_show,
                                  xset::name::evt_pnl_show,
                                  p,
                                  0,
                                  nullptr,
                                  0,
                                  0,
                                  0,
                                  false);
            }
            gtk_widget_hide(GTK_WIDGET(main_window->panel[p - 1]));
        }
    }
    if (show[panel_1] || show[panel_2])
    {
        gtk_widget_show(GTK_WIDGET(main_window->hpane_top));
    }
    else
    {
        gtk_widget_hide(GTK_WIDGET(main_window->hpane_top));
    }
    if (show[panel_3] || show[panel_4])
    {
        gtk_widget_show(GTK_WIDGET(main_window->hpane_bottom));
    }
    else
    {
        gtk_widget_hide(GTK_WIDGET(main_window->hpane_bottom));
    }

    // current panel hidden?
    if (!xset_get_b_panel(main_window->curpanel, xset::panel::show))
    {
        panel_t p = main_window->curpanel + 1;
        if (!valid_panel(p))
        {
            p = 1;
        }
        while (p != main_window->curpanel)
        {
            if (xset_get_b_panel(p, xset::panel::show))
            {
                main_window->curpanel = p;
                main_window->notebook = main_window->panel[p - 1];
                cur_tabx = gtk_notebook_get_current_page(GTK_NOTEBOOK(main_window->notebook));
                PtkFileBrowser* file_browser = PTK_FILE_BROWSER_REINTERPRET(
                    gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_window->notebook), cur_tabx));
                if (!file_browser)
                {
                    continue;
                }
                // if (file_browser->folder_view)
                gtk_widget_grab_focus(file_browser->folder_view);
                break;
            }
            p++;
            if (!valid_panel(p))
            {
                p = 1;
            }
        }
    }
    set_panel_focus(main_window, nullptr);

    // update views all panels
    for (const panel_t p : PANELS)
    {
        if (show[p])
        {
            cur_tabx = gtk_notebook_get_current_page(GTK_NOTEBOOK(main_window->panel[p - 1]));
            if (cur_tabx != -1)
            {
                PtkFileBrowser* file_browser = PTK_FILE_BROWSER_REINTERPRET(
                    gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_window->panel[p - 1]), cur_tabx));
                if (file_browser)
                {
                    ptk_file_browser_update_views(nullptr, file_browser);
                }
            }
        }
    }
}

static bool
on_menu_bar_event(GtkWidget* widget, GdkEvent* event, MainWindow* main_window)
{
    (void)widget;
    (void)event;
    rebuild_menus(main_window);
    return false;
}

static bool
bookmark_menu_keypress(GtkWidget* widget, GdkEventKey* event, void* user_data)
{
    (void)event;
    (void)user_data;

    GtkWidget* item = widget;

    if (item)
    {
        const std::string file_path =
            static_cast<const char*>(g_object_get_data(G_OBJECT(item), "path"));

        if (file_path.empty())
        {
            return false;
        }

        const auto file_browser =
            static_cast<PtkFileBrowser*>(g_object_get_data(G_OBJECT(item), "file_browser"));
        const auto main_window = static_cast<MainWindow*>(file_browser->main_window);

        main_window_add_new_tab(main_window, file_path);

        return true;
    }

    return false;
}

static GtkWidget*
bookmark_add_menuitem(PtkFileBrowser* file_browser, GtkWidget* menu)
{
    GtkWidget* item = nullptr;

    // Add All Bookmarks
    for (auto [book_path, book_name] : get_all_bookmarks())
    {
        item = gtk_menu_item_new_with_label(book_path.c_str());

        g_object_set_data(G_OBJECT(item), "file_browser", file_browser);
        g_object_set_data(G_OBJECT(item), "path", ztd::strdup(book_path));
        g_object_set_data(G_OBJECT(item), "name", ztd::strdup(book_name));

        g_signal_connect(item, "activate", G_CALLBACK(bookmark_menu_keypress), nullptr);

        gtk_widget_set_sensitive(item, true);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    }

    return item;
}

static void
rebuild_menus(MainWindow* main_window)
{
    GtkWidget* newmenu;
    std::string menu_elements;
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    xset_t set;
    xset_t child_set;

    // ztd::logger::info("rebuild_menus");
    PtkFileBrowser* file_browser =
        PTK_FILE_BROWSER_REINTERPRET(main_window_get_current_file_browser(main_window));
    if (!file_browser)
    {
        return;
    }
    const xset_context_t context = xset_context_new();
    main_context_fill(file_browser, context);

    // File
    newmenu = gtk_menu_new();
    xset_set_cb(xset::name::main_new_window, (GFunc)on_new_window_activate, main_window);
    xset_set_cb(xset::name::main_root_window, (GFunc)on_open_current_folder_as_root, main_window);
    xset_set_cb(xset::name::main_search, (GFunc)on_find_file_activate, main_window);
    xset_set_cb(xset::name::main_terminal, (GFunc)on_open_terminal_activate, main_window);
    xset_set_cb(xset::name::main_root_terminal, (GFunc)on_open_root_terminal_activate, main_window);
    xset_set_cb(xset::name::main_save_session, (GFunc)on_open_url, main_window);
    xset_set_cb(xset::name::main_exit, (GFunc)on_quit_activate, main_window);
    menu_elements = "main_save_session main_search separator main_terminal main_root_terminal "
                    "main_new_window main_root_window separator main_save_tabs separator main_exit";
    xset_add_menu(file_browser, newmenu, accel_group, menu_elements);
    gtk_widget_show_all(GTK_WIDGET(newmenu));
    g_signal_connect(newmenu, "key-press-event", G_CALLBACK(xset_menu_keypress), nullptr);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(main_window->file_menu_item), newmenu);

    // View
    newmenu = gtk_menu_new();
    xset_set_cb(xset::name::main_prefs, (GFunc)on_preference_activate, main_window);
    xset_set_cb(xset::name::main_full, (GFunc)on_fullscreen_activate, main_window);
    xset_set_cb(xset::name::main_design_mode, (GFunc)main_design_mode, main_window);
    xset_set_cb(xset::name::main_icon, (GFunc)on_main_icon, nullptr);
    xset_set_cb(xset::name::main_title, (GFunc)update_window_title, main_window);

    i32 vis_count = 0;
    for (const panel_t p : PANELS)
    {
        if (xset_get_b_panel(p, xset::panel::show))
        {
            vis_count++;
        }
    }
    if (!vis_count)
    {
        xset_set_b_panel(1, xset::panel::show, true);
        vis_count++;
    }
    set = xset_get(xset::name::panel1_show);
    xset_set_cb(set, (GFunc)show_panels_all_windows, main_window);
    set->disable = (main_window->curpanel == 1 && vis_count == 1);
    set = xset_get(xset::name::panel2_show);
    xset_set_cb(set, (GFunc)show_panels_all_windows, main_window);
    set->disable = (main_window->curpanel == 2 && vis_count == 1);
    set = xset_get(xset::name::panel3_show);
    xset_set_cb(set, (GFunc)show_panels_all_windows, main_window);
    set->disable = (main_window->curpanel == 3 && vis_count == 1);
    set = xset_get(xset::name::panel4_show);
    xset_set_cb(set, (GFunc)show_panels_all_windows, main_window);
    set->disable = (main_window->curpanel == 4 && vis_count == 1);

    set = xset_get(xset::name::panel_prev);
    xset_set_cb(set, (GFunc)focus_panel, main_window);
    xset_set_ob1_int(set, "panel_num", panel_control_code_prev);
    set->disable = (vis_count == 1);
    set = xset_get(xset::name::panel_next);
    xset_set_cb(set, (GFunc)focus_panel, main_window);
    xset_set_ob1_int(set, "panel_num", panel_control_code_next);
    set->disable = (vis_count == 1);
    set = xset_get(xset::name::panel_hide);
    xset_set_cb(set, (GFunc)focus_panel, main_window);
    xset_set_ob1_int(set, "panel_num", panel_control_code_hide);
    set->disable = (vis_count == 1);
    set = xset_get(xset::name::panel_1);
    xset_set_cb(set, (GFunc)focus_panel, main_window);
    xset_set_ob1_int(set, "panel_num", panel_1);
    set->disable = (main_window->curpanel == 1);
    set = xset_get(xset::name::panel_2);
    xset_set_cb(set, (GFunc)focus_panel, main_window);
    xset_set_ob1_int(set, "panel_num", panel_2);
    set->disable = (main_window->curpanel == 2);
    set = xset_get(xset::name::panel_3);
    xset_set_cb(set, (GFunc)focus_panel, main_window);
    xset_set_ob1_int(set, "panel_num", panel_3);
    set->disable = (main_window->curpanel == 3);
    set = xset_get(xset::name::panel_4);
    xset_set_cb(set, (GFunc)focus_panel, main_window);
    xset_set_ob1_int(set, "panel_num", panel_4);
    set->disable = (main_window->curpanel == 4);

    menu_elements = "panel1_show panel2_show panel3_show panel4_show main_focus_panel";
    main_task_prepare_menu(main_window, newmenu, accel_group);
    xset_add_menu(file_browser, newmenu, accel_group, menu_elements);

    // Panel View submenu
    ptk_file_menu_add_panel_view_menu(file_browser, newmenu, accel_group);

    menu_elements = "separator main_tasks main_auto separator main_title main_icon main_full "
                    "separator main_design_mode main_prefs";
    xset_add_menu(file_browser, newmenu, accel_group, menu_elements);
    gtk_widget_show_all(GTK_WIDGET(newmenu));
    g_signal_connect(newmenu, "key-press-event", G_CALLBACK(xset_menu_keypress), nullptr);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(main_window->view_menu_item), newmenu);

    // Devices
    main_window->dev_menu = create_devices_menu(main_window);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(main_window->dev_menu_item), main_window->dev_menu);
    g_signal_connect(main_window->dev_menu,
                     "key-press-event",
                     G_CALLBACK(xset_menu_keypress),
                     nullptr);

    // Bookmarks
    newmenu = gtk_menu_new();
    set = xset_get(xset::name::book_add);
    xset_set_cb(set, (GFunc)ptk_bookmark_view_add_bookmark_cb, file_browser);
    set->disable = false;
    xset_add_menuitem(file_browser, newmenu, accel_group, set);
    gtk_menu_shell_append(GTK_MENU_SHELL(newmenu), gtk_separator_menu_item_new());
    bookmark_add_menuitem(file_browser, newmenu);
    gtk_widget_show_all(GTK_WIDGET(newmenu));
    g_signal_connect(newmenu, "key-press-event", G_CALLBACK(bookmark_menu_keypress), nullptr);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(main_window->book_menu_item), newmenu);

    // Plugins
    main_window->plug_menu = create_plugins_menu(main_window);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(main_window->plug_menu_item), main_window->plug_menu);
    g_signal_connect(main_window->plug_menu,
                     "key-press-event",
                     G_CALLBACK(xset_menu_keypress),
                     nullptr);

    // Tool
    newmenu = gtk_menu_new();
    set = xset_get(xset::name::main_tool);
    if (!set->child)
    {
        child_set = xset_custom_new();
        child_set->menu_label = "New _Command";
        child_set->parent = xset::get_name_from_xsetname(xset::name::main_tool);
        set->child = child_set->name;
    }
    else
    {
        child_set = xset_get(set->child.value());
    }
    xset_add_menuitem(file_browser, newmenu, accel_group, child_set);
    gtk_widget_show_all(GTK_WIDGET(newmenu));
    g_signal_connect(newmenu, "key-press-event", G_CALLBACK(xset_menu_keypress), nullptr);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(main_window->tool_menu_item), newmenu);

    // Help
    newmenu = gtk_menu_new();
    xset_set_cb(xset::name::main_about, (GFunc)on_about_activate, main_window);
    menu_elements = ztd::strdup("main_about");
    xset_add_menu(file_browser, newmenu, accel_group, menu_elements);
    gtk_widget_show_all(GTK_WIDGET(newmenu));
    g_signal_connect(newmenu, "key-press-event", G_CALLBACK(xset_menu_keypress), nullptr);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(main_window->help_menu_item), newmenu);
    // ztd::logger::info("rebuild_menus  DONE");
}

static void
on_main_window_realize(GtkWidget* widget, MainWindow* main_window)
{
    (void)widget;
    // preset the task manager height for no double-resize on first show
    idle_set_task_height(main_window);
}

static void
main_window_init(MainWindow* main_window)
{
    main_window->configure_evt_timer = 0;
    main_window->fullscreen = false;
    main_window->opened_maximized = app_settings.get_maximized();
    main_window->maximized = app_settings.get_maximized();

    /* this is used to limit the scope of gtk_grab and modal dialogs */
    main_window->wgroup = gtk_window_group_new();
    gtk_window_group_add_window(main_window->wgroup, GTK_WINDOW(main_window));

    /* Add to total window count */
    ++n_windows;
    all_windows.emplace_back(main_window);

    WindowReference::increase();

    // g_signal_connect( G_OBJECT( main_window ), "task-notify",
    //                            G_CALLBACK( ptk_file_task_notify_handler ), nullptr );

    /* Start building GUI */
    /*
    NOTE: gtk_window_set_icon_name does not work under some WMs, such as IceWM.
    gtk_window_set_icon_name( GTK_WINDOW( main_window ),
                              "gnome-fs-directory" ); */
    update_window_icon(GTK_WINDOW(main_window), gtk_icon_theme_get_default());

    main_window->main_vbox = gtk_box_new(GtkOrientation::GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(main_window), main_window->main_vbox);

    // Create menu bar
    main_window->accel_group = gtk_accel_group_new();
    main_window->menu_bar = gtk_menu_bar_new();
    GtkWidget* menu_hbox = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(menu_hbox), main_window->menu_bar, true, true, 0);

    gtk_box_pack_start(GTK_BOX(main_window->main_vbox), menu_hbox, false, false, 0);

    main_window->file_menu_item = gtk_menu_item_new_with_mnemonic("_File");
    gtk_menu_shell_append(GTK_MENU_SHELL(main_window->menu_bar), main_window->file_menu_item);

    main_window->view_menu_item = gtk_menu_item_new_with_mnemonic("_View");
    gtk_menu_shell_append(GTK_MENU_SHELL(main_window->menu_bar), main_window->view_menu_item);

    main_window->dev_menu_item = gtk_menu_item_new_with_mnemonic("_Devices");
    gtk_menu_shell_append(GTK_MENU_SHELL(main_window->menu_bar), main_window->dev_menu_item);
    main_window->dev_menu = nullptr;

    main_window->book_menu_item = gtk_menu_item_new_with_mnemonic("_Bookmarks");
    gtk_menu_shell_append(GTK_MENU_SHELL(main_window->menu_bar), main_window->book_menu_item);

    main_window->plug_menu_item = gtk_menu_item_new_with_mnemonic("_Plugins");
    gtk_menu_shell_append(GTK_MENU_SHELL(main_window->menu_bar), main_window->plug_menu_item);
    main_window->plug_menu = nullptr;

    main_window->tool_menu_item = gtk_menu_item_new_with_mnemonic("_Tools");
    gtk_menu_shell_append(GTK_MENU_SHELL(main_window->menu_bar), main_window->tool_menu_item);

    main_window->help_menu_item = gtk_menu_item_new_with_mnemonic("_Help");
    gtk_menu_shell_append(GTK_MENU_SHELL(main_window->menu_bar), main_window->help_menu_item);

    rebuild_menus(main_window);

    /* Create client area */
    main_window->task_vpane = gtk_paned_new(GtkOrientation::GTK_ORIENTATION_VERTICAL);
    main_window->vpane = gtk_paned_new(GtkOrientation::GTK_ORIENTATION_VERTICAL);
    main_window->hpane_top = gtk_paned_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL);
    main_window->hpane_bottom = gtk_paned_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL);

    for (const panel_t p : PANELS)
    {
        const panel_t idx = p - 1;
        main_window->panel[idx] = gtk_notebook_new();
        gtk_notebook_set_show_border(GTK_NOTEBOOK(main_window->panel[idx]), false);
        gtk_notebook_set_scrollable(GTK_NOTEBOOK(main_window->panel[idx]), true);
        g_signal_connect(main_window->panel[idx],
                         "switch-page",
                         G_CALLBACK(on_folder_notebook_switch_pape),
                         main_window);
    }

    main_window->task_scroll = gtk_scrolled_window_new(nullptr, nullptr);

    gtk_paned_pack1(GTK_PANED(main_window->hpane_top), main_window->panel[0], false, true);
    gtk_paned_pack2(GTK_PANED(main_window->hpane_top), main_window->panel[1], true, true);
    gtk_paned_pack1(GTK_PANED(main_window->hpane_bottom), main_window->panel[2], false, true);
    gtk_paned_pack2(GTK_PANED(main_window->hpane_bottom), main_window->panel[3], true, true);

    gtk_paned_pack1(GTK_PANED(main_window->vpane), main_window->hpane_top, false, true);
    gtk_paned_pack2(GTK_PANED(main_window->vpane), main_window->hpane_bottom, true, true);

    gtk_paned_pack1(GTK_PANED(main_window->task_vpane), main_window->vpane, true, true);
    gtk_paned_pack2(GTK_PANED(main_window->task_vpane), main_window->task_scroll, false, true);

    gtk_box_pack_start(GTK_BOX(main_window->main_vbox),
                       GTK_WIDGET(main_window->task_vpane),
                       true,
                       true,
                       0);

    main_window->notebook = main_window->panel[0];
    main_window->curpanel = 1;

    // Task View
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(main_window->task_scroll),
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC,
                                   GtkPolicyType::GTK_POLICY_AUTOMATIC);
    main_window->task_view = main_task_view_new(main_window);
    gtk_container_add(GTK_CONTAINER(main_window->task_scroll), GTK_WIDGET(main_window->task_view));

    gtk_window_set_role(GTK_WINDOW(main_window), "file_manager");

    gtk_widget_show_all(main_window->main_vbox);

    g_signal_connect(G_OBJECT(main_window->file_menu_item),
                     "button-press-event",
                     G_CALLBACK(on_menu_bar_event),
                     main_window);
    g_signal_connect(G_OBJECT(main_window->view_menu_item),
                     "button-press-event",
                     G_CALLBACK(on_menu_bar_event),
                     main_window);
    g_signal_connect(G_OBJECT(main_window->dev_menu_item),
                     "button-press-event",
                     G_CALLBACK(on_menu_bar_event),
                     main_window);
    g_signal_connect(G_OBJECT(main_window->book_menu_item),
                     "button-press-event",
                     G_CALLBACK(on_menu_bar_event),
                     main_window);
    g_signal_connect(G_OBJECT(main_window->plug_menu_item),
                     "button-press-event",
                     G_CALLBACK(on_menu_bar_event),
                     main_window);
    g_signal_connect(G_OBJECT(main_window->tool_menu_item),
                     "button-press-event",
                     G_CALLBACK(on_menu_bar_event),
                     main_window);
    g_signal_connect(G_OBJECT(main_window->help_menu_item),
                     "button-press-event",
                     G_CALLBACK(on_menu_bar_event),
                     main_window);

    // use this OR widget_class->key_press_event = on_main_window_keypress;
    g_signal_connect(G_OBJECT(main_window),
                     "key-press-event",
                     G_CALLBACK(on_main_window_keypress),
                     nullptr);

    g_signal_connect(main_window, "focus-in-event", G_CALLBACK(on_main_window_focus), nullptr);

    g_signal_connect(G_OBJECT(main_window),
                     "configure-event",
                     G_CALLBACK(on_window_configure_event),
                     main_window);

    g_signal_connect(G_OBJECT(main_window),
                     "button-press-event",
                     G_CALLBACK(on_window_button_press_event),
                     main_window);

    g_signal_connect(G_OBJECT(main_window),
                     "realize",
                     G_CALLBACK(on_main_window_realize),
                     main_window);

    main_window->panel_change = false;
    show_panels(nullptr, main_window);

    gtk_widget_hide(GTK_WIDGET(main_window->task_scroll));
    on_task_popup_show(nullptr, main_window, nullptr);

    // show window
    gtk_window_set_default_size(GTK_WINDOW(main_window),
                                app_settings.get_width(),
                                app_settings.get_height());
    if (app_settings.get_maximized())
    {
        gtk_window_maximize(GTK_WINDOW(main_window));
    }
    gtk_widget_show(GTK_WIDGET(main_window));

    // restore panel sliders
    // do this after maximizing/showing window so slider positions are valid
    // in actual window size
    i32 pos = xset_get_int(xset::name::panel_sliders, xset::var::x);
    if (pos < 200)
    {
        pos = 200;
    }
    gtk_paned_set_position(GTK_PANED(main_window->hpane_top), pos);
    pos = xset_get_int(xset::name::panel_sliders, xset::var::y);
    if (pos < 200)
    {
        pos = 200;
    }
    gtk_paned_set_position(GTK_PANED(main_window->hpane_bottom), pos);
    pos = xset_get_int(xset::name::panel_sliders, xset::var::s);
    if (pos < 200)
    {
        pos = -1;
    }
    gtk_paned_set_position(GTK_PANED(main_window->vpane), pos);

    // build the main menu initially, eg for F10 - Note: file_list is nullptr
    // NOT doing this because it slows down the initial opening of the window
    // and shows a stale menu anyway.
    // rebuild_menus( main_window );

    main_window_event(main_window, nullptr, xset::name::evt_win_new, 0, 0, nullptr, 0, 0, 0, true);
}

static void
main_window_finalize(GObject* obj)
{
    ztd::remove(all_windows, MAIN_WINDOW_REINTERPRET(obj));

    --n_windows;

    g_object_unref((MAIN_WINDOW_REINTERPRET(obj))->wgroup);

    WindowReference::decrease();

    G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void
main_window_get_property(GObject* obj, u32 prop_id, GValue* value, GParamSpec* pspec)
{
    (void)obj;
    (void)prop_id;
    (void)value;
    (void)pspec;
}

static void
main_window_set_property(GObject* obj, u32 prop_id, const GValue* value, GParamSpec* pspec)
{
    (void)obj;
    (void)prop_id;
    (void)value;
    (void)pspec;
}

static void
main_window_close(MainWindow* main_window)
{
    /*
    ztd::logger::info("DISC={}", g_signal_handlers_disconnect_by_func(
                            G_OBJECT(main_window),
                            G_CALLBACK(ptk_file_task_notify_handler), nullptr));
    */
    if (event_handler->win_close->s || event_handler->win_close->ob2_data)
    {
        main_window_event(main_window,
                          event_handler->win_close,
                          xset::name::evt_win_close,
                          0,
                          0,
                          nullptr,
                          0,
                          0,
                          0,
                          false);
    }
    gtk_widget_destroy(GTK_WIDGET(main_window));
}

static void
on_abort_tasks_response(GtkDialog* dlg, i32 response, GtkWidget* main_window)
{
    (void)dlg;
    (void)response;
    main_window_close(MAIN_WINDOW_REINTERPRET(main_window));
}

void
main_window_store_positions(MainWindow* main_window)
{
    if (!main_window)
    {
        main_window = main_window_get_last_active();
    }
    if (!main_window)
    {
        return;
    }
    // if the window is not fullscreen (is normal or maximized) save sliders
    // and columns
    if (!main_window->fullscreen)
    {
        // store width/height + sliders
        i32 pos;
        GtkAllocation allocation;
        gtk_widget_get_allocation(GTK_WIDGET(main_window), &allocation);

        if (!main_window->maximized && allocation.width > 0)
        {
            app_settings.set_width(allocation.width);
            app_settings.set_height(allocation.height);
        }
        if (GTK_IS_PANED(main_window->hpane_top))
        {
            pos = gtk_paned_get_position(GTK_PANED(main_window->hpane_top));
            if (pos)
            {
                xset_set(xset::name::panel_sliders, xset::var::x, std::to_string(pos));
            }

            pos = gtk_paned_get_position(GTK_PANED(main_window->hpane_bottom));
            if (pos)
            {
                xset_set(xset::name::panel_sliders, xset::var::y, std::to_string(pos));
            }

            pos = gtk_paned_get_position(GTK_PANED(main_window->vpane));
            if (pos)
            {
                xset_set(xset::name::panel_sliders, xset::var::s, std::to_string(pos));
            }

            if (gtk_widget_get_visible(main_window->task_scroll))
            {
                pos = gtk_paned_get_position(GTK_PANED(main_window->task_vpane));
                if (pos)
                {
                    // save absolute height
                    xset_set(xset::name::task_show_manager,
                             xset::var::x,
                             std::to_string(allocation.height - pos));
                    // ztd::logger::info("CLOS  win {}x{}    task height {}   slider {}",
                    // allocation.width, allocation.height, allocation.height - pos, pos);
                }
            }
        }

        // store task columns
        on_task_columns_changed(main_window->task_view, nullptr);

        // store fb columns
        PtkFileBrowser* a_browser;
        if (main_window->maximized)
        {
            main_window->opened_maximized = true; // force save of columns
        }
        for (const panel_t p : PANELS)
        {
            const i32 page_x =
                gtk_notebook_get_current_page(GTK_NOTEBOOK(main_window->panel[p - 1]));
            if (page_x != -1)
            {
                a_browser = PTK_FILE_BROWSER_REINTERPRET(
                    gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_window->panel[p - 1]), page_x));
                if (a_browser && a_browser->view_mode == PtkFBViewMode::PTK_FB_LIST_VIEW)
                {
                    ptk_file_browser_save_column_widths(GTK_TREE_VIEW(a_browser->folder_view),
                                                        a_browser);
                }
            }
        }
    }
}

static gboolean
main_window_delete_event(GtkWidget* widget, GdkEventAny* event)
{
    (void)event;
    // ztd::logger::info("main_window_delete_event");

    MainWindow* main_window = MAIN_WINDOW_REINTERPRET(widget);

    main_window_store_positions(main_window);

    // save settings
    app_settings.set_maximized(main_window->maximized);
    autosave_request_cancel();
    save_settings(main_window);

    // tasks running?
    if (main_tasks_running(main_window))
    {
        GtkWidget* dlg = gtk_message_dialog_new(GTK_WINDOW(widget),
                                                GtkDialogFlags::GTK_DIALOG_MODAL,
                                                GtkMessageType::GTK_MESSAGE_QUESTION,
                                                GtkButtonsType::GTK_BUTTONS_YES_NO,
                                                "Stop all tasks running in this window?");
        gtk_dialog_set_default_response(GTK_DIALOG(dlg), GtkResponseType::GTK_RESPONSE_NO);

        const i32 response = gtk_dialog_run(GTK_DIALOG(dlg));

        if (response == GtkResponseType::GTK_RESPONSE_YES)
        {
            gtk_widget_destroy(dlg);
            dlg = gtk_message_dialog_new(GTK_WINDOW(widget),
                                         GtkDialogFlags::GTK_DIALOG_MODAL,
                                         GtkMessageType::GTK_MESSAGE_INFO,
                                         GtkButtonsType::GTK_BUTTONS_CLOSE,
                                         "Aborting tasks...");
            g_signal_connect(dlg, "response", G_CALLBACK(on_abort_tasks_response), widget);
            g_signal_connect(dlg, "destroy", G_CALLBACK(gtk_widget_destroy), dlg);
            gtk_widget_show_all(dlg);

            on_task_stop(nullptr,
                         main_window->task_view,
                         xset_get(xset::name::task_stop_all),
                         nullptr);
            while (main_tasks_running(main_window))
            {
                while (g_main_context_pending(nullptr))
                {
                    g_main_context_iteration(nullptr, true);
                }
            }
        }
        else
        {
            gtk_widget_destroy(dlg);
            return true;
        }
    }
    main_window_close(main_window);
    return true;
}

static gboolean
main_window_window_state_event(GtkWidget* widget, GdkEventWindowState* event)
{
    MainWindow* main_window = MAIN_WINDOW_REINTERPRET(widget);

    const bool maximized =
        ((event->new_window_state & GdkWindowState::GDK_WINDOW_STATE_MAXIMIZED) != 0);

    main_window->maximized = maximized;
    app_settings.set_maximized(maximized);

    if (!main_window->maximized)
    {
        if (main_window->opened_maximized)
        {
            main_window->opened_maximized = false;
        }
        show_panels(nullptr, main_window); // restore columns
    }

    return true;
}

char*
main_window_get_tab_cwd(PtkFileBrowser* file_browser, tab_t tab_num)
{
    if (!file_browser)
    {
        return nullptr;
    }
    i32 page_x;
    MainWindow* main_window = MAIN_WINDOW(file_browser->main_window);
    GtkWidget* notebook = main_window->panel[file_browser->mypanel - 1];
    const i32 pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
    const i32 page_num = gtk_notebook_page_num(GTK_NOTEBOOK(notebook), GTK_WIDGET(file_browser));

    switch (tab_num)
    {
        case tab_control_code_prev:
            // prev
            page_x = page_num - 1;
            break;
        case tab_control_code_next:
            // next
            page_x = page_num + 1;
            break;
        default:
            // tab_num starts counting at 1
            page_x = tab_num - 1;
            break;
    }

    if (page_x > -1 && page_x < pages)
    {
        return ztd::strdup(ptk_file_browser_get_cwd(PTK_FILE_BROWSER_REINTERPRET(
            gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), page_x))));
    }

    return nullptr;
}

char*
main_window_get_panel_cwd(PtkFileBrowser* file_browser, panel_t panel_num)
{
    if (!file_browser)
    {
        return nullptr;
    }
    MainWindow* main_window = MAIN_WINDOW(file_browser->main_window);
    panel_t panel_x = file_browser->mypanel;

    switch (panel_num)
    {
        case panel_control_code_prev:
            // prev
            do
            {
                if (--panel_x < 1)
                {
                    panel_x = 4;
                }
                if (panel_x == file_browser->mypanel)
                {
                    return nullptr;
                }
            } while (!gtk_widget_get_visible(main_window->panel[panel_x - 1]));
            break;
        case panel_control_code_next:
            // next
            do
            {
                if (!valid_panel(++panel_x))
                {
                    panel_x = 1;
                }
                if (panel_x == file_browser->mypanel)
                {
                    return nullptr;
                }
            } while (!gtk_widget_get_visible(main_window->panel[panel_x - 1]));
            break;
        default:
            panel_x = panel_num;
            if (!gtk_widget_get_visible(main_window->panel[panel_x - 1]))
            {
                return nullptr;
            }
            break;
    }

    GtkWidget* notebook = main_window->panel[panel_x - 1];
    const i32 page_x = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
    return ztd::strdup(ptk_file_browser_get_cwd(
        PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), page_x))));
}

void
main_window_open_in_panel(PtkFileBrowser* file_browser, panel_t panel_num,
                          const std::filesystem::path& file_path)
{
    if (!file_browser)
    {
        return;
    }
    MainWindow* main_window = MAIN_WINDOW(file_browser->main_window);
    panel_t panel_x = file_browser->mypanel;

    switch (panel_num)
    {
        case panel_control_code_prev:
            // prev
            do
            {
                if (--panel_x < 1)
                {
                    panel_x = 4;
                }
                if (panel_x == file_browser->mypanel)
                {
                    return;
                }
            } while (!gtk_widget_get_visible(main_window->panel[panel_x - 1]));
            break;
        case panel_control_code_next:
            // next
            do
            {
                if (!valid_panel(++panel_x))
                {
                    panel_x = 1;
                }
                if (panel_x == file_browser->mypanel)
                {
                    return;
                }
            } while (!gtk_widget_get_visible(main_window->panel[panel_x - 1]));
            break;
        default:
            panel_x = panel_num;
            break;
    }

    if (!valid_panel(panel_x))
    {
        return;
    }

    // show panel
    if (!gtk_widget_get_visible(main_window->panel[panel_x - 1]))
    {
        xset_set_b_panel(panel_x, xset::panel::show, true);
        show_panels_all_windows(nullptr, main_window);
    }

    // open in tab in panel
    const i32 save_curpanel = main_window->curpanel;

    main_window->curpanel = panel_x;
    main_window->notebook = main_window->panel[panel_x - 1];

    main_window_add_new_tab(main_window, file_path);

    main_window->curpanel = save_curpanel;
    main_window->notebook = main_window->panel[main_window->curpanel - 1];

    // focus original panel
    // while(g_main_context_pending(nullptr))
    //    g_main_context_iteration(nullptr, true);
    // gtk_widget_grab_focus(GTK_WIDGET(main_window->notebook));
    // gtk_widget_grab_focus(GTK_WIDGET(file_browser->folder_view));
    g_idle_add((GSourceFunc)delayed_focus_file_browser, file_browser);
}

bool
main_window_panel_is_visible(PtkFileBrowser* file_browser, panel_t panel)
{
    if (!valid_panel(panel))
    {
        return false;
    }
    MainWindow* main_window = MAIN_WINDOW(file_browser->main_window);
    return gtk_widget_get_visible(main_window->panel[panel - 1]);
}

const std::array<i64, 3>
main_window_get_counts(PtkFileBrowser* file_browser)
{
    if (!file_browser)
    {
        return {0, 0, 0};
    }

    const MainWindow* main_window = MAIN_WINDOW(file_browser->main_window);
    const GtkWidget* notebook = main_window->panel[file_browser->mypanel - 1];
    const tab_t tab_count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));

    // tab_num starts counting from 1
    const tab_t tab_num =
        gtk_notebook_page_num(GTK_NOTEBOOK(notebook), GTK_WIDGET(file_browser)) + 1;
    panel_t panel_count = 0;
    for (const panel_t p : PANELS)
    {
        const panel_t idx = p - 1;
        if (gtk_widget_get_visible(main_window->panel[idx]))
        {
            panel_count++;
        }
    }

    return {panel_count, tab_count, tab_num};
}

void
on_restore_notebook_page(GtkButton* btn, PtkFileBrowser* file_browser)
{
    (void)btn;

    const panel_t panel = file_browser->mypanel;

    if (closed_tabs_restore[panel].empty())
    {
        ztd::logger::info("No tabs to restore for panel {}", panel);
        return;
    }

    const auto file_path = closed_tabs_restore[panel].back();
    closed_tabs_restore[panel].pop_back();
    // ztd::logger::info("on_restore_notebook_page panel={} path={}", panel, file_path);

    // ztd::logger::info("on_restore_notebook_page fb={:p}", fmt::ptr(file_browser));
    if (!GTK_IS_WIDGET(file_browser))
    {
        return;
    }

    MainWindow* main_window = MAIN_WINDOW(file_browser->main_window);
    main_window_add_new_tab(main_window, file_path);
}

void
on_close_notebook_page(GtkButton* btn, PtkFileBrowser* file_browser)
{
    (void)btn;
    PtkFileBrowser* a_browser;

    closed_tabs_restore[file_browser->mypanel].emplace_back(ptk_file_browser_get_cwd(file_browser));
    // ztd::logger::info("on_close_notebook_page path={}",
    // closed_tabs_restore[file_browser->mypanel].back());

    // ztd::logger::info("on_close_notebook_page fb={:p}", fmt::ptr(file_browser));
    if (!GTK_IS_WIDGET(file_browser))
    {
        return;
    }
    GtkNotebook* notebook =
        GTK_NOTEBOOK(gtk_widget_get_ancestor(GTK_WIDGET(file_browser), GTK_TYPE_NOTEBOOK));
    MainWindow* main_window = MAIN_WINDOW(file_browser->main_window);

    main_window->curpanel = file_browser->mypanel;
    main_window->notebook = main_window->panel[main_window->curpanel - 1];

    if (event_handler->tab_close->s || event_handler->tab_close->ob2_data)
    {
        main_window_event(
            main_window,
            event_handler->tab_close,
            xset::name::evt_tab_close,
            file_browser->mypanel,
            gtk_notebook_page_num(GTK_NOTEBOOK(main_window->notebook), GTK_WIDGET(file_browser)) +
                1,
            nullptr,
            0,
            0,
            0,
            false);
    }

    // save solumns and slider positions of tab to be closed
    ptk_file_browser_slider_release(nullptr, nullptr, file_browser);
    ptk_file_browser_save_column_widths(GTK_TREE_VIEW(file_browser->folder_view), file_browser);

    // without this signal blocked, on_close_notebook_page is called while
    // ptk_file_browser_update_views is still in progress causing segfault
    g_signal_handlers_block_matched(main_window->notebook,
                                    GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                    0,
                                    0,
                                    nullptr,
                                    (void*)on_folder_notebook_switch_pape,
                                    nullptr);

    // remove page can also be used to destroy - same result
    // gtk_notebook_remove_page( notebook, gtk_notebook_get_current_page( notebook ) );
    gtk_widget_destroy(GTK_WIDGET(file_browser));

    if (!app_settings.get_always_show_tabs())
    {
        if (gtk_notebook_get_n_pages(notebook) == 1)
        {
            gtk_notebook_set_show_tabs(notebook, false);
        }
    }
    if (gtk_notebook_get_n_pages(notebook) == 0)
    {
        std::filesystem::path path;
        const auto default_path = xset_get_s(xset::name::go_set_default);
        if (default_path)
        {
            path = default_path.value();
        }
        else
        {
            path = vfs::user_dirs->home_dir();
        }
        main_window_add_new_tab(main_window, path);
        a_browser =
            PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 0));
        if (GTK_IS_WIDGET(a_browser))
        {
            ptk_file_browser_update_views(nullptr, a_browser);
        }

        g_signal_handlers_unblock_matched(main_window->notebook,
                                          GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                          0,
                                          0,
                                          nullptr,
                                          (void*)on_folder_notebook_switch_pape,
                                          nullptr);

        update_window_title(nullptr, main_window);
        if (xset_get_b(xset::name::main_save_tabs))
        {
            autosave_request_add();
        }
        return;
    }

    // update view of new current tab
    i32 cur_tabx;
    cur_tabx = gtk_notebook_get_current_page(GTK_NOTEBOOK(main_window->notebook));
    if (cur_tabx != -1)
    {
        a_browser = PTK_FILE_BROWSER_REINTERPRET(
            gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), cur_tabx));

        ptk_file_browser_update_views(nullptr, a_browser);
        if (GTK_IS_WIDGET(a_browser))
        {
            main_window_update_status_bar(main_window, a_browser);
            g_idle_add((GSourceFunc)delayed_focus, a_browser->folder_view);
        }
        if (event_handler->tab_focus->s || event_handler->tab_focus->ob2_data)
        {
            main_window_event(main_window,
                              event_handler->tab_focus,
                              xset::name::evt_tab_focus,
                              main_window->curpanel,
                              cur_tabx + 1,
                              nullptr,
                              0,
                              0,
                              0,
                              false);
        }
    }

    g_signal_handlers_unblock_matched(main_window->notebook,
                                      GSignalMatchType::G_SIGNAL_MATCH_FUNC,
                                      0,
                                      0,
                                      nullptr,
                                      (void*)on_folder_notebook_switch_pape,
                                      nullptr);

    update_window_title(nullptr, main_window);
    if (xset_get_b(xset::name::main_save_tabs))
    {
        autosave_request_add();
    }
}

static bool
notebook_clicked(GtkWidget* widget, GdkEventButton* event,
                 PtkFileBrowser* file_browser) // MOD added
{
    (void)widget;
    on_file_browser_panel_change(file_browser, MAIN_WINDOW(file_browser->main_window));
    if ((event_handler->win_click->s || event_handler->win_click->ob2_data) &&
        main_window_event(file_browser->main_window,
                          event_handler->win_click,
                          xset::name::evt_win_click,
                          0,
                          0,
                          "tabbar",
                          0,
                          event->button,
                          event->state,
                          true))
    {
        return true;
    }
    // middle-click on tab closes
    if (event->type == GdkEventType::GDK_BUTTON_PRESS)
    {
        if (event->button == 2)
        {
            on_close_notebook_page(nullptr, file_browser);
            return true;
        }
        else if (event->button == 3)
        {
            GtkWidget* popup = gtk_menu_new();
            GtkAccelGroup* accel_group = gtk_accel_group_new();
            const xset_context_t context = xset_context_new();
            main_context_fill(file_browser, context);

            xset_t set;

            set = xset_get(xset::name::tab_close);
            xset_set_cb(set, (GFunc)on_close_notebook_page, file_browser);
            xset_add_menuitem(file_browser, popup, accel_group, set);
            set = xset_get(xset::name::tab_restore);
            xset_set_cb(set, (GFunc)on_restore_notebook_page, file_browser);
            xset_add_menuitem(file_browser, popup, accel_group, set);
            set = xset_get(xset::name::tab_new);
            xset_set_cb(set, (GFunc)ptk_file_browser_new_tab, file_browser);
            xset_add_menuitem(file_browser, popup, accel_group, set);
            set = xset_get(xset::name::tab_new_here);
            xset_set_cb(set, (GFunc)ptk_file_browser_new_tab_here, file_browser);
            xset_add_menuitem(file_browser, popup, accel_group, set);
            gtk_widget_show_all(GTK_WIDGET(popup));
            g_signal_connect(popup, "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);
            g_signal_connect(popup, "key-press-event", G_CALLBACK(xset_menu_keypress), nullptr);
            gtk_menu_popup_at_pointer(GTK_MENU(popup), nullptr);
            return true;
        }
    }
    return false;
}

static void
on_file_browser_begin_chdir(PtkFileBrowser* file_browser, MainWindow* main_window)
{
    main_window_update_status_bar(main_window, file_browser);
}

static void
on_file_browser_after_chdir(PtkFileBrowser* file_browser, MainWindow* main_window)
{
    // main_window_stop_busy_task( main_window );

    if (main_window_get_current_file_browser(main_window) == GTK_WIDGET(file_browser))
    {
        set_window_title(main_window, file_browser);
        // gtk_entry_set_text(main_window->address_bar, file_browser->dir->path);
        // gtk_statusbar_push(GTK_STATUSBAR(main_window->status_bar), 0, "");
        // main_window_update_command_ui(main_window, file_browser);
    }

    // main_window_update_tab_label(main_window, file_browser, file_browser->dir->path);

    if (file_browser->inhibit_focus)
    {
        // complete ptk_file_browser.c ptk_file_browser_seek_path()
        file_browser->inhibit_focus = false;
        if (file_browser->seek_name)
        {
            ptk_file_browser_seek_path(file_browser, "", file_browser->seek_name);
            std::free(file_browser->seek_name);
            file_browser->seek_name = nullptr;
        }
    }
    else
    {
        ptk_file_browser_select_last(file_browser);                   // MOD restore last selections
        gtk_widget_grab_focus(GTK_WIDGET(file_browser->folder_view)); // MOD
    }
    if (xset_get_b(xset::name::main_save_tabs))
    {
        autosave_request_add();
    }

    if (event_handler->tab_chdir->s || event_handler->tab_chdir->ob2_data)
    {
        main_window_event(main_window,
                          event_handler->tab_chdir,
                          xset::name::evt_tab_chdir,
                          0,
                          0,
                          nullptr,
                          0,
                          0,
                          0,
                          true);
    }
}

GtkWidget*
main_window_create_tab_label(MainWindow* main_window, PtkFileBrowser* file_browser)
{
    (void)main_window;
    GtkEventBox* evt_box;
    GtkWidget* tab_label;
    GtkWidget* tab_text = nullptr;
    GtkWidget* tab_icon = nullptr;
    GtkWidget* close_btn;
    GtkWidget* close_icon;
    GdkPixbuf* pixbuf = nullptr;

    /* Create tab label */
    evt_box = GTK_EVENT_BOX(gtk_event_box_new());
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(evt_box), false);

    tab_label = gtk_box_new(GtkOrientation::GTK_ORIENTATION_HORIZONTAL, 0);
    xset_t set = xset_get_panel(file_browser->mypanel, xset::panel::icon_tab);
    if (set->icon)
    {
        pixbuf = vfs_load_icon(set->icon.value(), 16);
        if (pixbuf)
        {
            tab_icon = gtk_image_new_from_pixbuf(pixbuf);
            g_object_unref(pixbuf);
        }
        else
        {
            tab_icon = xset_get_image(set->icon.value(), GtkIconSize::GTK_ICON_SIZE_MENU);
        }
    }
    if (!tab_icon)
    {
        tab_icon = gtk_image_new_from_icon_name("gtk-directory", GtkIconSize::GTK_ICON_SIZE_MENU);
    }
    gtk_box_pack_start(GTK_BOX(tab_label), tab_icon, false, false, 4);

    const auto cwd = ptk_file_browser_get_cwd(file_browser);
    if (!cwd.empty())
    {
        const auto name = ptk_file_browser_get_cwd(file_browser).filename();
        tab_text = gtk_label_new(name.c_str());
    }

    gtk_label_set_ellipsize(GTK_LABEL(tab_text), PangoEllipsizeMode::PANGO_ELLIPSIZE_MIDDLE);
    if (std::strlen(gtk_label_get_text(GTK_LABEL(tab_text))) < 30)
    {
        gtk_label_set_ellipsize(GTK_LABEL(tab_text), PangoEllipsizeMode::PANGO_ELLIPSIZE_NONE);
        gtk_label_set_width_chars(GTK_LABEL(tab_text), -1);
    }
    else
    {
        gtk_label_set_width_chars(GTK_LABEL(tab_text), 30);
    }
    gtk_label_set_max_width_chars(GTK_LABEL(tab_text), 30);
    gtk_box_pack_start(GTK_BOX(tab_label), tab_text, false, false, 4);

    if (app_settings.get_show_close_tab_buttons())
    {
        close_btn = gtk_button_new();
        gtk_widget_set_focus_on_click(GTK_WIDGET(close_btn), false);
        gtk_button_set_relief(GTK_BUTTON(close_btn), GTK_RELIEF_NONE);
        close_icon = gtk_image_new_from_icon_name("window-close", GtkIconSize::GTK_ICON_SIZE_MENU);

        gtk_container_add(GTK_CONTAINER(close_btn), close_icon);
        gtk_box_pack_end(GTK_BOX(tab_label), close_btn, false, false, 0);
        g_signal_connect(G_OBJECT(close_btn),
                         "clicked",
                         G_CALLBACK(on_close_notebook_page),
                         file_browser);
    }

    gtk_container_add(GTK_CONTAINER(evt_box), tab_label);

    gtk_widget_set_events(GTK_WIDGET(evt_box), GdkEventMask::GDK_ALL_EVENTS_MASK);
    gtk_drag_dest_set(
        GTK_WIDGET(evt_box),
        GTK_DEST_DEFAULT_ALL,
        drag_targets,
        sizeof(drag_targets) / sizeof(GtkTargetEntry),
        GdkDragAction(GdkDragAction::GDK_ACTION_DEFAULT | GdkDragAction::GDK_ACTION_COPY |
                      GdkDragAction::GDK_ACTION_MOVE | GdkDragAction::GDK_ACTION_LINK));
    g_signal_connect((void*)evt_box, "drag-motion", G_CALLBACK(on_tab_drag_motion), file_browser);

    // MOD  middle-click to close tab
    g_signal_connect(G_OBJECT(evt_box),
                     "button-press-event",
                     G_CALLBACK(notebook_clicked),
                     file_browser);

    gtk_widget_show_all(GTK_WIDGET(evt_box));
    if (!set->icon)
    {
        gtk_widget_hide(tab_icon);
    }
    return GTK_WIDGET(evt_box);
}

void
main_window_update_tab_label(MainWindow* main_window, PtkFileBrowser* file_browser,
                             const std::filesystem::path& path)
{
    GtkWidget* label =
        gtk_notebook_get_tab_label(GTK_NOTEBOOK(main_window->notebook), GTK_WIDGET(file_browser));
    if (!label)
    {
        return;
    }

    GtkContainer* hbox = GTK_CONTAINER(gtk_bin_get_child(GTK_BIN(label)));
    GList* children = gtk_container_get_children(hbox);
    // icon = GTK_IMAGE(children->data);
    GtkLabel* text = GTK_LABEL(children->next->data);

    // TODO: Change the icon

    const std::string name = path.filename();
    gtk_label_set_text(text, name.data());
    gtk_label_set_ellipsize(text, PangoEllipsizeMode::PANGO_ELLIPSIZE_MIDDLE);
    if (name.size() < 30)
    {
        gtk_label_set_ellipsize(text, PangoEllipsizeMode::PANGO_ELLIPSIZE_NONE);
        gtk_label_set_width_chars(text, -1);
    }
    else
    {
        gtk_label_set_width_chars(text, 30);
    }

    g_list_free(children); // sfm 0.6.0 enabled
}

void
main_window_add_new_tab(MainWindow* main_window, const std::filesystem::path& folder_path)
{
    GtkWidget* notebook = main_window->notebook;

    PtkFileBrowser* curfb =
        PTK_FILE_BROWSER_REINTERPRET(main_window_get_current_file_browser(main_window));
    if (GTK_IS_WIDGET(curfb))
    {
        // save sliders of current fb ( new tab while task manager is shown changes vals )
        ptk_file_browser_slider_release(nullptr, nullptr, curfb);
        // save column widths of fb so new tab has same
        ptk_file_browser_save_column_widths(GTK_TREE_VIEW(curfb->folder_view), curfb);
    }
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER_REINTERPRET(
        ptk_file_browser_new(main_window->curpanel, notebook, main_window->task_view, main_window));
    if (!file_browser)
    {
        return;
    }
    // ztd::logger::info("New tab panel={} path={}", main_window->curpanel, folder_path);

    // ztd::logger::info("main_window_add_new_tab fb={:p}", fmt::ptr(file_browser));
    ptk_file_browser_set_single_click(file_browser, app_settings.get_single_click());
    // FIXME: this should not be hard-code
    ptk_file_browser_set_single_click_timeout(file_browser,
                                              app_settings.get_single_hover() ? SINGLE_CLICK_TIMEOUT
                                                                              : 0);
    ptk_file_browser_show_thumbnails(
        file_browser,
        app_settings.get_show_thumbnail() ? app_settings.get_max_thumb_size() : 0);

    ptk_file_browser_set_sort_order(file_browser,
                                    (PtkFBSortOrder)xset_get_int_panel(file_browser->mypanel,
                                                                       xset::panel::list_detailed,
                                                                       xset::var::x));
    ptk_file_browser_set_sort_type(file_browser,
                                   (GtkSortType)xset_get_int_panel(file_browser->mypanel,
                                                                   xset::panel::list_detailed,
                                                                   xset::var::y));

    gtk_widget_show(GTK_WIDGET(file_browser));

    // file_browser->add_event<EventType::CHDIR_BEFORE>(on_file_browser_before_chdir, main_window);
    file_browser->add_event<EventType::CHDIR_BEGIN>(on_file_browser_begin_chdir, main_window);
    file_browser->add_event<EventType::CHDIR_AFTER>(on_file_browser_after_chdir, main_window);
    file_browser->add_event<EventType::OPEN_ITEM>(on_file_browser_open_item, main_window);
    file_browser->add_event<EventType::CHANGE_CONTENT>(on_file_browser_content_change, main_window);
    file_browser->add_event<EventType::CHANGE_SEL>(on_file_browser_sel_change, main_window);
    file_browser->add_event<EventType::CHANGE_PANE>(on_file_browser_panel_change, main_window);

    GtkWidget* tab_label = main_window_create_tab_label(main_window, file_browser);
    const i32 idx =
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), GTK_WIDGET(file_browser), tab_label);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(notebook), GTK_WIDGET(file_browser), true);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), idx);

    if (app_settings.get_always_show_tabs())
    {
        gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), true);
    }
    else if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook)) > 1)
    {
        gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), true);
    }
    else
    {
        gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), false);
    }

    if (!ptk_file_browser_chdir(file_browser,
                                folder_path,
                                PtkFBChdirMode::PTK_FB_CHDIR_ADD_HISTORY))
    {
        ptk_file_browser_chdir(file_browser, "/", PtkFBChdirMode::PTK_FB_CHDIR_ADD_HISTORY);
    }

    if (event_handler->tab_new->s || event_handler->tab_new->ob2_data)
    {
        main_window_event(main_window,
                          event_handler->tab_new,
                          xset::name::evt_tab_new,
                          0,
                          0,
                          nullptr,
                          0,
                          0,
                          0,
                          true);
    }

    set_panel_focus(main_window, file_browser);
    //    while(g_main_context_pending(nullptr))  // wait for chdir to grab focus
    //        g_main_context_iteration(nullptr, true);
    // gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );
    // ztd::logger::info("focus browser {} {}", idx, file_browser->folder_view);
    // ztd::logger::info("call delayed (newtab) #{} {:p}", idx,
    // fmt::ptr(file_browser->folder_view));
    //    g_idle_add( ( GSourceFunc ) delayed_focus, file_browser->folder_view );
}

GtkWidget*
main_window_new()
{
    return GTK_WIDGET(g_object_new(FM_TYPE_MAIN_WINDOW, nullptr));
}

GtkWidget*
main_window_get_current_file_browser(MainWindow* main_window)
{
    if (!main_window)
    {
        main_window = main_window_get_last_active();
        if (!main_window)
        {
            return nullptr;
        }
    }
    if (main_window->notebook)
    {
        const i32 idx = gtk_notebook_get_current_page(GTK_NOTEBOOK(main_window->notebook));
        if (idx >= 0)
        {
            return gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_window->notebook), idx);
        }
    }
    return nullptr;
}

static void
on_preference_activate(GtkMenuItem* menuitem, void* user_data)
{
    (void)menuitem;
    MainWindow* main_window = MAIN_WINDOW(user_data);
    main_window_preference(main_window);
}

static void
main_window_preference(MainWindow* main_window)
{
    edit_preference(GTK_WINDOW(main_window), PrefDlgPage::PREF_GENERAL);
}

static void
on_about_activate(GtkMenuItem* menuitem, void* user_data)
{
    (void)menuitem;
    static GtkWidget* about_dlg = nullptr;
    if (!about_dlg)
    {
        GtkBuilder* builder = gtk_builder_new();

        builder = ptk_gtk_builder_new_from_file(PTK_DLG_ABOUT);
        about_dlg = GTK_WIDGET(gtk_builder_get_object(builder, "dlg"));
        g_object_unref(builder);
        gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(about_dlg), PACKAGE_VERSION);

        std::string name;
        xset_t set = xset_get(xset::name::main_icon);
        if (set->icon)
        {
            name = set->icon.value();
        }
        else
        {
            name = "spacefm";
        }
        gtk_about_dialog_set_logo_icon_name(GTK_ABOUT_DIALOG(about_dlg), name.c_str());

        // g_object_add_weak_pointer(G_OBJECT(about_dlg), (void*)&about_dlg);
        g_signal_connect(about_dlg, "response", G_CALLBACK(gtk_widget_destroy), nullptr);
    }
    gtk_window_set_transient_for(GTK_WINDOW(about_dlg), GTK_WINDOW(user_data));
    gtk_window_present(GTK_WINDOW(about_dlg));
}

static void
main_window_add_new_window(MainWindow* main_window)
{
    if (main_window && !main_window->maximized && !main_window->fullscreen)
    {
        // use current main_window's size for new window
        GtkAllocation allocation;
        gtk_widget_get_allocation(GTK_WIDGET(main_window), &allocation);
        if (allocation.width > 0)
        {
            app_settings.set_width(allocation.width);
            app_settings.set_height(allocation.height);
        }
    }
    // GtkWidget* new_win = main_window_new();
    main_window_new();
}

static void
on_new_window_activate(GtkMenuItem* menuitem, void* user_data)
{
    (void)menuitem;
    MainWindow* main_window = MAIN_WINDOW(user_data);

    autosave_request_cancel();
    main_window_store_positions(main_window);
    save_settings(main_window);

    main_window_add_new_window(main_window);
}

static bool
delayed_focus(GtkWidget* widget)
{
    if (GTK_IS_WIDGET(widget))
    {
        // ztd::logger::info("delayed_focus {:p}", fmt::ptr(widget));
        if (GTK_IS_WIDGET(widget))
        {
            gtk_widget_grab_focus(widget);
        }
    }
    return false;
}

static bool
delayed_focus_file_browser(PtkFileBrowser* file_browser)
{
    if (GTK_IS_WIDGET(file_browser) && GTK_IS_WIDGET(file_browser->folder_view))
    {
        // ztd::logger::info("delayed_focus_file_browser fb={:p}", fmt::ptr(file_browser));
        if (GTK_IS_WIDGET(file_browser) && GTK_IS_WIDGET(file_browser->folder_view))
        {
            gtk_widget_grab_focus(file_browser->folder_view);
            set_panel_focus(nullptr, file_browser);
        }
    }
    return false;
}

void
set_panel_focus(MainWindow* main_window, PtkFileBrowser* file_browser)
{
    if (!file_browser && !main_window)
    {
        return;
    }

    MainWindow* mw = main_window;
    if (!mw)
    {
        mw = MAIN_WINDOW(file_browser->main_window);
    }

    update_window_title(nullptr, mw);
    if (event_handler->pnl_focus->s || event_handler->pnl_focus->ob2_data)
    {
        main_window_event(main_window,
                          event_handler->pnl_focus,
                          xset::name::evt_pnl_focus,
                          mw->curpanel,
                          0,
                          nullptr,
                          0,
                          0,
                          0,
                          true);
    }
}

static void
on_fullscreen_activate(GtkMenuItem* menuitem, MainWindow* main_window)
{
    (void)menuitem;
    PtkFileBrowser* file_browser =
        PTK_FILE_BROWSER_REINTERPRET(main_window_get_current_file_browser(main_window));
    if (xset_get_b(xset::name::main_full))
    {
        if (file_browser && file_browser->view_mode == PtkFBViewMode::PTK_FB_LIST_VIEW)
        {
            ptk_file_browser_save_column_widths(GTK_TREE_VIEW(file_browser->folder_view),
                                                file_browser);
        }
        gtk_widget_hide(main_window->menu_bar);
        gtk_window_fullscreen(GTK_WINDOW(main_window));
        main_window->fullscreen = true;
    }
    else
    {
        main_window->fullscreen = false;
        gtk_window_unfullscreen(GTK_WINDOW(main_window));
        gtk_widget_show(main_window->menu_bar);

        if (!main_window->maximized)
        {
            show_panels(nullptr, main_window); // restore columns
        }
    }
}

static void
set_window_title(MainWindow* main_window, PtkFileBrowser* file_browser)
{
    std::filesystem::path disp_path;
    std::string disp_name;

    if (!file_browser || !main_window)
    {
        return;
    }

    if (file_browser->dir)
    {
        disp_path = file_browser->dir->path;
        disp_name = disp_path.filename();
    }
    else
    {
        const auto cwd = ptk_file_browser_get_cwd(file_browser);
        if (!cwd.empty())
        {
            disp_path = cwd;
            disp_name = disp_path.filename();
        }
    }

    const auto orig_fmt = xset_get_s(xset::name::main_title);
    std::string fmt;
    if (orig_fmt)
    {
        fmt = orig_fmt.value();
    }
    else
    {
        fmt = "%d";
    }

    static constexpr std::array<const std::string_view, 4> keys{"%t", "%T", "%p", "%P"};
    if (ztd::contains(fmt, keys))
    {
        // get panel/tab info
        const auto counts = main_window_get_counts(file_browser);
        const panel_t ipanel_count = counts[0];
        const tab_t itab_count = counts[1];
        const tab_t itab_num = counts[2];

        fmt = ztd::replace(fmt, "%t", std::to_string(itab_num));
        fmt = ztd::replace(fmt, "%T", std::to_string(itab_count));
        fmt = ztd::replace(fmt, "%p", std::to_string(main_window->curpanel));
        fmt = ztd::replace(fmt, "%P", std::to_string(ipanel_count));
    }
    if (ztd::contains(fmt, "*") && !main_tasks_running(main_window))
    {
        fmt = ztd::replace(fmt, "*", "");
    }
    if (ztd::contains(fmt, "%n"))
    {
        fmt = ztd::replace(fmt, "%n", disp_name);
    }
    if (orig_fmt && ztd::contains(orig_fmt.value(), "%d"))
    {
        fmt = ztd::replace(fmt, "%d", disp_path.string());
    }

    gtk_window_set_title(GTK_WINDOW(main_window), fmt.data());
}

static void
update_window_title(GtkMenuItem* item, MainWindow* main_window)
{
    (void)item;
    PtkFileBrowser* file_browser =
        PTK_FILE_BROWSER_REINTERPRET(main_window_get_current_file_browser(main_window));
    if (file_browser)
    {
        set_window_title(main_window, file_browser);
    }
}

static void
on_folder_notebook_switch_pape(GtkNotebook* notebook, GtkWidget* page, u32 page_num,
                               void* user_data)
{
    (void)page;
    MainWindow* main_window = MAIN_WINDOW(user_data);
    PtkFileBrowser* file_browser;

    // save sliders of current fb ( new tab while task manager is shown changes vals )
    PtkFileBrowser* curfb =
        PTK_FILE_BROWSER_REINTERPRET(main_window_get_current_file_browser(main_window));
    if (curfb)
    {
        ptk_file_browser_slider_release(nullptr, nullptr, curfb);
        if (curfb->view_mode == PtkFBViewMode::PTK_FB_LIST_VIEW)
        {
            ptk_file_browser_save_column_widths(GTK_TREE_VIEW(curfb->folder_view), curfb);
        }
    }

    file_browser = PTK_FILE_BROWSER_REINTERPRET(gtk_notebook_get_nth_page(notebook, page_num));
    // ztd::logger::info("on_folder_notebook_switch_pape fb={:p}   panel={}   page={}",
    // file_browser, file_browser->mypanel, page_num);
    main_window->curpanel = file_browser->mypanel;
    main_window->notebook = main_window->panel[main_window->curpanel - 1];

    main_window_update_status_bar(main_window, file_browser);

    set_window_title(main_window, file_browser);

    if (event_handler->tab_focus->ob2_data || event_handler->tab_focus->s)
    {
        main_window_event(main_window,
                          event_handler->tab_focus,
                          xset::name::evt_tab_focus,
                          main_window->curpanel,
                          page_num + 1,
                          nullptr,
                          0,
                          0,
                          0,
                          true);
    }

    ptk_file_browser_update_views(nullptr, file_browser);

    if (GTK_IS_WIDGET(file_browser))
    {
        g_idle_add((GSourceFunc)delayed_focus, file_browser->folder_view);
    }
}

void
main_window_open_path_in_current_tab(MainWindow* main_window, const std::filesystem::path& path)
{
    PtkFileBrowser* file_browser =
        PTK_FILE_BROWSER_REINTERPRET(main_window_get_current_file_browser(main_window));
    if (!file_browser)
    {
        return;
    }
    ptk_file_browser_chdir(file_browser, path, PtkFBChdirMode::PTK_FB_CHDIR_ADD_HISTORY);
}

void
main_window_open_network(MainWindow* main_window, const std::string_view url, bool new_tab)
{
    PtkFileBrowser* file_browser =
        PTK_FILE_BROWSER_REINTERPRET(main_window_get_current_file_browser(main_window));
    if (!file_browser)
    {
        return;
    }
    ptk_location_view_mount_network(file_browser, url, new_tab, false);
}

static void
on_file_browser_open_item(PtkFileBrowser* file_browser, const std::filesystem::path& path,
                          PtkOpenAction action, MainWindow* main_window)
{
    if (path.empty())
    {
        return;
    }

    switch (action)
    {
        case PtkOpenAction::PTK_OPEN_DIR:
            ptk_file_browser_chdir(file_browser, path, PtkFBChdirMode::PTK_FB_CHDIR_ADD_HISTORY);
            break;
        case PtkOpenAction::PTK_OPEN_NEW_TAB:
            main_window_add_new_tab(main_window, path);
            break;
        case PtkOpenAction::PTK_OPEN_NEW_WINDOW:
        case PtkOpenAction::PTK_OPEN_TERMINAL:
        case PtkOpenAction::PTK_OPEN_FILE:
        default:
            break;
    }
}

static void
main_window_update_status_bar(MainWindow* main_window, PtkFileBrowser* file_browser)
{
    (void)main_window;
    if (!(GTK_IS_WIDGET(file_browser) && GTK_IS_STATUSBAR(file_browser->status_bar)))
    {
        return;
    }

    if (file_browser->status_bar_custom)
    {
        gtk_statusbar_push(GTK_STATUSBAR(file_browser->status_bar),
                           0,
                           file_browser->status_bar_custom);
        return;
    }

    const auto cwd = ptk_file_browser_get_cwd(file_browser);
    if (cwd.empty())
    {
        return;
    }

    std::string statusbar_txt;

    if (std::filesystem::exists(cwd))
    {
        const auto fs_stat = ztd::statvfs(cwd);

        // calc free space
        const std::string free_size = vfs_file_size_format(fs_stat.bsize() * fs_stat.bavail());
        // calc total space
        const std::string disk_size = vfs_file_size_format(fs_stat.frsize() * fs_stat.blocks());

        statusbar_txt.append(std::format(" {} / {}   ", free_size, disk_size));
    }

    // Show Reading... while sill loading
    if (file_browser->busy)
    {
        statusbar_txt.append(
            std::format("Reading {} ...", ptk_file_browser_get_cwd(file_browser).string()));
        gtk_statusbar_push(GTK_STATUSBAR(file_browser->status_bar), 0, statusbar_txt.data());
        return;
    }

    u64 total_size;
    u64 total_on_disk_size;

    // note: total size will not include content changes since last selection change
    const u32 num_sel = ptk_file_browser_get_n_sel(file_browser, &total_size, &total_on_disk_size);
    const u32 num_vis = ptk_file_browser_get_n_visible_files(file_browser);

    if (num_sel > 0)
    {
        const std::vector<vfs::file_info> sel_files =
            ptk_file_browser_get_selected_files(file_browser);
        if (sel_files.empty())
        {
            return;
        }

        const std::string file_size = vfs_file_size_format(total_size);
        const std::string disk_size = vfs_file_size_format(total_on_disk_size);

        statusbar_txt.append(
            std::format("{} / {} ({} / {})", num_sel, num_vis, file_size, disk_size));

        if (num_sel == 1)
        // display file name or symlink info in status bar if one file selected
        {
            vfs::file_info file = vfs_file_info_ref(sel_files.front());
            if (!file)
            {
                return;
            }

            if (file->is_symlink())
            {
                const auto file_path = cwd / file->get_name();
                const auto target = std::filesystem::absolute(file_path);
                if (!target.empty())
                {
                    std::filesystem::path target_path;

                    // ztd::logger::info("LINK: {}", file_path);
                    if (!target.is_absolute())
                    {
                        // relative link
                        target_path = cwd / target;
                    }
                    else
                    {
                        target_path = target;
                    }

                    if (file->is_directory())
                    {
                        if (std::filesystem::exists(target_path))
                        {
                            statusbar_txt.append(std::format("  Link -> {}/", target.string()));
                        }
                        else
                        {
                            statusbar_txt.append(
                                std::format("  !Link -> {}/ (missing)", target.string()));
                        }
                    }
                    else
                    {
                        const auto results = ztd::stat(target_path);
                        if (results.is_valid())
                        {
                            const std::string lsize = vfs_file_size_format(results.size());
                            statusbar_txt.append(
                                std::format("  Link -> {} ({})", target.string(), lsize));
                        }
                        else
                        {
                            statusbar_txt.append(
                                std::format("  !Link -> {} (missing)", target.string()));
                        }
                    }
                }
                else
                {
                    statusbar_txt.append(std::format("  !Link -> (error reading target)"));
                }
            }
            else
            {
                statusbar_txt.append(std::format("  {}", file->get_name()));
            }

            vfs_file_info_unref(file);
        }
        else
        {
            u32 count_dir = 0;
            u32 count_file = 0;
            u32 count_symlink = 0;
            u32 count_socket = 0;
            u32 count_pipe = 0;
            u32 count_block = 0;
            u32 count_char = 0;

            for (vfs::file_info file : sel_files)
            {
                file = vfs_file_info_ref(file);
                if (!file)
                {
                    continue;
                }

                if (file->is_directory())
                {
                    ++count_dir;
                }
                else if (file->is_regular_file())
                {
                    ++count_file;
                }
                else if (file->is_symlink())
                {
                    ++count_symlink;
                }
                else if (file->is_socket())
                {
                    ++count_socket;
                }
                else if (file->is_fifo())
                {
                    ++count_pipe;
                }
                else if (file->is_block_file())
                {
                    ++count_block;
                }
                else if (file->is_character_file())
                {
                    ++count_char;
                }

                vfs_file_info_unref(file);
            }

            if (count_dir)
            {
                statusbar_txt.append(std::format("  Directories ({})", count_dir));
            }
            if (count_file)
            {
                statusbar_txt.append(std::format("  Files ({})", count_file));
            }
            if (count_symlink)
            {
                statusbar_txt.append(std::format("  Symlinks ({})", count_symlink));
            }
            if (count_socket)
            {
                statusbar_txt.append(std::format("  Sockets ({})", count_socket));
            }
            if (count_pipe)
            {
                statusbar_txt.append(std::format("  Named Pipes ({})", count_pipe));
            }
            if (count_block)
            {
                statusbar_txt.append(std::format("  Block Devices ({})", count_block));
            }
            if (count_char)
            {
                statusbar_txt.append(std::format("  Character Devices ({})", count_char));
            }
        }
    }
    else
    {
        // size of files in dir, does not get subdir size
        // TODO, can use file_browser->dir->file_list
        u64 disk_size_bytes = 0;
        u64 disk_size_disk = 0;
        for (const auto& file : std::filesystem::directory_iterator(cwd))
        {
            const auto file_stat = ztd::stat(file.path());
            if (!file_stat.is_regular_file())
            {
                continue;
            }
            disk_size_bytes += file_stat.size();
            disk_size_disk += file_stat.blocks() * ztd::BLOCK_SIZE;
        }
        const std::string file_size = vfs_file_size_format(disk_size_bytes);
        const std::string disk_size = vfs_file_size_format(disk_size_disk);

        // count for .hidden files
        const u32 num_hid = ptk_file_browser_get_n_all_files(file_browser) - num_vis;
        const u32 num_hidx = file_browser->dir ? file_browser->dir->xhidden_count : 0;
        if (num_hid || num_hidx)
        {
            statusbar_txt.append(std::format("{} visible ({} hidden)  ({} / {})",
                                             num_vis,
                                             num_hid,
                                             file_size,
                                             disk_size));
        }
        else
        {
            statusbar_txt.append(std::format("{} {}  ({} / {})",
                                             num_vis,
                                             num_vis == 1 ? "item" : "items",
                                             file_size,
                                             disk_size));
        }

        // cur dir is a symlink? canonicalize path
        if (std::filesystem::is_symlink(cwd))
        {
            const auto canon = std::filesystem::read_symlink(cwd);
            statusbar_txt.append(std::format("  {} -> {}", cwd.string(), canon.string()));
        }
        else
        {
            statusbar_txt.append(std::format("  {}", cwd.string()));
        }
    }

    // too much padding
    gtk_widget_set_margin_top(GTK_WIDGET(file_browser->status_bar), 0);
    gtk_widget_set_margin_bottom(GTK_WIDGET(file_browser->status_bar), 0);

    gtk_statusbar_push(GTK_STATUSBAR(file_browser->status_bar), 0, statusbar_txt.data());
}

static void
on_file_browser_panel_change(PtkFileBrowser* file_browser, MainWindow* main_window)
{
    // ztd::logger::info("panel_change  panel {}", file_browser->mypanel);
    main_window->curpanel = file_browser->mypanel;
    main_window->notebook = main_window->panel[main_window->curpanel - 1];
    // set_window_title( main_window, file_browser );
    set_panel_focus(main_window, file_browser);
}

static void
on_file_browser_sel_change(PtkFileBrowser* file_browser, MainWindow* main_window)
{
    // ztd::logger::info("sel_change  panel {}", file_browser->mypanel);
    if ((event_handler->pnl_sel->ob2_data || event_handler->pnl_sel->s) &&
        main_window_event(main_window,
                          event_handler->pnl_sel,
                          xset::name::evt_pnl_sel,
                          0,
                          0,
                          nullptr,
                          0,
                          0,
                          0,
                          true))
    {
        return;
    }
    main_window_update_status_bar(main_window, file_browser);
}

static void
on_file_browser_content_change(PtkFileBrowser* file_browser, MainWindow* main_window)
{
    // ztd::logger::info("content_change  panel {}", file_browser->mypanel);
    main_window_update_status_bar(main_window, file_browser);
}

static bool
on_tab_drag_motion(GtkWidget* widget, GdkDragContext* drag_context, i32 x, i32 y, u32 time,
                   PtkFileBrowser* file_browser)
{
    (void)widget;
    (void)drag_context;
    (void)x;
    (void)y;
    (void)time;
    GtkNotebook* notebook = GTK_NOTEBOOK(gtk_widget_get_parent(GTK_WIDGET(file_browser)));
    // TODO: Add a timeout here and do not set current page immediately
    const i32 idx = gtk_notebook_page_num(notebook, GTK_WIDGET(file_browser));
    gtk_notebook_set_current_page(notebook, idx);
    return false;
}

static bool
on_window_button_press_event(GtkWidget* widget, GdkEventButton* event,
                             MainWindow* main_window) // sfm
{
    (void)widget;
    if (event->type != GdkEventType::GDK_BUTTON_PRESS)
    {
        return false;
    }

    // handle mouse back/forward buttons anywhere in the main window
    if (event->button == 4 || event->button == 5 || event->button == 8 || event->button == 9) // sfm
    {
        PtkFileBrowser* file_browser =
            PTK_FILE_BROWSER_REINTERPRET(main_window_get_current_file_browser(main_window));
        if (!file_browser)
        {
            return false;
        }
        if (event->button == 4 || event->button == 8)
        {
            ptk_file_browser_go_back(nullptr, file_browser);
        }
        else
        {
            ptk_file_browser_go_forward(nullptr, file_browser);
        }
        return true;
    }
    return false;
}

static bool
on_main_window_focus(GtkWidget* main_window, GdkEventFocus* event, void* user_data)
{
    (void)event;
    (void)user_data;
    // this causes a widget not realized loop by running rebuild_menus while
    // rebuild_menus is already running
    // but this unneeded anyway?  cross-window menu changes seem to work ok
    // rebuild_menus( main_window );  // xset may change in another window
    if (event_handler->win_focus->s || event_handler->win_focus->ob2_data)
    {
        main_window_event(MAIN_WINDOW_REINTERPRET(main_window),
                          event_handler->win_focus,
                          xset::name::evt_win_focus,
                          0,
                          0,
                          nullptr,
                          0,
                          0,
                          0,
                          true);
    }
    return false;
}

static bool
on_main_window_keypress(MainWindow* main_window, GdkEventKey* event, xset_t known_set)
{
    // ztd::logger::info("main_keypress {} {}", event->keyval, event->state);

    PtkFileBrowser* browser;

    if (known_set)
    {
        xset_t set = known_set;
        return on_main_window_keypress_found_key(main_window, set);
    }

    if (event->keyval == 0)
    {
        return false;
    }

    const u32 keymod = ptk_get_keymod(event->state);

    if ((event->keyval == GDK_KEY_Home &&
         (keymod == 0 || keymod == GdkModifierType::GDK_SHIFT_MASK)) ||
        (event->keyval == GDK_KEY_End &&
         (keymod == 0 || keymod == GdkModifierType::GDK_SHIFT_MASK)) ||
        (event->keyval == GDK_KEY_Delete && keymod == 0) ||
        (event->keyval == GDK_KEY_Tab && keymod == 0) ||
        (keymod == 0 && (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter)) ||
        (event->keyval == GDK_KEY_Left &&
         (keymod == 0 || keymod == GdkModifierType::GDK_SHIFT_MASK)) ||
        (event->keyval == GDK_KEY_Right &&
         (keymod == 0 || keymod == GdkModifierType::GDK_SHIFT_MASK)) ||
        (event->keyval == GDK_KEY_BackSpace && keymod == 0) ||
        (keymod == 0 && event->keyval != GDK_KEY_Escape &&
         gdk_keyval_to_unicode(event->keyval))) // visible char
    {
        browser = PTK_FILE_BROWSER_REINTERPRET(main_window_get_current_file_browser(main_window));
        if (browser && browser->path_bar && gtk_widget_has_focus(GTK_WIDGET(browser->path_bar)))
        {
            return false; // send to pathbar
        }
    }

#if defined(HAVE_NONLATIN_KEYBOARD_SUPPORT)
    u32 nonlatin_key = 0;
    // need to transpose nonlatin keyboard layout ?
    if (!((GDK_KEY_0 <= event->keyval && event->keyval <= GDK_KEY_9) ||
          (GDK_KEY_A <= event->keyval && event->keyval <= GDK_KEY_Z) ||
          (GDK_KEY_a <= event->keyval && event->keyval <= GDK_KEY_z)))
    {
        nonlatin_key = event->keyval;
        transpose_nonlatin_keypress(event);
    }
#endif

    if ((event_handler->win_key->s || event_handler->win_key->ob2_data) &&
        main_window_event(main_window,
                          event_handler->win_key,
                          xset::name::evt_win_key,
                          0,
                          0,
                          nullptr,
                          event->keyval,
                          0,
                          keymod,
                          true))
    {
        return true;
    }

    for (xset_t set : xsets)
    {
        assert(set != nullptr);

        if (set->shared_key)
        {
            // set has shared key
#if defined(HAVE_NONLATIN_KEYBOARD_SUPPORT)
            // nonlatin key match is for nonlatin keycodes set prior to 1.0.3
            set = xset_get(set->shared_key.value());
            if ((set->key == event->keyval || (nonlatin_key && set->key == nonlatin_key)) &&
                set->keymod == keymod)
#else
            set = xset_get(set->shared_key.value());
            if (set->key == event->keyval && set->keymod == keymod)
#endif
            {
                // shared key match
                if (ztd::startswith(set->name, "panel"))
                {
                    // use current panel's set
                    browser = PTK_FILE_BROWSER_REINTERPRET(
                        main_window_get_current_file_browser(main_window));
                    if (browser)
                    {
                        const std::string new_set_name =
                            std::format("panel{}{}", browser->mypanel, set->name.data() + 6);
                        set = xset_get(new_set_name);
                    }
                    else
                    { // failsafe
                        return false;
                    }
                }
                return on_main_window_keypress_found_key(main_window, set);
            }
            else
            {
                continue;
            }
        }
#if defined(HAVE_NONLATIN_KEYBOARD_SUPPORT)
        // nonlatin key match is for nonlatin keycodes set prior to 1.0.3
        if (((set->key == event->keyval ||
            (nonlatin_key && set->key == nonlatin_key)) &&
            set->keymod == keymod)
#else
        if (set->key == event->keyval && set->keymod == keymod)
#endif
        {
            return on_main_window_keypress_found_key(main_window, set);
        }
    }

#if defined(HAVE_NONLATIN_KEYBOARD_SUPPORT)
    if (nonlatin_key != 0)
    {
        // use literal keycode for pass-thru, eg for find-as-you-type search
        event->keyval = nonlatin_key;
    }
#endif

    if ((event->state & GdkModifierType::GDK_MOD1_MASK))
    {
        rebuild_menus(main_window);
    }

    return false;
}

static bool
on_main_window_keypress_found_key(MainWindow* main_window, xset_t set)
{
    PtkFileBrowser* browser;

    browser = PTK_FILE_BROWSER_REINTERPRET(main_window_get_current_file_browser(main_window));
    if (!browser)
    {
        return true;
    }

    // special edit items
    if (set->xset_name == xset::name::edit_cut || set->xset_name == xset::name::edit_copy ||
        set->xset_name == xset::name::edit_delete || set->xset_name == xset::name::select_all)
    {
        if (!gtk_widget_is_focus(browser->folder_view))
        {
            return false;
        }
    }
    else if (set->xset_name == xset::name::edit_paste)
    {
        const bool side_dir_focus =
            (browser->side_dir && gtk_widget_is_focus(GTK_WIDGET(browser->side_dir)));
        if (!gtk_widget_is_focus(GTK_WIDGET(browser->folder_view)) && !side_dir_focus)
        {
            return false;
        }
    }

    // run menu_cb
    if (set->menu_style < xset::menu::submenu)
    {
        set->browser = browser;
        xset_menu_cb(nullptr, set); // also does custom activate
    }
    if (!set->lock)
    {
        return true;
    }

    // handlers
    if (ztd::startswith(set->name, "dev_"))
    {
        ptk_location_view_on_action(GTK_WIDGET(browser->side_dev), set);
    }
    else if (ztd::startswith(set->name, "main_"))
    {
        if (set->xset_name == xset::name::main_new_window)
        {
            on_new_window_activate(nullptr, main_window);
        }
        else if (set->xset_name == xset::name::main_root_window)
        {
            on_open_current_folder_as_root(nullptr, main_window);
        }
        else if (set->xset_name == xset::name::main_search)
        {
            on_find_file_activate(nullptr, main_window);
        }
        else if (set->xset_name == xset::name::main_terminal)
        {
            on_open_terminal_activate(nullptr, main_window);
        }
        else if (set->xset_name == xset::name::main_root_terminal)
        {
            on_open_root_terminal_activate(nullptr, main_window);
        }
        else if (set->xset_name == xset::name::main_save_session)
        {
            on_open_url(nullptr, main_window);
        }
        else if (set->xset_name == xset::name::main_exit)
        {
            on_quit_activate(nullptr, main_window);
        }
        else if (set->xset_name == xset::name::main_full)
        {
            xset_set_b(xset::name::main_full, !main_window->fullscreen);
            on_fullscreen_activate(nullptr, main_window);
        }
        else if (set->xset_name == xset::name::main_prefs)
        {
            on_preference_activate(nullptr, main_window);
        }
        else if (set->xset_name == xset::name::main_design_mode)
        {
            main_design_mode(nullptr, main_window);
        }
        else if (set->xset_name == xset::name::main_icon)
        {
            on_main_icon();
        }
        else if (set->xset_name == xset::name::main_title)
        {
            update_window_title(nullptr, main_window);
        }
        else if (set->xset_name == xset::name::main_about)
        {
            on_about_activate(nullptr, main_window);
        }
    }
    else if (ztd::startswith(set->name, "panel_"))
    {
        i32 i;
        if (set->xset_name == xset::name::panel_prev)
        {
            i = panel_control_code_prev;
        }
        else if (set->xset_name == xset::name::panel_next)
        {
            i = panel_control_code_next;
        }
        else if (set->xset_name == xset::name::panel_hide)
        {
            i = panel_control_code_hide;
        }
        else
        {
            i = std::stol(set->name);
        }
        focus_panel(nullptr, main_window, i);
    }
    else if (ztd::startswith(set->name, "plug_"))
    {
        on_plugin_install(nullptr, main_window, set);
    }
    else if (ztd::startswith(set->name, "task_"))
    {
        if (set->xset_name == xset::name::task_manager)
        {
            on_task_popup_show(nullptr, main_window, set->name.c_str());
        }
        else if (set->xset_name == xset::name::task_col_reorder)
        {
            on_reorder(nullptr, GTK_WIDGET(browser->task_view));
        }
        else if (set->xset_name == xset::name::task_col_status ||
                 set->xset_name == xset::name::task_col_count ||
                 set->xset_name == xset::name::task_col_path ||
                 set->xset_name == xset::name::task_col_file ||
                 set->xset_name == xset::name::task_col_to ||
                 set->xset_name == xset::name::task_col_progress ||
                 set->xset_name == xset::name::task_col_total ||
                 set->xset_name == xset::name::task_col_started ||
                 set->xset_name == xset::name::task_col_elapsed ||
                 set->xset_name == xset::name::task_col_curspeed ||
                 set->xset_name == xset::name::task_col_curest ||
                 set->xset_name == xset::name::task_col_avgspeed ||
                 set->xset_name == xset::name::task_col_avgest ||
                 set->xset_name == xset::name::task_col_reorder)
        {
            on_task_column_selected(nullptr, browser->task_view);
        }
        else if (set->xset_name == xset::name::task_stop ||
                 set->xset_name == xset::name::task_stop_all ||
                 set->xset_name == xset::name::task_pause ||
                 set->xset_name == xset::name::task_pause_all ||
                 set->xset_name == xset::name::task_que ||
                 set->xset_name == xset::name::task_que_all ||
                 set->xset_name == xset::name::task_resume ||
                 set->xset_name == xset::name::task_resume_all)
        {
            PtkFileTask* ptask = get_selected_task(browser->task_view);
            on_task_stop(nullptr, browser->task_view, set, ptask);
        }
        else if (set->xset_name == xset::name::task_showout)
        {
            show_task_dialog(nullptr, browser->task_view);
        }
        else if (ztd::startswith(set->name, "task_err_"))
        {
            on_task_popup_errset(nullptr, main_window, set->name.c_str());
        }
    }
    else if (set->xset_name == xset::name::rubberband)
    {
        main_window_rubberband_all();
    }
    else
    {
        ptk_file_browser_on_action(browser, set->xset_name);
    }

    return true;
}

MainWindow*
main_window_get_last_active()
{
    if (!all_windows.empty())
    {
        return all_windows.at(0);
    }
    return nullptr;
}

const std::vector<MainWindow*>&
main_window_get_all()
{
    return all_windows;
}

static long
get_desktop_index(GtkWindow* win)
{
#if 1
    (void)win;

    return -1;
#else // Broken with wayland
    i64 desktop = -1;
    GdkDisplay* display;
    GdkWindow* window = nullptr;

    if (win)
    {
        // get desktop of win
        display = gtk_widget_get_display(GTK_WIDGET(win));
        window = gtk_widget_get_window(GTK_WIDGET(win));
    }
    else
    {
        // get current desktop
        display = gdk_display_get_default();
        if (display)
            window = gdk_x11_window_lookup_for_display(display, gdk_x11_get_default_root_xwindow());
    }

    if (!(GDK_IS_DISPLAY(display) && GDK_IS_WINDOW(window)))
        return desktop;

    // find out what desktop (workspace) window is on   #include <gdk/gdkx.h>
    Atom type;
    i32 format;
    u64 nitems;
    u64 bytes_after;
    unsigned char* data;
    const char* atom_name = win ? "_NET_WM_DESKTOP" : "_NET_CURRENT_DESKTOP";
    Atom net_wm_desktop = gdk_x11_get_xatom_by_name_for_display(display, atom_name);

    if (net_wm_desktop == None)
        ztd::logger::error("atom not found: {}", atom_name);
    else if (XGetWindowProperty(GDK_DISPLAY_XDISPLAY(display),
                                GDK_WINDOW_XID(window),
                                net_wm_desktop,
                                0,
                                1,
                                False,
                                XA_CARDINAL,
                                (Atom*)&type,
                                &format,
                                &nitems,
                                &bytes_after,
                                &data) != Success ||
             type == None || data == nullptr)
    {
        if (type == None)
            ztd::logger::error("No such property from XGetWindowProperty() {}", atom_name);
        else if (data == nullptr)
            ztd::logger::error("No data returned from XGetWindowProperty() {}", atom_name);
        else
            ztd::logger::error("XGetWindowProperty() {} failed\n", atom_name);
    }
    else
    {
        desktop = *data;
        XFree(data);
    }
    return desktop;
#endif
}

MainWindow*
main_window_get_on_current_desktop()
{ // find the last used spacefm window on the current desktop
    const i64 cur_desktop = get_desktop_index(nullptr);
    // ztd::logger::info("current_desktop = {}", cur_desktop);
    if (cur_desktop == -1)
    {
        return main_window_get_last_active(); // revert to dumb if no current
    }

    bool invalid = false;
    for (MainWindow* window : all_windows)
    {
        const i64 desktop = get_desktop_index(GTK_WINDOW(window));
        // ztd::logger::info( "    test win {:p} = {}", window, desktop);
        if (desktop == cur_desktop || desktop > 254 /* 255 == all desktops */)
        {
            return window;
        }
        else if (desktop == -1 && !invalid)
        {
            invalid = true;
        }
    }
    // revert to dumb if one or more window desktops unreadable
    return invalid ? main_window_get_last_active() : nullptr;
}

enum MainWindowTaskCol
{
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
    TASK_COL_STARTTIME,
    TASK_COL_ICON,
    TASK_COL_DATA
};

inline constexpr std::array<const std::string_view, 14> task_titles{
    // If you change "Status", also change it in on_task_button_press_event
    "Status",
    "#",
    "Directory",
    "Item",
    "To",
    "Progress",
    "Total",
    "Started",
    "Elapsed",
    "Current",
    "CRemain",
    "Average",
    "Remain",
    "StartTime",
};

inline constexpr std::array<xset::name, 13> task_names{
    xset::name::task_col_status,
    xset::name::task_col_count,
    xset::name::task_col_path,
    xset::name::task_col_file,
    xset::name::task_col_to,
    xset::name::task_col_progress,
    xset::name::task_col_total,
    xset::name::task_col_started,
    xset::name::task_col_elapsed,
    xset::name::task_col_curspeed,
    xset::name::task_col_curest,
    xset::name::task_col_avgspeed,
    xset::name::task_col_avgest,
};

void
on_reorder(GtkWidget* item, GtkWidget* parent)
{
    (void)item;
    xset_msg_dialog(
        parent,
        GtkMessageType::GTK_MESSAGE_INFO,
        "Reorder Columns Help",
        GtkButtonsType::GTK_BUTTONS_OK,
        "To change the order of the columns, drag the column header to the desired location.");
}

void
main_context_fill(PtkFileBrowser* file_browser, const xset_context_t& c)
{
    PtkFileBrowser* a_browser;
    vfs::mime_type mime_type;
    GtkClipboard* clip = nullptr;
    vfs::volume vol;
    GtkTreeModel* model;
    GtkTreeModel* model_task;
    GtkTreeIter it;

    c->valid = false;
    if (!GTK_IS_WIDGET(file_browser))
    {
        return;
    }

    MainWindow* main_window = MAIN_WINDOW(file_browser->main_window);
    if (!main_window)
    {
        return;
    }

    if (!c->var[ItemPropContext::CONTEXT_NAME].empty())
    {
        // if name is set, assume we do not need all selected files info
        c->var[ItemPropContext::CONTEXT_DIR] = ptk_file_browser_get_cwd(file_browser);
        // if (c->var[ItemPropContext::CONTEXT_DIR])
        //{
        c->var[ItemPropContext::CONTEXT_WRITE_ACCESS] =
            ptk_file_browser_write_access(c->var[ItemPropContext::CONTEXT_DIR]) ? "false" : "true";
        // }

        const std::vector<vfs::file_info> sel_files =
            ptk_file_browser_get_selected_files(file_browser);
        if (!sel_files.empty())
        {
            vfs::file_info file = vfs_file_info_ref(sel_files.front());

            c->var[ItemPropContext::CONTEXT_NAME] = file->get_name();
            const auto path = std::filesystem::path() / c->var[ItemPropContext::CONTEXT_DIR] /
                              c->var[ItemPropContext::CONTEXT_NAME];
            c->var[ItemPropContext::CONTEXT_IS_DIR] =
                std::filesystem::is_directory(path) ? "true" : "false";
            c->var[ItemPropContext::CONTEXT_IS_TEXT] = file->is_text(path) ? "true" : "false";
            c->var[ItemPropContext::CONTEXT_IS_LINK] = file->is_symlink() ? "true" : "false";

            mime_type = file->get_mime_type();
            if (mime_type)
            {
                c->var[ItemPropContext::CONTEXT_MIME] = mime_type->get_type();
            }

            c->var[ItemPropContext::CONTEXT_MUL_SEL] = sel_files.size() > 1 ? "true" : "false";

            vfs_file_info_unref(file);
        }
        else
        {
            c->var[ItemPropContext::CONTEXT_NAME] = "";
            c->var[ItemPropContext::CONTEXT_IS_DIR] = "false";
            c->var[ItemPropContext::CONTEXT_IS_TEXT] = "false";
            c->var[ItemPropContext::CONTEXT_IS_LINK] = "false";
            c->var[ItemPropContext::CONTEXT_MIME] = "";
            c->var[ItemPropContext::CONTEXT_MUL_SEL] = "false";
        }

        vfs_file_info_list_free(sel_files);
    }

    c->var[ItemPropContext::CONTEXT_IS_ROOT] = geteuid() == 0 ? "true" : "false";

    if (c->var[ItemPropContext::CONTEXT_CLIP_FILES].empty())
    {
        clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
        if (!gtk_clipboard_wait_is_target_available(
                clip,
                gdk_atom_intern("x-special/gnome-copied-files", false)) &&
            !gtk_clipboard_wait_is_target_available(clip, gdk_atom_intern("text/uri-list", false)))
        {
            c->var[ItemPropContext::CONTEXT_CLIP_FILES] = "false";
        }
        else
        {
            c->var[ItemPropContext::CONTEXT_CLIP_FILES] = "true";
        }
    }

    if (c->var[ItemPropContext::CONTEXT_CLIP_TEXT].empty())
    {
        if (!clip)
        {
            clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
        }
        c->var[ItemPropContext::CONTEXT_CLIP_TEXT] =
            gtk_clipboard_wait_is_text_available(clip) ? "true" : "false";
    }

    // hack - Due to ptk_file_browser_update_views main iteration, fb tab may be destroyed
    // asynchronously - common if gui thread is blocked on stat
    // NOTE:  this is no longer needed
    if (!GTK_IS_WIDGET(file_browser))
    {
        return;
    }

    // device
    if (file_browser->side_dev &&
        (vol = ptk_location_view_get_selected_vol(GTK_TREE_VIEW(file_browser->side_dev))))
    {
        c->var[ItemPropContext::CONTEXT_DEVICE] = vol->device_file;
        c->var[ItemPropContext::CONTEXT_DEVICE_LABEL] = vol->label;
        c->var[ItemPropContext::CONTEXT_DEVICE_MOUNT_POINT] = vol->mount_point;
        c->var[ItemPropContext::CONTEXT_DEVICE_UDI] = vol->udi;
        c->var[ItemPropContext::CONTEXT_DEVICE_FSTYPE] = vol->fs_type;

        std::string flags;
        if (vol->is_removable)
        {
            flags = std::format("{} removable", flags);
        }
        else
        {
            flags = std::format("{} internal", flags);
        }

        if (vol->requires_eject)
        {
            flags = std::format("{} ejectable", flags);
        }

        if (vol->is_optical)
        {
            flags = std::format("{} optical", flags);
        }

        if (!vol->is_user_visible)
        {
            flags = std::format("{} policy_hide", flags);
        }

        if (vol->is_mounted)
        {
            flags = std::format("{} mounted", flags);
        }
        else if (vol->is_mountable)
        {
            flags = std::format("{} mountable", flags);
        }
        else
        {
            flags = std::format("{} no_media", flags);
        }

        c->var[ItemPropContext::CONTEXT_DEVICE_PROP] = flags;
    }

    // panels
    i32 panel_count = 0;
    for (const panel_t p : PANELS)
    {
        if (!xset_get_b_panel(p, xset::panel::show))
        {
            continue;
        }
        const i32 i = gtk_notebook_get_current_page(GTK_NOTEBOOK(main_window->panel[p - 1]));
        if (i != -1)
        {
            a_browser = PTK_FILE_BROWSER_REINTERPRET(
                gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_window->panel[p - 1]), i));
        }
        else
        {
            continue;
        }
        if (!a_browser || !gtk_widget_get_visible(GTK_WIDGET(a_browser)))
        {
            continue;
        }

        panel_count++;
        c->var[ItemPropContext::CONTEXT_PANEL1_DIR + p - 1] = ptk_file_browser_get_cwd(a_browser);

        if (a_browser->side_dev &&
            (vol = ptk_location_view_get_selected_vol(GTK_TREE_VIEW(a_browser->side_dev))))
        {
            c->var[ItemPropContext::CONTEXT_PANEL1_DEVICE + p - 1] = vol->device_file;
        }

        // panel has files selected?
        if (a_browser->view_mode == PtkFBViewMode::PTK_FB_ICON_VIEW ||
            a_browser->view_mode == PtkFBViewMode::PTK_FB_COMPACT_VIEW)
        {
            GList* sel_files = folder_view_get_selected_items(a_browser, &model);
            if (sel_files)
            {
                c->var[ItemPropContext::CONTEXT_PANEL1_SEL + p - 1] = "true";
            }
            else
            {
                c->var[ItemPropContext::CONTEXT_PANEL1_SEL + p - 1] = "false";
            }
            g_list_foreach(sel_files, (GFunc)gtk_tree_path_free, nullptr);
            g_list_free(sel_files);
        }
        else if (file_browser->view_mode == PtkFBViewMode::PTK_FB_LIST_VIEW)
        {
            GtkTreeSelection* tree_sel =
                gtk_tree_view_get_selection(GTK_TREE_VIEW(a_browser->folder_view));
            if (gtk_tree_selection_count_selected_rows(tree_sel) > 0)
            {
                c->var[ItemPropContext::CONTEXT_PANEL1_SEL + p - 1] = "true";
            }
            else
            {
                c->var[ItemPropContext::CONTEXT_PANEL1_SEL + p - 1] = "false";
            }
        }
        else
        {
            c->var[ItemPropContext::CONTEXT_PANEL1_SEL + p - 1] = "false";
        }

        if (file_browser == a_browser)
        {
            c->var[ItemPropContext::CONTEXT_TAB] = std::to_string(i + 1);
            c->var[ItemPropContext::CONTEXT_TAB_COUNT] =
                std::to_string(gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_window->panel[p - 1])));
        }
    }
    c->var[ItemPropContext::CONTEXT_PANEL_COUNT] = panel_count;
    c->var[ItemPropContext::CONTEXT_PANEL] = file_browser->mypanel;

    for (const panel_t p : PANELS)
    {
        if (c->var[ItemPropContext::CONTEXT_PANEL1_DIR + p - 1].empty())
        {
            c->var[ItemPropContext::CONTEXT_PANEL1_DIR + p - 1] = "";
        }
        if (c->var[ItemPropContext::CONTEXT_PANEL1_SEL + p - 1].empty())
        {
            c->var[ItemPropContext::CONTEXT_PANEL1_SEL + p - 1] = "false";
        }
        if (c->var[ItemPropContext::CONTEXT_PANEL1_DEVICE + p - 1].empty())
        {
            c->var[ItemPropContext::CONTEXT_PANEL1_DEVICE + p - 1] = "";
        }
    }

    // tasks
    static constexpr std::array<const std::string_view, 7> job_titles{
        "move",
        "copy",
        "trash",
        "delete",
        "link",
        "change",
        "run",
    };

    PtkFileTask* ptask = get_selected_task(file_browser->task_view);
    if (ptask)
    {
        c->var[ItemPropContext::CONTEXT_TASK_TYPE] = job_titles.at(ptask->task->type).data();
        if (ptask->task->type == VFSFileTaskType::EXEC)
        {
            if (ptask->task->current_file)
            {
                c->var[ItemPropContext::CONTEXT_TASK_NAME] = ptask->task->current_file.value();
            }
            if (ptask->task->dest_dir)
            {
                c->var[ItemPropContext::CONTEXT_TASK_DIR] = ptask->task->dest_dir.value();
            }
        }
        else
        {
            c->var[ItemPropContext::CONTEXT_TASK_NAME] = "";
            ptk_file_task_lock(ptask);
            if (ptask->task->current_file)
            {
                const auto current_file = ptask->task->current_file.value();
                c->var[ItemPropContext::CONTEXT_TASK_DIR] = current_file.parent_path();
            }
            ptk_file_task_unlock(ptask);
        }
    }
    else
    {
        c->var[ItemPropContext::CONTEXT_TASK_TYPE] = "";
        c->var[ItemPropContext::CONTEXT_TASK_NAME] = "";
        c->var[ItemPropContext::CONTEXT_TASK_DIR] = "";
    }
    if (!main_window->task_view || !GTK_IS_TREE_VIEW(main_window->task_view))
    {
        c->var[ItemPropContext::CONTEXT_TASK_COUNT] = "0";
    }
    else
    {
        model_task = gtk_tree_view_get_model(GTK_TREE_VIEW(main_window->task_view));
        i32 task_count = 0;
        if (gtk_tree_model_get_iter_first(model_task, &it))
        {
            task_count++;
            while (gtk_tree_model_iter_next(model_task, &it))
            {
                task_count++;
            }
        }
        c->var[ItemPropContext::CONTEXT_TASK_COUNT] = std::to_string(task_count);
    }

    c->valid = true;
}

static MainWindow*
get_task_view_window(GtkWidget* view)
{
    for (MainWindow* window : all_windows)
    {
        if (window->task_view == view)
        {
            return window;
        }
    }
    return nullptr;
}

const std::string
main_write_exports(vfs::file_task vtask, const std::string_view value)
{
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER(vtask->exec_browser);
    MainWindow* main_window = MAIN_WINDOW(file_browser->main_window);

    const xset_t set = vtask->exec_set;

    std::string buf;

    // buf.append("\n#source");
    // buf.append("\n\ncp $0 /tmp\n\n");

    // panels
    for (panel_t p : PANELS)
    {
        PtkFileBrowser* a_browser;

        if (!xset_get_b_panel(p, xset::panel::show))
        {
            continue;
        }
        const i32 current_page =
            gtk_notebook_get_current_page(GTK_NOTEBOOK(main_window->panel[p - 1]));
        if (current_page != -1)
        {
            a_browser = PTK_FILE_BROWSER_REINTERPRET(
                gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_window->panel[p - 1]), current_page));
        }
        else
        {
            continue;
        }

        if (!a_browser || !gtk_widget_get_visible(GTK_WIDGET(a_browser)))
        {
            continue;
        }

        // cwd
        const auto cwd = ptk_file_browser_get_cwd(a_browser);
        buf.append(std::format("set fm_pwd_panel[{}] {}\n", p, ztd::shell::quote(cwd.string())));
        buf.append(std::format("set fm_tab_panel[{}] {}\n", p, current_page + 1));

        // selected files
        const std::vector<vfs::file_info> sel_files =
            ptk_file_browser_get_selected_files(a_browser);
        if (!sel_files.empty())
        {
            // create fish array
            buf.append(std::format("set fm_panel{}_files (echo ", p));
            for (vfs::file_info file : sel_files)
            {
                const auto path = cwd / file->get_name();
                buf.append(std::format("{} ", ztd::shell::quote(path.string())));
            }
            buf.append(std::format(")\n"));

            if (file_browser == a_browser)
            {
                // create fish array
                buf.append(std::format("set fm_filenames (echo "));
                for (vfs::file_info file : sel_files)
                {
                    buf.append(std::format("{} ", ztd::shell::quote(file->get_name())));
                }
                buf.append(std::format(")\n"));
            }

            vfs_file_info_list_free(sel_files);
        }

        // device
        if (a_browser->side_dev)
        {
            vfs::volume vol =
                ptk_location_view_get_selected_vol(GTK_TREE_VIEW(a_browser->side_dev));
            if (vol)
            {
                if (file_browser == a_browser)
                {
                    // clang-format off
                    buf.append(std::format("set fm_device {}\n", ztd::shell::quote(vol->device_file)));
                    buf.append(std::format("set fm_device_udi {}\n", ztd::shell::quote(vol->udi)));
                    buf.append(std::format("set fm_device_mount_point {}\n", ztd::shell::quote(vol->mount_point)));
                    buf.append(std::format("set fm_device_label {}\n", ztd::shell::quote(vol->label)));
                    buf.append(std::format("set fm_device_fstype {}\n", ztd::shell::quote(vol->fs_type)));
                    buf.append(std::format("set fm_device_size {}\n", vol->size));
                    buf.append(std::format("set fm_device_display_name {}\n", ztd::shell::quote(vol->disp_name)));
                    buf.append(std::format("set fm_device_icon {}\n", ztd::shell::quote(vol->icon)));
                    buf.append(std::format("set fm_device_is_mounted {}\n", vol->is_mounted ? 1 : 0));
                    buf.append(std::format("set fm_device_is_optical {}\n", vol->is_optical ? 1 : 0));
                    buf.append(std::format("set fm_device_is_removable {}\n", vol->is_removable ? 1 : 0));
                    buf.append(std::format("set fm_device_is_mountable {}\n", vol->is_mountable ? 1 : 0));
                    // clang-format on
                }
                // clang-format off
                buf.append(std::format("set fm_panel{}_device {}\n", p, ztd::shell::quote(vol->device_file)));
                buf.append(std::format("set fm_panel{}_device_udi {}\n", p, ztd::shell::quote(vol->udi)));
                buf.append(std::format("set fm_panel{}_device_mount_point {}\n", p, ztd::shell::quote(vol->mount_point)));
                buf.append(std::format("set fm_panel{}_device_label {}\n", p, ztd::shell::quote(vol->label)));
                buf.append(std::format("set fm_panel{}_device_fstype {}\n", p, ztd::shell::quote(vol->fs_type)));
                buf.append(std::format("set fm_panel{}_device_size {}\n", p, vol->size));
                buf.append(std::format("set fm_panel{}_device_display_name {}\n", p, ztd::shell::quote(vol->disp_name)));
                buf.append(std::format("set fm_panel{}_device_icon {}\n", p, ztd::shell::quote(vol->icon)));
                buf.append(std::format("set fm_panel{}_device_is_mounted {}\n", p, vol->is_mounted ? 1 : 0));
                buf.append(std::format("set fm_panel{}_device_is_optical {}\n", p, vol->is_optical ? 1 : 0));
                buf.append(std::format("set fm_panel{}_device_is_removable{}\n", p, vol->is_removable ? 1 : 0));
                buf.append(std::format("set fm_panel{}_device_is_mountable{}\n", p, vol->is_mountable ? 1 : 0));
                // clang-format on
            }
        }

        // tabs
        const i32 num_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_window->panel[p - 1]));
        for (const auto i : ztd::range(num_pages))
        {
            PtkFileBrowser* t_browser = PTK_FILE_BROWSER_REINTERPRET(
                gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_window->panel[p - 1]), i));
            const std::string path =
                ztd::shell::quote(ptk_file_browser_get_cwd(t_browser).string());
            buf.append(std::format("set fm_pwd_panel{}_tab[{}] {}\n", p, i + 1, path));
            if (p == file_browser->mypanel)
            {
                buf.append(std::format("set fm_pwd_tab[{}] {}\n", i + 1, path));
            }
            if (file_browser == t_browser)
            {
                // my browser
                buf.append(std::format("set fm_pwd {}\n", path));
                buf.append(std::format("set fm_panel {}\n", p));
                buf.append(std::format("set fm_tab {}\n", i + 1));
            }
        }
    }

    // my selected files
    buf.append("\n");
    buf.append(std::format("set fm_files (echo $fm_panel{}_files)\n", file_browser->mypanel));
    buf.append(std::format("set fm_file $fm_panel{}_files[1]\n", file_browser->mypanel));
    buf.append(std::format("set fm_filename $fm_filenames[1]\n"));
    buf.append("\n");

    // user
    buf.append(std::format("set fm_user {}\n", ztd::shell::quote(Glib::get_user_name())));

    // variable value
    buf.append(std::format("set fm_value {}\n", ztd::shell::quote(value)));
    if (vtask->exec_ptask)
    {
        buf.append(std::format("set fm_my_task {:p}\n", fmt::ptr(vtask->exec_ptask)));
        buf.append(std::format("set fm_my_task_id {:p}\n", fmt::ptr(vtask->exec_ptask)));
    }
    buf.append(std::format("set fm_my_window {:p}\n", fmt::ptr(main_window)));
    buf.append(std::format("set fm_my_window_id {:p}\n", fmt::ptr(main_window)));

    // utils
    buf.append(std::format("set fm_editor {}\n",
                           ztd::shell::quote(xset_get_s(xset::name::editor).value_or(""))));
    buf.append(std::format("set fm_editor_terminal {}\n", xset_get_b(xset::name::editor) ? 1 : 0));

    // set
    if (set)
    {
        // cmd_dir
        std::filesystem::path path;
        std::filesystem::path esc_path;

        if (set->plugin)
        {
            path = set->plugin->path / "files";
            if (!std::filesystem::exists(path))
            {
                path = set->plugin->path / set->plugin->name;
            }
        }
        else
        {
            path = vfs::user_dirs->program_config_dir() / "scripts" / set->name;
        }
        buf.append(std::format("set fm_cmd_dir {}\n", ztd::shell::quote(path.string())));

        // cmd_data
        path = vfs::user_dirs->program_config_dir() / "plugin-data" / set->name;
        buf.append(std::format("set fm_cmd_data {}\n", ztd::shell::quote(path.string())));

        // plugin_dir
        if (set->plugin)
        {
            buf.append(std::format("set fm_plugin_dir {}\n",
                                   ztd::shell::quote(set->plugin->path.string())));
        }

        // cmd_name
        if (set->menu_label)
        {
            buf.append(
                std::format("set fm_cmd_name {}\n", ztd::shell::quote(set->menu_label.value())));
        }
    }

    // tmp
    buf.append(std::format("set fm_tmp_dir {}\n",
                           ztd::shell::quote(vfs::user_dirs->program_tmp_dir().string())));

    // tasks
    PtkFileTask* ptask = get_selected_task(file_browser->task_view);
    if (ptask)
    {
        static constexpr std::array<const std::string_view, 7>
            job_titles{"move", "copy", "trash", "delete", "link", "change", "run"};
        buf.append("\n");
        buf.append(std::format("set fm_task_type {}\n", job_titles.at(ptask->task->type)));

        const auto dest_dir = ptask->task->dest_dir.value_or("");
        const auto current_file = ptask->task->current_file.value_or("");
        const auto current_dest = ptask->task->current_dest.value_or("");

        if (ptask->task->type == VFSFileTaskType::EXEC)
        {
            // clang-format off
            buf.append(std::format("set fm_task_pwd {}\n", ztd::shell::quote(dest_dir.string())));
            buf.append(std::format("set fm_task_name {}\n", ztd::shell::quote(current_file.string())));
            buf.append(std::format("set fm_task_command {}\n", ztd::shell::quote(ptask->task->exec_command)));
            buf.append(std::format("set fm_task_user {}\n", ztd::shell::quote(ptask->task->exec_as_user)));
            buf.append(std::format("set fm_task_icon {}\n", ztd::shell::quote(ptask->task->exec_icon)));
            buf.append(std::format("set fm_task_pid {}\n", ptask->task->exec_pid));
            // clang-format on
        }
        else
        {
            // clang-format off
            buf.append(std::format("set fm_task_dest_dir {}\n", ztd::shell::quote(dest_dir.string())));
            buf.append(std::format("set fm_task_current_src_file {}\n", ztd::shell::quote(current_file.string())));
            buf.append(std::format("set fm_task_current_dest_file {}\n", ztd::shell::quote(current_dest.string())));
            // clang-format on
        }
        buf.append(std::format("set fm_task_id {:p}\n", fmt::ptr(ptask)));
        if (ptask->task_view && (main_window = get_task_view_window(ptask->task_view)))
        {
            buf.append(std::format("set fm_task_window {:p}\n", fmt::ptr(main_window)));
            buf.append(std::format("set fm_task_window_id {:p}\n", fmt::ptr(main_window)));
        }
    }

    buf.append("\n\n");

    return buf;
}

static void
on_task_columns_changed(GtkWidget* view, void* user_data)
{
    (void)user_data;
    MainWindow* main_window = get_task_view_window(view);
    if (!main_window || !view)
    {
        return;
    }

    for (const auto i : ztd::range(task_names.size()))
    {
        GtkTreeViewColumn* col = gtk_tree_view_get_column(GTK_TREE_VIEW(view), i);
        if (!col)
        {
            return;
        }
        const char* title = gtk_tree_view_column_get_title(col);
        for (const auto [index, value] : ztd::enumerate(task_names))
        {
            if (ztd::same(title, task_titles.at(index)))
            {
                xset_t set = xset_get(value);
                // save column position
                xset_set_var(set, xset::var::x, std::to_string(i));
                // if the window was opened maximized and stayed maximized, or the
                // window is unmaximized and not fullscreen, save the columns
                if ((!main_window->maximized || main_window->opened_maximized) &&
                    !main_window->fullscreen)
                {
                    const i32 width = gtk_tree_view_column_get_width(col);
                    if (width) // manager unshown, all widths are zero
                    {
                        // save column width
                        xset_set_var(set, xset::var::y, std::to_string(width));
                    }
                }
                // set column visibility
                gtk_tree_view_column_set_visible(col, xset_get_b(value));

                break;
            }
        }
    }
}

static void
on_task_destroy(GtkWidget* view, void* user_data)
{
    (void)user_data;
    const u32 id = g_signal_lookup("columns-changed", G_TYPE_FROM_INSTANCE(view));
    if (id)
    {
        const u64 hand = g_signal_handler_find((void*)view,
                                               GSignalMatchType::G_SIGNAL_MATCH_ID,
                                               id,
                                               0,
                                               nullptr,
                                               nullptr,
                                               nullptr);
        if (hand)
        {
            g_signal_handler_disconnect((void*)view, hand);
        }
    }
    on_task_columns_changed(view, nullptr); // save widths
}

static void
on_task_column_selected(GtkMenuItem* item, GtkWidget* view)
{
    (void)item;
    on_task_columns_changed(view, nullptr);
}

static bool
main_tasks_running(MainWindow* main_window)
{
    if (!main_window->task_view || !GTK_IS_TREE_VIEW(main_window->task_view))
    {
        return false;
    }

    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(main_window->task_view));
    GtkTreeIter it;
    const bool ret = gtk_tree_model_get_iter_first(model, &it);

    return ret;
}

void
main_task_pause_all_queued(PtkFileTask* ptask)
{
    if (!ptask->task_view)
    {
        return;
    }

    PtkFileTask* qtask;
    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(ptask->task_view));
    GtkTreeIter it;
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            gtk_tree_model_get(model, &it, MainWindowTaskCol::TASK_COL_DATA, &qtask, -1);
            if (qtask && qtask != ptask && qtask->task && !qtask->complete &&
                qtask->task->state_pause == VFSFileTaskState::QUEUE)
            {
                ptk_file_task_pause(qtask, VFSFileTaskState::PAUSE);
            }
        } while (gtk_tree_model_iter_next(model, &it));
    }
}

void
main_task_start_queued(GtkWidget* view, PtkFileTask* new_ptask)
{
    GtkTreeIter it;
    PtkFileTask* qtask;
    GSList* running = nullptr;
    GSList* queued = nullptr;
    const bool smart = xset_get_b(xset::name::task_q_smart);
    if (!GTK_IS_TREE_VIEW(view))
    {
        return;
    }

    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            gtk_tree_model_get(model, &it, MainWindowTaskCol::TASK_COL_DATA, &qtask, -1);
            if (qtask && qtask->task && !qtask->complete &&
                qtask->task->state == VFSFileTaskState::RUNNING)
            {
                if (qtask->task->state_pause == VFSFileTaskState::QUEUE)
                {
                    queued = g_slist_append(queued, qtask);
                }
                else if (qtask->task->state_pause == VFSFileTaskState::RUNNING)
                {
                    running = g_slist_append(running, qtask);
                }
            }
        } while (gtk_tree_model_iter_next(model, &it));
    }

    if (new_ptask && new_ptask->task && !new_ptask->complete &&
        new_ptask->task->state_pause == VFSFileTaskState::QUEUE &&
        new_ptask->task->state == VFSFileTaskState::RUNNING)
    {
        queued = g_slist_append(queued, new_ptask);
    }

    if (!queued || (!smart && running))
    {
        g_slist_free(queued);
        g_slist_free(running);
        return;
    }

    if (!smart)
    {
        ptk_file_task_pause(PTK_FILE_TASK(queued->data), VFSFileTaskState::RUNNING);

        g_slist_free(queued);
        g_slist_free(running);
        return;
    }

    // smart
    for (GSList* q = queued; q; q = g_slist_next(q))
    {
        qtask = PTK_FILE_TASK(q->data);
        if (qtask)
        {
            // qtask has no devices so run it
            running = g_slist_append(running, qtask);
            ptk_file_task_pause(qtask, VFSFileTaskState::RUNNING);
            continue;
        }
    }
    g_slist_free(queued);
    g_slist_free(running);
}

static void
on_task_stop(GtkMenuItem* item, GtkWidget* view, xset_t set2, PtkFileTask* ptask2)
{
    GtkTreeModel* model = nullptr;
    GtkTreeIter it;
    PtkFileTask* ptask = nullptr;
    xset_t set;

    enum class MainWindowJob
    {
        JOB_STOP,
        JOB_PAUSE,
        JOB_QUEUE,
        JOB_RESUME
    };
    MainWindowJob job;

    if (item)
    {
        set = xset_get(static_cast<const char*>(g_object_get_data(G_OBJECT(item), "set")));
    }
    else
    {
        set = set2;
    }

    if (!set || !ztd::startswith(set->name, "task_"))
    {
        return;
    }

    if (ztd::startswith(set->name, "task_stop"))
    {
        job = MainWindowJob::JOB_STOP;
    }
    else if (ztd::startswith(set->name, "task_pause"))
    {
        job = MainWindowJob::JOB_PAUSE;
    }
    else if (ztd::startswith(set->name, "task_que"))
    {
        job = MainWindowJob::JOB_QUEUE;
    }
    else if (ztd::startswith(set->name, "task_resume"))
    {
        job = MainWindowJob::JOB_RESUME;
    }
    else
    {
        return;
    }

    const bool all = ztd::endswith(set->name, "_all");

    if (all)
    {
        model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
    }
    else
    {
        if (item)
        {
            ptask = PTK_FILE_TASK(g_object_get_data(G_OBJECT(item), "task"));
        }
        else
        {
            ptask = ptask2;
        }
        if (!ptask)
        {
            return;
        }
    }

    if (!model || gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            if (model)
            {
                gtk_tree_model_get(model, &it, MainWindowTaskCol::TASK_COL_DATA, &ptask, -1);
            }
            if (ptask && ptask->task && !ptask->complete &&
                (ptask->task->type != VFSFileTaskType::EXEC || ptask->task->exec_pid ||
                 job == MainWindowJob::JOB_STOP))
            {
                switch (job)
                {
                    case MainWindowJob::JOB_STOP:
                        ptk_file_task_cancel(ptask);
                        break;
                    case MainWindowJob::JOB_PAUSE:
                        ptk_file_task_pause(ptask, VFSFileTaskState::PAUSE);
                        break;
                    case MainWindowJob::JOB_QUEUE:
                        ptk_file_task_pause(ptask, VFSFileTaskState::QUEUE);
                        break;
                    case MainWindowJob::JOB_RESUME:
                        ptk_file_task_pause(ptask, VFSFileTaskState::RUNNING);
                        break;
                    default:
                        break;
                }
            }
        } while (model && gtk_tree_model_iter_next(model, &it));
    }
    main_task_start_queued(view, nullptr);
}

static bool
idle_set_task_height(MainWindow* main_window)
{
    GtkAllocation allocation;

    gtk_widget_get_allocation(GTK_WIDGET(main_window), &allocation);

    // set new config panel sizes to half of window
    if (!xset_is(xset::name::panel_sliders))
    {
        // this is not perfect because panel half-width is set before user
        // adjusts window size
        xset_t set = xset_get(xset::name::panel_sliders);
        set->x = std::to_string(allocation.width / 2);
        set->y = std::to_string(allocation.width / 2);
        set->s = std::to_string(allocation.height / 2);
    }

    // restore height (in case window height changed)
    i32 taskh = xset_get_int(xset::name::task_show_manager, xset::var::x); // task height >=0.9.2
    if (taskh == 0)
    {
        // use pre-0.9.2 slider pos to calculate height
        const i32 pos = xset_get_int(xset::name::panel_sliders, xset::var::z); // < 0.9.2 slider pos
        if (pos == 0)
        {
            taskh = 200;
        }
        else
        {
            taskh = allocation.height - pos;
        }
    }
    if (taskh > allocation.height / 2)
    {
        taskh = allocation.height / 2;
    }
    if (taskh < 1)
    {
        taskh = 90;
    }
    // ztd::logger::info("SHOW  win {}x{}    task height {}   slider {}", allocation.width,
    // allocation.height, taskh, allocation.height - taskh);
    gtk_paned_set_position(GTK_PANED(main_window->task_vpane), allocation.height - taskh);
    return false;
}

static void
show_task_manager(MainWindow* main_window, bool show)
{
    GtkAllocation allocation;

    gtk_widget_get_allocation(GTK_WIDGET(main_window), &allocation);

    if (show)
    {
        if (!gtk_widget_get_visible(GTK_WIDGET(main_window->task_scroll)))
        {
            gtk_widget_show(main_window->task_scroll);
            // allow vpane to auto-adjust before setting new slider pos
            g_idle_add((GSourceFunc)idle_set_task_height, main_window);
        }
    }
    else
    {
        // save height
        if (gtk_widget_get_visible(GTK_WIDGET(main_window->task_scroll)))
        {
            const i32 pos = gtk_paned_get_position(GTK_PANED(main_window->task_vpane));
            if (pos)
            {
                // save slider pos for version < 0.9.2 (in case of downgrade)
                xset_set(xset::name::panel_sliders, xset::var::z, std::to_string(pos));
                // save absolute height introduced v0.9.2
                xset_set(xset::name::task_show_manager,
                         xset::var::x,
                         std::to_string(allocation.height - pos));
                // ztd::logger::info("HIDE  win {}x{}    task height {}   slider {}",
                // allocation.width, allocation.height, allocation.height - pos, pos);
            }
        }
        // hide
        const bool tasks_has_focus = gtk_widget_is_focus(GTK_WIDGET(main_window->task_view));
        gtk_widget_hide(GTK_WIDGET(main_window->task_scroll));
        if (tasks_has_focus)
        {
            // focus the file list
            PtkFileBrowser* file_browser =
                PTK_FILE_BROWSER_REINTERPRET(main_window_get_current_file_browser(main_window));
            if (file_browser)
            {
                gtk_widget_grab_focus(file_browser->folder_view);
            }
        }
    }
}

static void
on_task_popup_show(GtkMenuItem* item, MainWindow* main_window, const char* name2)
{
    GtkTreeModel* model = nullptr;
    GtkTreeIter it;
    const char* name = nullptr;

    if (item)
    {
        name = static_cast<const char*>(g_object_get_data(G_OBJECT(item), "name"));
    }
    else
    {
        name = name2;
    }

    if (name)
    {
        const xset::name xset_name = xset::get_xsetname_from_name(name);

        if (xset_name == xset::name::task_show_manager)
        {
            if (xset_get_b(xset::name::task_show_manager))
            {
                xset_set_b(xset::name::task_hide_manager, false);
            }
            else
            {
                xset_set_b(xset::name::task_hide_manager, true);
                xset_set_b(xset::name::task_show_manager, false);
            }
        }
        else
        {
            if (xset_get_b(xset::name::task_hide_manager))
            {
                xset_set_b(xset::name::task_show_manager, false);
            }
            else
            {
                xset_set_b(xset::name::task_hide_manager, false);
                xset_set_b(xset::name::task_show_manager, true);
            }
        }
    }

    if (xset_get_b(xset::name::task_show_manager))
    {
        show_task_manager(main_window, true);
    }
    else
    {
        model = gtk_tree_view_get_model(GTK_TREE_VIEW(main_window->task_view));
        if (gtk_tree_model_get_iter_first(model, &it))
        {
            show_task_manager(main_window, true);
        }
        else if (xset_get_b(xset::name::task_hide_manager))
        {
            show_task_manager(main_window, false);
        }
    }
}

static void
on_task_popup_errset(GtkMenuItem* item, MainWindow* main_window, const char* name2)
{
    (void)main_window;
    const char* name;
    if (item)
    {
        name = static_cast<const char*>(g_object_get_data(G_OBJECT(item), "name"));
    }
    else
    {
        name = name2;
    }

    if (!name)
    {
        return;
    }

    const xset::name xset_name = xset::get_xsetname_from_name(name);

    if (xset_name == xset::name::task_err_first)
    {
        if (xset_get_b(xset::name::task_err_first))
        {
            xset_set_b(xset::name::task_err_any, false);
            xset_set_b(xset::name::task_err_cont, false);
        }
        else
        {
            xset_set_b(xset::name::task_err_any, false);
            xset_set_b(xset::name::task_err_cont, true);
        }
    }
    else if (xset_name == xset::name::task_err_any)
    {
        if (xset_get_b(xset::name::task_err_any))
        {
            xset_set_b(xset::name::task_err_first, false);
            xset_set_b(xset::name::task_err_cont, false);
        }
        else
        {
            xset_set_b(xset::name::task_err_first, false);
            xset_set_b(xset::name::task_err_cont, true);
        }
    }
    else
    {
        if (xset_get_b(xset::name::task_err_cont))
        {
            xset_set_b(xset::name::task_err_first, false);
            xset_set_b(xset::name::task_err_any, false);
        }
        else
        {
            xset_set_b(xset::name::task_err_first, true);
            xset_set_b(xset::name::task_err_any, false);
        }
    }
}

static void
main_task_prepare_menu(MainWindow* main_window, GtkWidget* menu, GtkAccelGroup* accel_group)
{
    (void)menu;
    (void)accel_group;
    xset_t set;
    xset_t set_radio;

    GtkWidget* parent = main_window->task_view;
    set = xset_get(xset::name::task_show_manager);
    xset_set_cb(set, (GFunc)on_task_popup_show, main_window);
    xset_set_ob1(set, "name", set->name.data());
    xset_set_ob2(set, nullptr, nullptr);
    set_radio = set;
    set = xset_get(xset::name::task_hide_manager);
    xset_set_cb(set, (GFunc)on_task_popup_show, main_window);
    xset_set_ob1(set, "name", set->name.data());
    xset_set_ob2(set, nullptr, set_radio->name.data());

    xset_set_cb(xset::name::task_col_count, (GFunc)on_task_column_selected, parent);
    xset_set_cb(xset::name::task_col_path, (GFunc)on_task_column_selected, parent);
    xset_set_cb(xset::name::task_col_file, (GFunc)on_task_column_selected, parent);
    xset_set_cb(xset::name::task_col_to, (GFunc)on_task_column_selected, parent);
    xset_set_cb(xset::name::task_col_progress, (GFunc)on_task_column_selected, parent);
    xset_set_cb(xset::name::task_col_total, (GFunc)on_task_column_selected, parent);
    xset_set_cb(xset::name::task_col_started, (GFunc)on_task_column_selected, parent);
    xset_set_cb(xset::name::task_col_elapsed, (GFunc)on_task_column_selected, parent);
    xset_set_cb(xset::name::task_col_curspeed, (GFunc)on_task_column_selected, parent);
    xset_set_cb(xset::name::task_col_curest, (GFunc)on_task_column_selected, parent);
    xset_set_cb(xset::name::task_col_avgspeed, (GFunc)on_task_column_selected, parent);
    xset_set_cb(xset::name::task_col_avgest, (GFunc)on_task_column_selected, parent);
    xset_set_cb(xset::name::task_col_reorder, (GFunc)on_reorder, parent);

    set = xset_get(xset::name::task_err_first);
    xset_set_cb(set, (GFunc)on_task_popup_errset, main_window);
    xset_set_ob1(set, "name", set->name.data());
    xset_set_ob2(set, nullptr, nullptr);
    set_radio = set;
    set = xset_get(xset::name::task_err_any);
    xset_set_cb(set, (GFunc)on_task_popup_errset, main_window);
    xset_set_ob1(set, "name", set->name.data());
    xset_set_ob2(set, nullptr, set_radio->name.data());
    set = xset_get(xset::name::task_err_cont);
    xset_set_cb(set, (GFunc)on_task_popup_errset, main_window);
    xset_set_ob1(set, "name", set->name.data());
    xset_set_ob2(set, nullptr, set_radio->name.data());
}

static PtkFileTask*
get_selected_task(GtkWidget* view)
{
    GtkTreeModel* model;
    GtkTreeSelection* tree_sel;
    GtkTreeIter it;
    PtkFileTask* ptask = nullptr;

    if (!view)
    {
        return nullptr;
    }
    MainWindow* main_window = get_task_view_window(view);
    if (!main_window)
    {
        return nullptr;
    }

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
    tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    if (gtk_tree_selection_get_selected(tree_sel, nullptr, &it))
    {
        gtk_tree_model_get(model, &it, MainWindowTaskCol::TASK_COL_DATA, &ptask, -1);
    }
    return ptask;
}

static void
show_task_dialog(GtkWidget* widget, GtkWidget* view)
{
    (void)widget;
    PtkFileTask* ptask = get_selected_task(view);
    if (!ptask)
    {
        return;
    }

    ptk_file_task_lock(ptask);
    ptk_file_task_progress_open(ptask);
    if (ptask->task->state_pause != VFSFileTaskState::RUNNING)
    {
        // update dlg
        ptask->pause_change = true;
        ptask->progress_count = 50; // trigger fast display
    }
    if (ptask->progress_dlg)
    {
        gtk_window_present(GTK_WINDOW(ptask->progress_dlg));
    }
    ptk_file_task_unlock(ptask);
}

static bool
on_task_button_press_event(GtkWidget* view, GdkEventButton* event, MainWindow* main_window)
{
    GtkTreeModel* model = nullptr;
    GtkTreePath* tree_path;
    GtkTreeViewColumn* col = nullptr;
    GtkTreeIter it;
    PtkFileTask* ptask = nullptr;
    xset_t set;
    bool is_tasks;

    if (event->type != GdkEventType::GDK_BUTTON_PRESS)
    {
        return false;
    }

    if ((event_handler->win_click->s || event_handler->win_click->ob2_data) &&
        main_window_event(main_window,
                          event_handler->win_click,
                          xset::name::evt_win_click,
                          0,
                          0,
                          "tasklist",
                          0,
                          event->button,
                          event->state,
                          true))
    {
        return false;
    }

    xset_context_t context;
    std::string menu_elements;

    switch (event->button)
    {
        case 1:
        case 2:
            // left or middle click
            // get selected task
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
            // ztd::logger::info("x = {}  y = {}", event->x, event->y);
            // due to bug in gtk_tree_view_get_path_at_pos (gtk 2.24), a click
            // on the column header resize divider registers as a click on the
            // first row first column.  So if event->x < 7 ignore
            if (event->x < 7)
            {
                return false;
            }
            if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view),
                                               event->x,
                                               event->y,
                                               &tree_path,
                                               &col,
                                               nullptr,
                                               nullptr))
            {
                return false;
            }
            if (tree_path && gtk_tree_model_get_iter(model, &it, tree_path))
            {
                gtk_tree_model_get(model, &it, MainWindowTaskCol::TASK_COL_DATA, &ptask, -1);
            }
            gtk_tree_path_free(tree_path);

            if (!ptask)
            {
                return false;
            }
            if (event->button == 1 && !ztd::same(gtk_tree_view_column_get_title(col), "Status"))
            {
                return false;
            }
            xset::name sname;
            switch (ptask->task->state_pause)
            {
                case VFSFileTaskState::PAUSE:
                    sname = xset::name::task_que;
                    break;
                case VFSFileTaskState::QUEUE:
                    sname = xset::name::task_resume;
                    break;
                case VFSFileTaskState::RUNNING:
                case VFSFileTaskState::SIZE_TIMEOUT:
                case VFSFileTaskState::QUERY_OVERWRITE:
                case VFSFileTaskState::ERROR:
                case VFSFileTaskState::FINISH:
                default:
                    sname = xset::name::task_pause;
            }
            set = xset_get(sname);
            on_task_stop(nullptr, view, set, ptask);
            return true;
            break;
        case 3:
            // get selected task
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
            if ((is_tasks = gtk_tree_model_get_iter_first(model, &it)))
            {
                if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view),
                                                  event->x,
                                                  event->y,
                                                  &tree_path,
                                                  &col,
                                                  nullptr,
                                                  nullptr))
                {
                    if (tree_path && gtk_tree_model_get_iter(model, &it, tree_path))
                    {
                        gtk_tree_model_get(model,
                                           &it,
                                           MainWindowTaskCol::TASK_COL_DATA,
                                           &ptask,
                                           -1);
                    }
                    gtk_tree_path_free(tree_path);
                }
            }

            // build popup
            PtkFileBrowser* file_browser;
            file_browser =
                PTK_FILE_BROWSER_REINTERPRET(main_window_get_current_file_browser(main_window));
            if (!file_browser)
            {
                return false;
            }
            GtkWidget* popup;
            GtkAccelGroup* accel_group;
            popup = gtk_menu_new();
            accel_group = gtk_accel_group_new();
            context = xset_context_new();
            main_context_fill(file_browser, context);

            set = xset_get(xset::name::task_stop);
            xset_set_cb(set, (GFunc)on_task_stop, view);
            xset_set_ob1(set, "task", ptask);
            set->disable = !ptask;

            set = xset_get(xset::name::task_pause);
            xset_set_cb(set, (GFunc)on_task_stop, view);
            xset_set_ob1(set, "task", ptask);
            set->disable = (!ptask || ptask->task->state_pause == VFSFileTaskState::PAUSE ||
                            (ptask->task->type == VFSFileTaskType::EXEC && !ptask->task->exec_pid));

            set = xset_get(xset::name::task_que);
            xset_set_cb(set, (GFunc)on_task_stop, view);
            xset_set_ob1(set, "task", ptask);
            set->disable = (!ptask || ptask->task->state_pause == VFSFileTaskState::QUEUE ||
                            (ptask->task->type == VFSFileTaskType::EXEC && !ptask->task->exec_pid));

            set = xset_get(xset::name::task_resume);
            xset_set_cb(set, (GFunc)on_task_stop, view);
            xset_set_ob1(set, "task", ptask);
            set->disable = (!ptask || ptask->task->state_pause == VFSFileTaskState::RUNNING ||
                            (ptask->task->type == VFSFileTaskType::EXEC && !ptask->task->exec_pid));

            xset_set_cb(xset::name::task_stop_all, (GFunc)on_task_stop, view);
            xset_set_cb(xset::name::task_pause_all, (GFunc)on_task_stop, view);
            xset_set_cb(xset::name::task_que_all, (GFunc)on_task_stop, view);
            xset_set_cb(xset::name::task_resume_all, (GFunc)on_task_stop, view);
            set = xset_get(xset::name::task_all);
            set->disable = !is_tasks;

            const char* showout;
            showout = ztd::strdup("");
            if (ptask && ptask->pop_handler)
            {
                xset_set_cb(xset::name::task_showout, (GFunc)show_task_dialog, view);
                showout = ztd::strdup(" task_showout");
            }

            main_task_prepare_menu(main_window, popup, accel_group);

            menu_elements = std::format(
                "task_stop separator task_pause task_que task_resume{} task_all separator "
                "task_show_manager "
                "task_hide_manager separator task_columns task_popups task_errors task_queue",
                showout);
            xset_add_menu(file_browser, popup, accel_group, menu_elements);

            gtk_widget_show_all(GTK_WIDGET(popup));
            g_signal_connect(popup, "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);
            g_signal_connect(popup, "key_press_event", G_CALLBACK(xset_menu_keypress), nullptr);
            gtk_menu_popup_at_pointer(GTK_MENU(popup), nullptr);
            // right click
            break;
        default:
            break;
    }

    return false;
}

static void
on_task_row_activated(GtkWidget* view, GtkTreePath* tree_path, GtkTreeViewColumn* col,
                      void* user_data)
{
    (void)col;
    (void)user_data;
    GtkTreeModel* model;
    GtkTreeIter it;
    PtkFileTask* ptask;

    MainWindow* main_window = get_task_view_window(view);
    if (!main_window)
    {
        return;
    }

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
    if (!gtk_tree_model_get_iter(model, &it, tree_path))
    {
        return;
    }

    gtk_tree_model_get(model, &it, MainWindowTaskCol::TASK_COL_DATA, &ptask, -1);
    if (ptask)
    {
        if (ptask->pop_handler)
        {
            // show custom dialog
            ztd::logger::info("TASK_POPUP >>> {}", ptask->pop_handler);
            const std::string command = std::format("{} -c {}", FISH_PATH, ptask->pop_handler);
            Glib::spawn_command_line_async(command);
        }
        else
        {
            // show normal dialog
            show_task_dialog(nullptr, view);
        }
    }
}

void
main_task_view_remove_task(PtkFileTask* ptask)
{
    // ztd::logger::info("main_task_view_remove_task  ptask={}", ptask);

    GtkWidget* view = ptask->task_view;
    if (!view)
    {
        return;
    }

    MainWindow* main_window = get_task_view_window(view);
    if (!main_window)
    {
        return;
    }

    PtkFileTask* ptaskt = nullptr;
    GtkTreeIter it;

    GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            gtk_tree_model_get(model, &it, MainWindowTaskCol::TASK_COL_DATA, &ptaskt, -1);
        } while (ptaskt != ptask && gtk_tree_model_iter_next(model, &it));
    }
    if (ptaskt == ptask)
    {
        gtk_list_store_remove(GTK_LIST_STORE(model), &it);
    }

    if (!gtk_tree_model_get_iter_first(model, &it))
    {
        if (xset_get_b(xset::name::task_hide_manager))
        {
            show_task_manager(main_window, false);
        }
    }

    update_window_title(nullptr, main_window);
    // ztd::logger::info("main_task_view_remove_task DONE ptask={}", ptask);
}

void
main_task_view_update_task(PtkFileTask* ptask)
{
    PtkFileTask* ptaskt = nullptr;
    GtkWidget* view;
    GtkTreeModel* model;
    GtkTreeIter it;
    GdkPixbuf* pixbuf;
    xset_t set;

    // ztd::logger::info("main_task_view_update_task  ptask={}", ptask);
    static constexpr std::array<const std::string_view, 7> job_titles{
        "moving",
        "copying",
        "trashing",
        "deleting",
        "linking",
        "changing",
        "running",
    };

    if (!ptask)
    {
        return;
    }

    view = ptask->task_view;
    if (!view)
    {
        return;
    }

    MainWindow* main_window = get_task_view_window(view);
    if (!main_window)
    {
        return;
    }

    std::filesystem::path dest_dir;
    if (ptask->task->type != VFSFileTaskType::EXEC)
    {
        if (ptask->task->dest_dir)
        {
            dest_dir = ptask->task->dest_dir.value();
        }
    }

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            gtk_tree_model_get(model, &it, MainWindowTaskCol::TASK_COL_DATA, &ptaskt, -1);
        } while (ptaskt != ptask && gtk_tree_model_iter_next(model, &it));
    }
    if (ptaskt != ptask)
    {
        // new row
        std::tm* local_time = std::localtime(&ptask->task->start_time);
        std::ostringstream started;
        started << std::put_time(local_time, "%H:%M");

        gtk_list_store_insert_with_values(GTK_LIST_STORE(model),
                                          &it,
                                          0,
                                          MainWindowTaskCol::TASK_COL_TO,
                                          dest_dir.empty() ? nullptr : dest_dir.c_str(),
                                          MainWindowTaskCol::TASK_COL_STARTED,
                                          started.str().data(),
                                          MainWindowTaskCol::TASK_COL_STARTTIME,
                                          (i64)ptask->task->start_time,
                                          MainWindowTaskCol::TASK_COL_DATA,
                                          ptask,
                                          -1);
    }

    if (ptask->task->state_pause == VFSFileTaskState::RUNNING || ptask->pause_change_view)
    {
        // update row
        std::string path;
        std::string file;

        i32 percent = ptask->task->percent;
        if (percent < 0)
        {
            percent = 0;
        }
        else if (percent > 100)
        {
            percent = 100;
        }
        if (ptask->task->type != VFSFileTaskType::EXEC)
        {
            if (ptask->task->current_file)
            {
                const auto current_file = ptask->task->current_file.value();
                path = current_file.parent_path();
                file = current_file.filename();
            }
        }
        else
        {
            const auto current_file = ptask->task->current_file.value();

            path = ptask->task->dest_dir.value(); // cwd
            file = std::format("( {} )", current_file.string());
        }

        // status
        std::string status;
        char* status3;
        if (ptask->task->type != VFSFileTaskType::EXEC)
        {
            if (!ptask->err_count)
            {
                status = job_titles.at(ptask->task->type);
            }
            else
            {
                status =
                    std::format("{} error {}", ptask->err_count, job_titles.at(ptask->task->type));
            }
        }
        else
        {
            // exec task
            if (!ptask->task->exec_action.empty())
            {
                status = ptask->task->exec_action;
            }
            else
            {
                status = job_titles.at(ptask->task->type);
            }
        }

        if (ptask->task->state_pause == VFSFileTaskState::PAUSE)
        {
            const std::string str = std::format("paused {}", status);
            status3 = ztd::strdup(str);
        }
        else if (ptask->task->state_pause == VFSFileTaskState::QUEUE)
        {
            const std::string str = std::format("queued {}", status);
            status3 = ztd::strdup(str);
        }
        else
        {
            status3 = ztd::strdup(status);
        }

        // update icon if queue state changed
        pixbuf = nullptr;
        if (ptask->pause_change_view)
        {
            // icon
            std::string iname;
            if (ptask->task->state_pause == VFSFileTaskState::PAUSE)
            {
                set = xset_get(xset::name::task_pause);
                iname = set->icon ? set->icon.value() : "media-playback-pause";
            }
            else if (ptask->task->state_pause == VFSFileTaskState::QUEUE)
            {
                set = xset_get(xset::name::task_que);
                iname = set->icon ? set->icon.value() : "list-add";
            }
            else if (ptask->err_count && ptask->task->type != VFSFileTaskType::EXEC)
            {
                iname = "error";
            }
            else if (ptask->task->type == 0 || ptask->task->type == 1 || ptask->task->type == 4)
            {
                iname = "stock_copy";
            }
            else if (ptask->task->type == 2 || ptask->task->type == 3)
            {
                iname = "stock_delete";
            }
            else if (ptask->task->type == VFSFileTaskType::EXEC && !ptask->task->exec_icon.empty())
            {
                iname = ptask->task->exec_icon;
            }
            else
            {
                iname = "gtk-execute";
            }

            i32 icon_size = app_settings.get_icon_size_small();
            if (icon_size > PANE_MAX_ICON_SIZE)
            {
                icon_size = PANE_MAX_ICON_SIZE;
            }

            GtkIconTheme* icon_theme = gtk_icon_theme_get_default();

            pixbuf = gtk_icon_theme_load_icon(
                icon_theme,
                iname.data(),
                icon_size,
                (GtkIconLookupFlags)GtkIconLookupFlags::GTK_ICON_LOOKUP_USE_BUILTIN,
                nullptr);
            if (!pixbuf)
            {
                pixbuf = gtk_icon_theme_load_icon(
                    icon_theme,
                    "gtk-execute",
                    icon_size,
                    (GtkIconLookupFlags)GtkIconLookupFlags::GTK_ICON_LOOKUP_USE_BUILTIN,
                    nullptr);
            }
            ptask->pause_change_view = false;
        }

        if (ptask->task->type != VFSFileTaskType::EXEC || ptaskt != ptask /* new task */)
        {
            if (pixbuf)
            {
                gtk_list_store_set(GTK_LIST_STORE(model),
                                   &it,
                                   MainWindowTaskCol::TASK_COL_ICON,
                                   pixbuf,
                                   MainWindowTaskCol::TASK_COL_STATUS,
                                   status3,
                                   MainWindowTaskCol::TASK_COL_COUNT,
                                   ptask->dsp_file_count.data(),
                                   MainWindowTaskCol::TASK_COL_PATH,
                                   path.data(),
                                   MainWindowTaskCol::TASK_COL_FILE,
                                   file.data(),
                                   MainWindowTaskCol::TASK_COL_PROGRESS,
                                   percent,
                                   MainWindowTaskCol::TASK_COL_TOTAL,
                                   ptask->dsp_size_tally.data(),
                                   MainWindowTaskCol::TASK_COL_ELAPSED,
                                   ptask->dsp_elapsed.data(),
                                   MainWindowTaskCol::TASK_COL_CURSPEED,
                                   ptask->dsp_curspeed.data(),
                                   MainWindowTaskCol::TASK_COL_CUREST,
                                   ptask->dsp_curest.data(),
                                   MainWindowTaskCol::TASK_COL_AVGSPEED,
                                   ptask->dsp_avgspeed.data(),
                                   MainWindowTaskCol::TASK_COL_AVGEST,
                                   ptask->dsp_avgest.data(),
                                   -1);
            }
            else
            {
                gtk_list_store_set(GTK_LIST_STORE(model),
                                   &it,
                                   MainWindowTaskCol::TASK_COL_STATUS,
                                   status3,
                                   MainWindowTaskCol::TASK_COL_COUNT,
                                   ptask->dsp_file_count.data(),
                                   MainWindowTaskCol::TASK_COL_PATH,
                                   path.data(),
                                   MainWindowTaskCol::TASK_COL_FILE,
                                   file.data(),
                                   MainWindowTaskCol::TASK_COL_PROGRESS,
                                   percent,
                                   MainWindowTaskCol::TASK_COL_TOTAL,
                                   ptask->dsp_size_tally.data(),
                                   MainWindowTaskCol::TASK_COL_ELAPSED,
                                   ptask->dsp_elapsed.data(),
                                   MainWindowTaskCol::TASK_COL_CURSPEED,
                                   ptask->dsp_curspeed.data(),
                                   MainWindowTaskCol::TASK_COL_CUREST,
                                   ptask->dsp_curest.data(),
                                   MainWindowTaskCol::TASK_COL_AVGSPEED,
                                   ptask->dsp_avgspeed.data(),
                                   MainWindowTaskCol::TASK_COL_AVGEST,
                                   ptask->dsp_avgest.data(),
                                   -1);
            }
        }
        else if (pixbuf)
        {
            gtk_list_store_set(GTK_LIST_STORE(model),
                               &it,
                               MainWindowTaskCol::TASK_COL_ICON,
                               pixbuf,
                               MainWindowTaskCol::TASK_COL_STATUS,
                               status3,
                               MainWindowTaskCol::TASK_COL_PROGRESS,
                               percent,
                               MainWindowTaskCol::TASK_COL_ELAPSED,
                               ptask->dsp_elapsed.data(),
                               -1);
        }
        else
        {
            gtk_list_store_set(GTK_LIST_STORE(model),
                               &it,
                               MainWindowTaskCol::TASK_COL_STATUS,
                               status3,
                               MainWindowTaskCol::TASK_COL_PROGRESS,
                               percent,
                               MainWindowTaskCol::TASK_COL_ELAPSED,
                               ptask->dsp_elapsed.data(),
                               -1);
        }

        // Clearing up
        std::free(status3);
        if (pixbuf)
        {
            g_object_unref(pixbuf);
        }

        if (!gtk_widget_get_visible(gtk_widget_get_parent(GTK_WIDGET(view))))
        {
            show_task_manager(main_window, true);
        }

        update_window_title(nullptr, main_window);
    }
    else
    {
        // task is paused
        gtk_list_store_set(GTK_LIST_STORE(model),
                           &it,
                           MainWindowTaskCol::TASK_COL_TOTAL,
                           ptask->dsp_size_tally.data(),
                           MainWindowTaskCol::TASK_COL_ELAPSED,
                           ptask->dsp_elapsed.data(),
                           MainWindowTaskCol::TASK_COL_CURSPEED,
                           ptask->dsp_curspeed.data(),
                           MainWindowTaskCol::TASK_COL_CUREST,
                           ptask->dsp_curest.data(),
                           MainWindowTaskCol::TASK_COL_AVGSPEED,
                           ptask->dsp_avgspeed.data(),
                           MainWindowTaskCol::TASK_COL_AVGEST,
                           ptask->dsp_avgest.data(),
                           -1);
    }
    // ztd::logger::info("DONE main_task_view_update_task");
}

static GtkWidget*
main_task_view_new(MainWindow* main_window)
{
    GtkTreeViewColumn* col;
    GtkCellRenderer* renderer;
    GtkCellRenderer* pix_renderer;

    static constexpr std::array<MainWindowTaskCol, 16> cols{
        MainWindowTaskCol::TASK_COL_STATUS,
        MainWindowTaskCol::TASK_COL_COUNT,
        MainWindowTaskCol::TASK_COL_PATH,
        MainWindowTaskCol::TASK_COL_FILE,
        MainWindowTaskCol::TASK_COL_TO,
        MainWindowTaskCol::TASK_COL_PROGRESS,
        MainWindowTaskCol::TASK_COL_TOTAL,
        MainWindowTaskCol::TASK_COL_STARTED,
        MainWindowTaskCol::TASK_COL_ELAPSED,
        MainWindowTaskCol::TASK_COL_CURSPEED,
        MainWindowTaskCol::TASK_COL_CUREST,
        MainWindowTaskCol::TASK_COL_AVGSPEED,
        MainWindowTaskCol::TASK_COL_AVGEST,
        MainWindowTaskCol::TASK_COL_STARTTIME,
        MainWindowTaskCol::TASK_COL_ICON,
        MainWindowTaskCol::TASK_COL_DATA,
    };

    // Model
    GtkListStore* list = gtk_list_store_new(cols.size(),
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_INT,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING,
                                            G_TYPE_INT64,
                                            GDK_TYPE_PIXBUF,
                                            G_TYPE_POINTER);

    // View
    GtkWidget* view = gtk_tree_view_new();
    gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(list));
    // gtk_tree_view_set_model adds a ref
    g_object_unref(list);
    // gtk_tree_view_set_single_click(GTK_TREE_VIEW(view), true);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(view), false);
    // gtk_tree_view_set_single_click_timeout(GTK_TREE_VIEW(view), SINGLE_CLICK_TIMEOUT);

    // Columns
    for (const auto i : ztd::range(task_names.size()))
    {
        col = gtk_tree_view_column_new();
        gtk_tree_view_column_set_resizable(col, true);
        gtk_tree_view_column_set_sizing(col, GtkTreeViewColumnSizing::GTK_TREE_VIEW_COLUMN_FIXED);
        gtk_tree_view_column_set_min_width(col, 20);

        // column order
        i32 j = 0;
        for (const auto [index, value] : ztd::enumerate(task_names))
        {
            if (xset_get_int(value, xset::var::x) == i)
            {
                // column width
                i32 width = xset_get_int(value, xset::var::y);
                if (width == 0)
                {
                    width = 80;
                }
                gtk_tree_view_column_set_fixed_width(col, width);

                j = index;

                break;
            }
        }

        GValue val;

        switch (cols.at(j))
        {
            case MainWindowTaskCol::TASK_COL_STATUS:
                // Icon and Text
                renderer = gtk_cell_renderer_text_new();
                pix_renderer = gtk_cell_renderer_pixbuf_new();
                gtk_tree_view_column_pack_start(col, pix_renderer, false);
                gtk_tree_view_column_pack_end(col, renderer, true);
                gtk_tree_view_column_set_attributes(col,
                                                    pix_renderer,
                                                    "pixbuf",
                                                    MainWindowTaskCol::TASK_COL_ICON,
                                                    nullptr);
                gtk_tree_view_column_set_attributes(col,
                                                    renderer,
                                                    "text",
                                                    MainWindowTaskCol::TASK_COL_STATUS,
                                                    nullptr);
                gtk_tree_view_column_set_expand(col, false);
                gtk_tree_view_column_set_sizing(
                    col,
                    GtkTreeViewColumnSizing::GTK_TREE_VIEW_COLUMN_FIXED);
                gtk_tree_view_column_set_min_width(col, 60);
                break;
            case MainWindowTaskCol::TASK_COL_PROGRESS:
                // Progress Bar
                renderer = gtk_cell_renderer_progress_new();
                gtk_tree_view_column_pack_start(col, renderer, true);
                gtk_tree_view_column_set_attributes(col, renderer, "value", cols.at(j), nullptr);
                break;
            case MainWindowTaskCol::TASK_COL_PATH:
            case MainWindowTaskCol::TASK_COL_FILE:
            case MainWindowTaskCol::TASK_COL_TO:
                // Text Column
                renderer = gtk_cell_renderer_text_new();
                gtk_tree_view_column_pack_start(col, renderer, true);
                gtk_tree_view_column_set_attributes(col, renderer, "text", cols.at(j), nullptr);

                // ellipsize
                val = GValue();
                g_value_init(&val, G_TYPE_CHAR);
                g_value_set_schar(&val, PangoEllipsizeMode::PANGO_ELLIPSIZE_MIDDLE);
                g_object_set_property(G_OBJECT(renderer), "ellipsize", &val);
                g_value_unset(&val);
                break;
            case MainWindowTaskCol::TASK_COL_COUNT:
            case MainWindowTaskCol::TASK_COL_TOTAL:
            case MainWindowTaskCol::TASK_COL_STARTED:
            case MainWindowTaskCol::TASK_COL_ELAPSED:
            case MainWindowTaskCol::TASK_COL_CURSPEED:
            case MainWindowTaskCol::TASK_COL_CUREST:
            case MainWindowTaskCol::TASK_COL_AVGSPEED:
            case MainWindowTaskCol::TASK_COL_AVGEST:
            case MainWindowTaskCol::TASK_COL_STARTTIME:
            case MainWindowTaskCol::TASK_COL_ICON:
            case MainWindowTaskCol::TASK_COL_DATA:
            default:
                // Text Column
                renderer = gtk_cell_renderer_text_new();
                gtk_tree_view_column_pack_start(col, renderer, true);
                gtk_tree_view_column_set_attributes(col, renderer, "text", cols.at(j), nullptr);
                break;
        }

        gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
        gtk_tree_view_column_set_title(col, task_titles.at(j).data());
        gtk_tree_view_column_set_reorderable(col, true);
        gtk_tree_view_column_set_visible(col, xset_get_b(task_names.at(j)));
        if (j == MainWindowTaskCol::TASK_COL_FILE) //|| j == MainWindowTaskCol::TASK_COL_PATH || j
                                                   //== MainWindowTaskCol::TASK_COL_TO
        {
            gtk_tree_view_column_set_sizing(col,
                                            GtkTreeViewColumnSizing::GTK_TREE_VIEW_COLUMN_FIXED);
            gtk_tree_view_column_set_min_width(col, 20);
            // If set_expand is true, columns flicker and adjustment is
            // difficult during high i/o load on some systems
            gtk_tree_view_column_set_expand(col, false);
        }
    }

    // invisible Starttime col for sorting
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_resizable(col, true);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, true);
    gtk_tree_view_column_set_attributes(col,
                                        renderer,
                                        "text",
                                        MainWindowTaskCol::TASK_COL_STARTTIME,
                                        nullptr);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
    gtk_tree_view_column_set_title(col, "StartTime");
    gtk_tree_view_column_set_reorderable(col, false);
    gtk_tree_view_column_set_visible(col, false);

    // Sort
    if (GTK_IS_TREE_SORTABLE(list))
    {
        gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(list),
                                             MainWindowTaskCol::TASK_COL_STARTTIME,
                                             GtkSortType::GTK_SORT_ASCENDING);
    }

    g_signal_connect(view, "row-activated", G_CALLBACK(on_task_row_activated), nullptr);
    g_signal_connect(view, "columns-changed", G_CALLBACK(on_task_columns_changed), nullptr);
    g_signal_connect(view, "destroy", G_CALLBACK(on_task_destroy), nullptr);
    g_signal_connect(view,
                     "button-press-event",
                     G_CALLBACK(on_task_button_press_event),
                     main_window);

    return view;
}

// ============== socket commands

static bool
get_bool(const std::string_view value)
{
    if (ztd::same(ztd::lower(value), "yes") || ztd::same(value, "1"))
    {
        return true;
    }
    else if (ztd::same(ztd::lower(value), "no") || ztd::same(value, "0"))
    {
        return false;
    }

    // throw std::logic_error("");
    ztd::logger::warn("socket command defaulting to false, invalid value: {}", value);
    ztd::logger::info("supported socket command values are 'yes|1|no|0");
    return false;
}

static const std::string
unescape(const std::string_view t)
{
    std::string unescaped = t.data();
    unescaped = ztd::replace(unescaped, "\\\n", "\\n");
    unescaped = ztd::replace(unescaped, "\\\t", "\\t");
    unescaped = ztd::replace(unescaped, "\\\r", "\\r");
    unescaped = ztd::replace(unescaped, "\\\"", "\"");

    return unescaped;
}

static bool
delayed_show_menu(GtkWidget* menu)
{
    MainWindow* main_window = main_window_get_last_active();
    if (main_window)
    {
        gtk_window_present(GTK_WINDOW(main_window));
    }
    gtk_widget_show_all(GTK_WIDGET(menu));
    gtk_menu_popup_at_pointer(GTK_MENU(menu), nullptr);
    g_signal_connect(G_OBJECT(menu), "key-press-event", G_CALLBACK(xset_menu_keypress), nullptr);
    g_signal_connect(menu, "selection-done", G_CALLBACK(gtk_widget_destroy), nullptr);
    return false;
}

// These are also the sockets return code
#define SOCKET_SUCCESS 0 // Successful exit status.
#define SOCKET_FAILURE 1 // Failing exit status.
#define SOCKET_INVALID 2 // Invalid request exit status.

const std::tuple<char, std::string>
main_window_socket_command(char* argv[])
{
    if (!(argv && argv[0]))
    {
        return {1, "invalid socket command"};
    }

    panel_t panel = 0;
    tab_t tab = 0;
    const char* window = nullptr;

    // must match file-browser.c
    static constexpr std::array<const std::string_view, 6> column_titles{
        "Name",
        "Size",
        "Type",
        "Permission",
        "Owner",
        "Modified",
    };

    // cmd options
    i64 i = 1;
    while (argv[i] && argv[i][0] == '-')
    {
        const std::string socket_property = argv[i];

        if (ztd::same(socket_property, "--window"))
        {
            if (!argv[i + 1])
            {
                return {SOCKET_FAILURE, std::format("option {} requires an argument", argv[i])};
            }
            window = argv[i + 1];
            i += 2;
            continue;
        }
        else if (ztd::same(socket_property, "--panel"))
        {
            if (!argv[i + 1])
            {
                return {SOCKET_FAILURE, std::format("option {} requires an argument", argv[i])};
            }
            panel = std::stol(argv[i + 1]);
            i += 2;
            continue;
        }
        else if (ztd::same(socket_property, "--tab"))
        {
            if (!argv[i + 1])
            {
                return {SOCKET_FAILURE, std::format("option {} requires an argument", argv[i])};
            }
            tab = std::stol(argv[i + 1]);
            i += 2;
            continue;
        }
        return {SOCKET_FAILURE, std::format("invalid option '{}'", argv[i])};
    }

    // window
    MainWindow* main_window = nullptr;
    if (!window)
    {
        main_window = main_window_get_last_active();
        if (!main_window)
        {
            return {SOCKET_INVALID, "invalid window"};
        }
    }
    else
    {
        for (MainWindow* window2 : all_windows)
        {
            const std::string str = std::format("{:p}", (void*)window2);
            if (ztd::same(str, window))
            {
                main_window = window2;
                break;
            }
        }
        if (!main_window)
        {
            return {SOCKET_INVALID, std::format("invalid window {}", window)};
        }
    }

    // panel
    if (!panel)
    {
        panel = main_window->curpanel;
    }
    if (!valid_panel(panel))
    {
        return {SOCKET_INVALID, std::format("invalid panel {}", panel)};
    }
    if (!xset_get_b_panel(panel, xset::panel::show) ||
        gtk_notebook_get_current_page(GTK_NOTEBOOK(main_window->panel[panel - 1])) == -1)
    {
        return {SOCKET_INVALID, std::format("panel {} is not visible", panel)};
    }

    // tab
    if (!tab)
    {
        tab = gtk_notebook_get_current_page(GTK_NOTEBOOK(main_window->panel[panel - 1])) + 1;
    }
    if (tab < 1 || tab > gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_window->panel[panel - 1])))
    {
        return {SOCKET_INVALID, std::format("invalid tab {}", tab)};
    }
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER_REINTERPRET(
        gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_window->panel[panel - 1]), tab - 1));

    // command
    const std::string socket_cmd = argv[i - 1];
    const std::string socket_property = argv[i];

    // ztd::logger::info("argv[i-2]={}", argv[i-2]);
    // ztd::logger::info("argv[i-1]={}", argv[i-1]);
    // ztd::logger::info("argv[i+0]={}", argv[i]);
    // ztd::logger::info("argv[i+1]={}", argv[i+1]);
    // ztd::logger::info("argv[i+2]={}", argv[i+2]);
    // ztd::logger::info("argv[i+3]={}", argv[i+3]);

    if (ztd::same(socket_cmd, "set"))
    {
        if (!argv[i])
        {
            return {SOCKET_FAILURE, "command set requires an argument"};
        }
        if (ztd::same(socket_property, "window_size") ||
            ztd::same(socket_property, "window_position"))
        {
            i32 height = 0;
            i32 width = 0;
            if (argv[i + 1])
            {
                // size format '620x480'
                if (!ztd::contains(argv[i + 1], "x"))
                {
                    return {SOCKET_INVALID, std::format("invalid size format {}", argv[i + 1])};
                }
                const auto size = ztd::split(argv[i + 1], "x");
                width = std::stol(size[0]);
                height = std::stol(size[1]);
            }
            if (height < 1 || width < 1)
            {
                return {SOCKET_INVALID, std::format("invalid {} value", argv[i])};
            }
            if (ztd::same(socket_property, "window_size"))
            {
                gtk_window_resize(GTK_WINDOW(main_window), width, height);
            }
            else
            {
                gtk_window_move(GTK_WINDOW(main_window), width, height);
            }
        }
        else if (ztd::same(socket_property, "window_maximized"))
        {
            if (get_bool(argv[i + 1]))
            {
                gtk_window_maximize(GTK_WINDOW(main_window));
            }
            else
            {
                gtk_window_unmaximize(GTK_WINDOW(main_window));
            }
        }
        else if (ztd::same(socket_property, "window_fullscreen"))
        {
            xset_set_b(xset::name::main_full, get_bool(argv[i + 1]));
            on_fullscreen_activate(nullptr, main_window);
        }
        else if (ztd::same(socket_property, "screen_size"))
        {
        }
        else if (ztd::same(socket_property, "window_vslider_top") ||
                 ztd::same(socket_property, "window_vslider_bottom") ||
                 ztd::same(socket_property, "window_hslider") ||
                 ztd::same(socket_property, "window_tslider"))
        {
            i32 width = -1;
            if (argv[i + 1])
            {
                width = std::stol(argv[i + 1]);
            }
            if (width < 0)
            {
                return {SOCKET_INVALID, "invalid slider value"};
            }

            GtkWidget* widget;
            if (ztd::same(socket_property, "window_vslider_top"))
            {
                widget = main_window->hpane_top;
            }
            else if (ztd::same(socket_property, "window_vslider_bottom"))
            {
                widget = main_window->hpane_bottom;
            }
            else if (ztd::same(socket_property, "window_hslider"))
            {
                widget = main_window->vpane;
            }
            else
            {
                widget = main_window->task_vpane;
            }

            gtk_paned_set_position(GTK_PANED(widget), width);
        }
        else if (ztd::same(socket_property, "focused_panel"))
        {
            i32 width = 0;
            if (argv[i + 1])
            {
                if (ztd::same(argv[i + 1], "prev"))
                {
                    width = panel_control_code_prev;
                }
                else if (ztd::same(argv[i + 1], "next"))
                {
                    width = panel_control_code_next;
                }
                else if (ztd::same(argv[i + 1], "hide"))
                {
                    width = panel_control_code_hide;
                }
                else
                {
                    width = std::stol(argv[i + 1]);
                }
            }
            if (!valid_panel(width) || !valid_panel_code(width))
            {
                return {SOCKET_INVALID, "invalid panel number"};
            }
            focus_panel(nullptr, (void*)main_window, width);
        }
        else if (ztd::same(socket_property, "focused_pane"))
        {
            GtkWidget* widget = nullptr;
            if (argv[i + 1])
            {
                if (ztd::same(argv[i + 1], "filelist"))
                {
                    widget = file_browser->folder_view;
                }
                else if (ztd::same(argv[i + 1], "devices"))
                {
                    widget = file_browser->side_dev;
                }
                else if (ztd::same(argv[i + 1], "dirtree"))
                {
                    widget = file_browser->side_dir;
                }
                else if (ztd::same(argv[i + 1], "pathbar"))
                {
                    widget = file_browser->path_bar;
                }
            }
            if (GTK_IS_WIDGET(widget))
            {
                gtk_widget_grab_focus(widget);
            }
        }
        else if (ztd::same(socket_property, "current_tab"))
        {
            i32 width = 0;
            if (argv[i + 1])
            {
                if (ztd::same(argv[i + 1], "prev"))
                {
                    width = tab_control_code_prev;
                }
                else if (ztd::same(argv[i + 1], "next"))
                {
                    width = tab_control_code_next;
                }
                else if (ztd::same(argv[i + 1], "close"))
                {
                    width = tab_control_code_close;
                }
                else
                {
                    width = std::stol(argv[i + 1]);
                }
            }
            if (!valid_tab_code(width) || width == 0 ||
                width > gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_window->panel[panel - 1])))
            {
                return {SOCKET_INVALID, "invalid tab number"};
            }
            ptk_file_browser_go_tab(nullptr, file_browser, width);
        }
        else if (ztd::same(socket_property, "tab_count"))
        {
        }
        else if (ztd::same(socket_property, "new_tab"))
        {
            focus_panel(nullptr, (void*)main_window, panel);
            if (!(argv[i + 1] && std::filesystem::is_directory(argv[i + 1])))
            {
                ptk_file_browser_new_tab(nullptr, file_browser);
            }
            else
            {
                main_window_add_new_tab(main_window, argv[i + 1]);
            }

            const auto counts = main_window_get_counts(file_browser);
            // const panel_t panel_count = counts[0];
            const tab_t tab_count = counts[1];
            const tab_t tab_num = counts[2];

            return {SOCKET_SUCCESS,
                    std::format("new_tab_window={}\nnew_tab_panel={}\n"
                                "new_tab_number={}\nnew_tab_path={}",
                                (void*)main_window,
                                panel,
                                tab_num,
                                argv[tab_count + 1])};
        }
        else if (ztd::endswith(socket_property, "_visible"))
        {
            bool valid = false;
            bool use_mode = false;
            xset::panel xset_panel_var;
            if (ztd::startswith(socket_property, "devices_"))
            {
                xset_panel_var = xset::panel::show_devmon;
                use_mode = true;
                valid = true;
            }
            else if (ztd::startswith(socket_property, "dirtree_"))
            {
                xset_panel_var = xset::panel::show_dirtree;
                use_mode = true;
                valid = true;
            }
            else if (ztd::startswith(socket_property, "toolbar_"))
            {
                xset_panel_var = xset::panel::show_toolbox;
                use_mode = true;
                valid = true;
            }
            else if (ztd::startswith(socket_property, "sidetoolbar_"))
            {
                xset_panel_var = xset::panel::show_sidebar;
                use_mode = true;
                valid = true;
            }
            else if (ztd::startswith(socket_property, "hidden_files_"))
            {
                xset_panel_var = xset::panel::show_hidden;
                valid = true;
            }
            else if (ztd::startswith(socket_property, "panel"))
            {
                const i32 j = argv[i][5] - 48;
                if (!valid_panel(j))
                {
                    return {SOCKET_INVALID, std::format("invalid property {}", argv[i])};
                }
                xset_set_b_panel(j, xset::panel::show, get_bool(argv[i + 1]));
                show_panels_all_windows(nullptr, main_window);
                return {SOCKET_SUCCESS, ""};
            }
            if (!valid)
            {
                return {SOCKET_FAILURE, std::format("invalid property {}", argv[i])};
            }
            if (use_mode)
            {
                xset_set_b_panel_mode(panel,
                                      xset_panel_var,
                                      main_window->panel_context.at(panel),
                                      get_bool(argv[i + 1]));
            }
            else
            {
                xset_set_b_panel(panel, xset_panel_var, get_bool(argv[i + 1]));
            }
            update_views_all_windows(nullptr, file_browser);
        }
        else if (ztd::same(socket_property, "panel_hslider_top") ||
                 ztd::same(socket_property, "panel_hslider_bottom") ||
                 ztd::same(socket_property, "panel_vslider"))
        {
            i32 width = -1;
            if (argv[i + 1])
            {
                width = std::stol(argv[i + 1]);
            }
            if (width < 0)
            {
                return {SOCKET_INVALID, "invalid slider value"};
            }
            GtkWidget* widget;
            if (ztd::same(socket_property, "panel_hslider_top"))
            {
                widget = file_browser->side_vpane_top;
            }
            else if (ztd::same(socket_property, "panel_hslider_bottom"))
            {
                widget = file_browser->side_vpane_bottom;
            }
            else
            {
                widget = file_browser->hpane;
            }
            gtk_paned_set_position(GTK_PANED(widget), width);
            ptk_file_browser_slider_release(nullptr, nullptr, file_browser);
            update_views_all_windows(nullptr, file_browser);
        }
        else if (ztd::same(socket_property, "column_width"))
        { // COLUMN WIDTH
            i32 width = 0;
            if (argv[i + 1] && argv[i + 2])
            {
                width = std::stol(argv[i + 2]);
            }
            if (width < 1)
            {
                return {SOCKET_INVALID, "invalid column width"};
            }
            if (file_browser->view_mode == PtkFBViewMode::PTK_FB_LIST_VIEW)
            {
                bool found = false;
                GtkTreeViewColumn* col;
                for (const auto [index, value] : ztd::enumerate(column_titles))
                {
                    col = gtk_tree_view_get_column(GTK_TREE_VIEW(file_browser->folder_view), index);
                    if (!col)
                    {
                        continue;
                    }
                    const char* title = gtk_tree_view_column_get_title(col);
                    if (ztd::same(argv[i + 1], title))
                    {
                        found = true;
                        break;
                    }
                    if (ztd::same(argv[i + 1], "name") && ztd::same(title, value))
                    {
                        found = true;
                        break;
                    }
                    else if (ztd::same(argv[i + 1], "size") && ztd::same(title, value))
                    {
                        found = true;
                        break;
                    }
                    else if (ztd::same(argv[i + 1], "type") && ztd::same(title, value))
                    {
                        found = true;
                        break;
                    }
                    else if (ztd::same(argv[i + 1], "permission") && ztd::same(title, value))
                    {
                        found = true;
                        break;
                    }
                    else if (ztd::same(argv[i + 1], "owner") && ztd::same(title, value))
                    {
                        found = true;
                        break;
                    }
                    else if (ztd::same(argv[i + 1], "modified") && ztd::same(title, value))
                    {
                        found = true;
                        break;
                    }
                }
                if (found)
                {
                    gtk_tree_view_column_set_fixed_width(col, width);
                }
                else
                {
                    return {SOCKET_INVALID, std::format("invalid column name '{}'", argv[i + 1])};
                }
            }
        }
        else if (ztd::same(socket_property, "sort_by"))
        { // COLUMN
            PtkFBSortOrder j = PtkFBSortOrder::PTK_FB_SORT_BY_NAME;
            if (!argv[i + 1])
            {
            }
            else if (ztd::same(argv[i + 1], "name"))
            {
                j = PtkFBSortOrder::PTK_FB_SORT_BY_NAME;
            }
            else if (ztd::same(argv[i + 1], "size"))
            {
                j = PtkFBSortOrder::PTK_FB_SORT_BY_SIZE;
            }
            else if (ztd::same(argv[i + 1], "type"))
            {
                j = PtkFBSortOrder::PTK_FB_SORT_BY_TYPE;
            }
            else if (ztd::same(argv[i + 1], "permission"))
            {
                j = PtkFBSortOrder::PTK_FB_SORT_BY_PERM;
            }
            else if (ztd::same(argv[i + 1], "owner"))
            {
                j = PtkFBSortOrder::PTK_FB_SORT_BY_OWNER;
            }
            else if (ztd::same(argv[i + 1], "modified"))
            {
                j = PtkFBSortOrder::PTK_FB_SORT_BY_MTIME;
            }
            else
            {
                return {SOCKET_INVALID, std::format("invalid column name '{}'", argv[i + 1])};
            }
            ptk_file_browser_set_sort_order(file_browser, j);
        }
        else if (ztd::startswith(socket_property, "sort_"))
        {
            xset::name xset_name;
            if (ztd::same(socket_property, "sort_ascend"))
            {
                ptk_file_browser_set_sort_type(file_browser,
                                               get_bool(argv[i + 1])
                                                   ? GtkSortType::GTK_SORT_ASCENDING
                                                   : GtkSortType::GTK_SORT_DESCENDING);
                return {SOCKET_SUCCESS, ""};
            }
            else if (ztd::same(socket_property, "sort_alphanum"))
            {
                xset_name = xset::name::sortx_alphanum;
                xset_set_b(xset_name, get_bool(argv[i + 1]));
            }
            // else if (ztd::same(socket_property, "sort_natural"))
            //{
            //     xset_name = xset::name::sortx_natural;
            //     xset_set_b(xset_name, get_bool(argv[i + 1]));
            // }
            else if (ztd::same(socket_property, "sort_case"))
            {
                xset_name = xset::name::sortx_case;
                xset_set_b(xset_name, get_bool(argv[i + 1]));
            }
            else if (ztd::same(socket_property, "sort_hidden_first"))
            {
                if (get_bool(argv[i + 1]))
                {
                    xset_name = xset::name::sortx_hidfirst;
                }
                else
                {
                    xset_name = xset::name::sortx_hidlast;
                }
                xset_set_b(xset_name, true);
            }
            else if (ztd::same(socket_property, "sort_first"))
            {
                if (ztd::same(argv[i + 1], "files"))
                {
                    xset_name = xset::name::sortx_files;
                }
                else if (ztd::same(argv[i + 1], "directories"))
                {
                    xset_name = xset::name::sortx_directories;
                }
                else if (ztd::same(argv[i + 1], "mixed"))
                {
                    xset_name = xset::name::sortx_mix;
                }
                else
                {
                    return {SOCKET_INVALID, std::format("invalid {} value", argv[i])};
                }
            }
            else
            {
                return {SOCKET_FAILURE, std::format("invalid property {}", argv[i])};
            }
            ptk_file_browser_set_sort_extra(file_browser, xset_name);
        }
        else if (ztd::same(socket_property, "show_thumbnails"))
        {
            if (app_settings.get_show_thumbnail() != get_bool(argv[i + 1]))
            {
                main_window_toggle_thumbnails_all_windows();
            }
        }
        else if (ztd::same(socket_property, "large_icons"))
        {
            if (file_browser->view_mode != PtkFBViewMode::PTK_FB_ICON_VIEW)
            {
                xset_set_b_panel_mode(panel,
                                      xset::panel::list_large,
                                      main_window->panel_context.at(panel),
                                      get_bool(argv[i + 1]));
                update_views_all_windows(nullptr, file_browser);
            }
        }
        else if (ztd::same(socket_property, "statusbar_text"))
        {
            if (!(argv[i + 1] && argv[i + 1][0]))
            {
                std::free(file_browser->status_bar_custom);
                file_browser->status_bar_custom = nullptr;
            }
            else
            {
                std::free(file_browser->status_bar_custom);
                file_browser->status_bar_custom = ztd::strdup(argv[i + 1]);
            }
            main_window_update_status_bar(main_window, file_browser);
        }
        else if (ztd::same(socket_property, "pathbar_text"))
        { // TEXT [[SELSTART] SELEND]
            if (!GTK_IS_WIDGET(file_browser->path_bar))
            {
                return {SOCKET_SUCCESS, ""};
            }
            if (!(argv[i + 1] && argv[i + 1][0]))
            {
                gtk_entry_set_text(GTK_ENTRY(file_browser->path_bar), "");
            }
            else
            {
                i32 width;
                i32 height;
                gtk_entry_set_text(GTK_ENTRY(file_browser->path_bar), argv[i + 1]);
                if (!argv[i + 2])
                {
                    width = 0;
                    height = -1;
                }
                else
                {
                    width = std::stol(argv[i + 2]);
                    height = argv[i + 3] ? std::stol(argv[i + 3]) : -1;
                }
                gtk_editable_set_position(GTK_EDITABLE(file_browser->path_bar), -1);
                gtk_editable_select_region(GTK_EDITABLE(file_browser->path_bar), width, height);
                gtk_widget_grab_focus(file_browser->path_bar);
            }
        }
        else if (ztd::same(socket_property, "clipboard_text") ||
                 ztd::same(socket_property, "clipboard_primary_text"))
        {
            if (argv[i + 1] && !g_utf8_validate(argv[i + 1], -1, nullptr))
            {
                return {SOCKET_INVALID, "text is not valid UTF-8"};
            }
            GtkClipboard* clip = gtk_clipboard_get(ztd::same(socket_property, "clipboard_text")
                                                       ? GDK_SELECTION_CLIPBOARD
                                                       : GDK_SELECTION_PRIMARY);
            const std::string str = unescape(argv[i + 1] ? argv[i + 1] : "");
            gtk_clipboard_set_text(clip, str.data(), -1);
        }
        else if (ztd::same(socket_property, "clipboard_from_file") ||
                 ztd::same(socket_property, "clipboard_primary_from_file"))
        {
            if (!argv[i + 1])
            {
                return {SOCKET_FAILURE, std::format("{} requires a file path", argv[i])};
            }
            std::string contents;
            try
            {
                contents = Glib::file_get_contents(argv[i + 1]);
            }
            catch (const Glib::FileError& e)
            {
                return {SOCKET_INVALID, std::format("error reading file '{}'", argv[i + 1])};
            }
            if (!g_utf8_validate(contents.data(), -1, nullptr))
            {
                return {SOCKET_INVALID,
                        std::format("file '{}' does not contain valid UTF-8 text", argv[i + 1])};
            }
            GtkClipboard* clip = gtk_clipboard_get(ztd::same(socket_property, "clipboard_from_file")
                                                       ? GDK_SELECTION_CLIPBOARD
                                                       : GDK_SELECTION_PRIMARY);
            gtk_clipboard_set_text(clip, contents.data(), -1);
        }
        else if (ztd::same(socket_property, "clipboard_cut_files") ||
                 ztd::same(socket_property, "clipboard_copy_files"))
        {
            ptk_clipboard_copy_file_list(argv + i + 1,
                                         ztd::same(socket_property, "clipboard_copy_files"));
        }
        else if (ztd::same(socket_property, "selected_filenames") ||
                 ztd::same(socket_property, "selected_files"))
        {
            if (!argv[i + 1] || argv[i + 1][0] == '\0')
            {
                // unselect all
                ptk_file_browser_select_file_list(file_browser, nullptr, false);
            }
            else
            {
                ptk_file_browser_select_file_list(file_browser, argv + i + 1, true);
            }
        }
        else if (ztd::same(socket_property, "selected_pattern"))
        {
            if (!argv[i + 1])
            {
                // unselect all
                ptk_file_browser_select_file_list(file_browser, nullptr, false);
            }
            else
            {
                ptk_file_browser_select_pattern(nullptr, file_browser, argv[i + 1]);
            }
        }
        else if (ztd::same(socket_property, "current_dir"))
        {
            if (!argv[i + 1])
            {
                return {SOCKET_FAILURE, std::format("{} requires a directory path", argv[i])};
            }
            if (!std::filesystem::is_directory(argv[i + 1]))
            {
                return {SOCKET_FAILURE, std::format("directory '{}' does not exist", argv[i + 1])};
            }
            ptk_file_browser_chdir(file_browser,
                                   argv[i + 1],
                                   PtkFBChdirMode::PTK_FB_CHDIR_ADD_HISTORY);
        }
        else
        {
            return {SOCKET_FAILURE, std::format("invalid property {}", argv[i])};
        }
    }
    else if (ztd::same(socket_cmd, "get"))
    {
        // get
        if (!argv[i])
        {
            return {SOCKET_FAILURE, std::format("command {} requires an argument", socket_cmd)};
        }

        if (ztd::same(socket_property, "window_size") ||
            ztd::same(socket_property, "window_position"))
        {
            i32 width;
            i32 height;
            if (ztd::same(socket_property, "window_size"))
            {
                gtk_window_get_size(GTK_WINDOW(main_window), &width, &height);
            }
            else
            {
                gtk_window_get_position(GTK_WINDOW(main_window), &width, &height);
            }
            return {SOCKET_SUCCESS, std::format("{}x{}", width, height)};
        }
        else if (ztd::same(socket_property, "window_maximized"))
        {
            return {SOCKET_SUCCESS, std::format("{}", !!main_window->maximized)};
        }
        else if (ztd::same(socket_property, "window_fullscreen"))
        {
            return {SOCKET_SUCCESS, std::format("{}", !!main_window->fullscreen)};
        }
        else if (ztd::same(socket_property, "screen_size"))
        {
            GdkRectangle workarea = GdkRectangle();
            gdk_monitor_get_workarea(gdk_display_get_primary_monitor(gdk_display_get_default()),
                                     &workarea);
            return {SOCKET_SUCCESS, std::format("{}x{}", workarea.width, workarea.height)};
        }
        else if (ztd::same(socket_property, "window_vslider_top") ||
                 ztd::same(socket_property, "window_vslider_bottom") ||
                 ztd::same(socket_property, "window_hslider") ||
                 ztd::same(socket_property, "window_tslider"))
        {
            GtkWidget* widget;

            if (ztd::same(socket_property, "window_vslider_top"))
            {
                widget = main_window->hpane_top;
            }
            else if (ztd::same(socket_property, "window_vslider_bottom"))
            {
                widget = main_window->hpane_bottom;
            }
            else if (ztd::same(socket_property, "window_hslider"))
            {
                widget = main_window->vpane;
            }
            else
            {
                widget = main_window->task_vpane;
            }
            return {SOCKET_SUCCESS, std::format("{}", gtk_paned_get_position(GTK_PANED(widget)))};
        }
        else if (ztd::same(socket_property, "focused_panel"))
        {
            return {SOCKET_SUCCESS, std::format("{}", main_window->curpanel)};
        }
        else if (ztd::same(socket_property, "focused_pane"))
        {
            if (file_browser->folder_view && gtk_widget_is_focus(file_browser->folder_view))
            {
                return {SOCKET_SUCCESS, "filelist"};
            }
            else if (file_browser->side_dev && gtk_widget_is_focus(file_browser->side_dev))
            {
                return {SOCKET_SUCCESS, "devices"};
            }
            else if (file_browser->side_dir && gtk_widget_is_focus(file_browser->side_dir))
            {
                return {SOCKET_SUCCESS, "dirtree"};
            }
            else if (file_browser->path_bar && gtk_widget_is_focus(file_browser->path_bar))
            {
                return {SOCKET_SUCCESS, "pathbar"};
            }
        }
        else if (ztd::same(socket_property, "current_tab"))
        {
            return {SOCKET_SUCCESS,
                    std::format("{}",
                                gtk_notebook_page_num(GTK_NOTEBOOK(main_window->panel[panel - 1]),
                                                      GTK_WIDGET(file_browser)) +
                                    1)};
        }
        else if (ztd::same(socket_property, "panel_count"))
        {
            const auto counts = main_window_get_counts(file_browser);
            const panel_t panel_count = counts[0];
            // const tab_t tab_count = counts[1];
            // const tab_t tab_num = counts[2];

            return {SOCKET_SUCCESS, std::format("{}", panel_count)};
        }
        else if (ztd::same(socket_property, "tab_count"))
        {
            const auto counts = main_window_get_counts(file_browser);
            // const panel_t panel_count = counts[0];
            const tab_t tab_count = counts[1];
            // const tab_t tab_num = counts[2];

            return {SOCKET_SUCCESS, std::format("{}", tab_count)};
        }
        else if (ztd::same(socket_property, "new_tab"))
        {
        }
        else if (ztd::endswith(socket_property, "_visible"))
        {
            bool valid = false;
            bool use_mode = false;
            xset::panel xset_panel_var;
            if (ztd::startswith(socket_property, "devices_"))
            {
                xset_panel_var = xset::panel::show_devmon;
                use_mode = true;
                valid = true;
            }
            else if (ztd::startswith(socket_property, "dirtree_"))
            {
                xset_panel_var = xset::panel::show_dirtree;
                use_mode = true;
                valid = true;
            }
            else if (ztd::startswith(socket_property, "toolbar_"))
            {
                xset_panel_var = xset::panel::show_toolbox;
                use_mode = true;
                valid = true;
            }
            else if (ztd::startswith(socket_property, "sidetoolbar_"))
            {
                xset_panel_var = xset::panel::show_sidebar;
                use_mode = true;
                valid = true;
            }
            else if (ztd::startswith(socket_property, "hidden_files_"))
            {
                xset_panel_var = xset::panel::show_hidden;
                valid = true;
            }
            else if (ztd::startswith(socket_property, "panel"))
            {
                const i32 j = argv[i][5] - 48;
                if (!valid_panel(j))
                {
                    return {SOCKET_INVALID, std::format("invalid property {}", argv[i])};
                }
                return {SOCKET_SUCCESS, std::format("{}", xset_get_b_panel(j, xset::panel::show))};
            }
            if (!valid)
            {
                return {SOCKET_FAILURE, std::format("invalid property {}", argv[i])};
            }
            if (use_mode)
            {
                return {SOCKET_SUCCESS,
                        std::format("{}",
                                    xset_get_b_panel_mode(panel,
                                                          xset_panel_var,
                                                          main_window->panel_context.at(panel)))};
            }
            else
            {
                return {SOCKET_SUCCESS, std::format("{}", xset_get_b_panel(panel, xset_panel_var))};
            }
        }
        else if (ztd::same(socket_property, "panel_hslider_top") ||
                 ztd::same(socket_property, "panel_hslider_bottom") ||
                 ztd::same(socket_property, "panel_vslider"))
        {
            GtkWidget* widget;
            if (ztd::same(socket_property, "panel_hslider_top"))
            {
                widget = file_browser->side_vpane_top;
            }
            else if (ztd::same(socket_property, "panel_hslider_bottom"))
            {
                widget = file_browser->side_vpane_bottom;
            }
            else
            {
                widget = file_browser->hpane;
            }
            return {SOCKET_SUCCESS, std::format("{}", gtk_paned_get_position(GTK_PANED(widget)))};
        }
        else if (ztd::same(socket_property, "column_width"))
        { // COLUMN
            if (file_browser->view_mode == PtkFBViewMode::PTK_FB_LIST_VIEW)
            {
                bool found = false;
                GtkTreeViewColumn* col;
                for (const auto [index, value] : ztd::enumerate(column_titles))
                {
                    col = gtk_tree_view_get_column(GTK_TREE_VIEW(file_browser->folder_view), index);
                    if (!col)
                    {
                        continue;
                    }
                    const char* title = gtk_tree_view_column_get_title(col);
                    if (ztd::same(argv[i + 1], title))
                    {
                        found = true;
                        break;
                    }
                    if (ztd::same(argv[i + 1], "name") && ztd::same(title, value))
                    {
                        found = true;
                        break;
                    }
                    else if (ztd::same(argv[i + 1], "size") && ztd::same(title, value))
                    {
                        found = true;
                        break;
                    }
                    else if (ztd::same(argv[i + 1], "type") && ztd::same(title, value))
                    {
                        found = true;
                        break;
                    }
                    else if (ztd::same(argv[i + 1], "permission") && ztd::same(title, value))
                    {
                        found = true;
                        break;
                    }
                    else if (ztd::same(argv[i + 1], "owner") && ztd::same(title, value))
                    {
                        found = true;
                        break;
                    }
                    else if (ztd::same(argv[i + 1], "modified") && ztd::same(title, value))
                    {
                        found = true;
                        break;
                    }
                }
                if (found)
                {
                    return {SOCKET_SUCCESS, std::format("{}", gtk_tree_view_column_get_width(col))};
                }
                else
                {
                    return {SOCKET_INVALID, std::format("invalid column name '{}'", argv[i + 1])};
                }
            }
        }
        else if (ztd::same(socket_property, "sort_by"))
        { // COLUMN
            if (file_browser->sort_order == PtkFBSortOrder::PTK_FB_SORT_BY_NAME)
            {
                return {SOCKET_SUCCESS, "name"};
            }
            else if (file_browser->sort_order == PtkFBSortOrder::PTK_FB_SORT_BY_SIZE)
            {
                return {SOCKET_SUCCESS, "size"};
            }
            else if (file_browser->sort_order == PtkFBSortOrder::PTK_FB_SORT_BY_TYPE)
            {
                return {SOCKET_SUCCESS, "type"};
            }
            else if (file_browser->sort_order == PtkFBSortOrder::PTK_FB_SORT_BY_PERM)
            {
                return {SOCKET_SUCCESS, "permission"};
            }
            else if (file_browser->sort_order == PtkFBSortOrder::PTK_FB_SORT_BY_OWNER)
            {
                return {SOCKET_SUCCESS, "owner"};
            }
            else if (file_browser->sort_order == PtkFBSortOrder::PTK_FB_SORT_BY_MTIME)
            {
                return {SOCKET_SUCCESS, "modified"};
            }
        }
        else if (ztd::startswith(socket_property, "sort_"))
        {
            if (ztd::same(socket_property, "sort_ascend"))
            {
                return {SOCKET_SUCCESS,
                        std::format(
                            "{}",
                            file_browser->sort_type == GtkSortType::GTK_SORT_ASCENDING ? 1 : 0)};
            }
#if 0
            else if (ztd::same(socket_property, "sort_natural"))
            {

            }
#endif
            else if (ztd::same(socket_property, "sort_alphanum"))
            {
                return {SOCKET_SUCCESS,
                        std::format("{}",
                                    xset_get_b_panel(file_browser->mypanel, xset::panel::sort_extra)
                                        ? 1
                                        : 0)};
            }
            else if (ztd::same(socket_property, "sort_case"))
            {
                return {
                    SOCKET_SUCCESS,
                    std::format("{}",
                                xset_get_b_panel(file_browser->mypanel, xset::panel::sort_extra) &&
                                        xset_get_int_panel(file_browser->mypanel,
                                                           xset::panel::sort_extra,
                                                           xset::var::x) == xset::b::xtrue
                                    ? 1
                                    : 0)};
            }
            else if (ztd::same(socket_property, "sort_hidden_first"))
            {
                return {SOCKET_SUCCESS,
                        std::format("{}",
                                    xset_get_int_panel(file_browser->mypanel,
                                                       xset::panel::sort_extra,
                                                       xset::var::z) == xset::b::xtrue
                                        ? 1
                                        : 0)};
            }
            else if (ztd::same(socket_property, "sort_first"))
            {
                const i32 result = xset_get_int_panel(file_browser->mypanel,
                                                      xset::panel::sort_extra,
                                                      xset::var::y);
                if (result == 0)
                {
                    return {SOCKET_SUCCESS, "mixed"};
                }
                else if (result == 1)
                {
                    return {SOCKET_SUCCESS, "directories"};
                }
                else if (result == 2)
                {
                    return {SOCKET_SUCCESS, "files"};
                }
            }
            else
            {
                return {SOCKET_FAILURE, std::format("invalid property {}", argv[i])};
            }
        }
        else if (ztd::same(socket_property, "show_thumbnails"))
        {
            return {SOCKET_SUCCESS, std::format("{}", app_settings.get_show_thumbnail() ? 1 : 0)};
        }
        else if (ztd::same(socket_property, "large_icons"))
        {
            return {SOCKET_SUCCESS, std::format("{}", file_browser->large_icons ? 1 : 0)};
        }
        else if (ztd::same(socket_property, "statusbar_text"))
        {
            return {SOCKET_SUCCESS,
                    std::format("{}", gtk_label_get_text(GTK_LABEL(file_browser->status_label)))};
        }
        else if (ztd::same(socket_property, "pathbar_text"))
        {
            if (GTK_IS_WIDGET(file_browser->path_bar))
            {
                return {SOCKET_SUCCESS,
                        std::format("{}", gtk_entry_get_text(GTK_ENTRY(file_browser->path_bar)))};
            }
        }
        else if (ztd::same(socket_property, "clipboard_text") ||
                 ztd::same(socket_property, "clipboard_primary_text"))
        {
            GtkClipboard* clip = gtk_clipboard_get(ztd::same(socket_property, "clipboard_text")
                                                       ? GDK_SELECTION_CLIPBOARD
                                                       : GDK_SELECTION_PRIMARY);
            return {SOCKET_SUCCESS, gtk_clipboard_wait_for_text(clip)};
        }
        else if (ztd::same(socket_property, "clipboard_from_file") ||
                 ztd::same(socket_property, "clipboard_primary_from_file"))
        {
        }
        else if (ztd::same(socket_property, "clipboard_cut_files") ||
                 ztd::same(socket_property, "clipboard_copy_files"))
        {
            GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
            GdkAtom gnome_target;
            GdkAtom uri_list_target;
            GtkSelectionData* sel_data;

            gnome_target = gdk_atom_intern("x-special/gnome-copied-files", false);
            sel_data = gtk_clipboard_wait_for_contents(clip, gnome_target);
            if (!sel_data)
            {
                uri_list_target = gdk_atom_intern("text/uri-list", false);
                sel_data = gtk_clipboard_wait_for_contents(clip, uri_list_target);
                if (!sel_data)
                {
                    return {SOCKET_SUCCESS, ""};
                }
            }
            if (gtk_selection_data_get_length(sel_data) <= 0 ||
                gtk_selection_data_get_format(sel_data) != 8)
            {
                gtk_selection_data_free(sel_data);
                return {SOCKET_SUCCESS, ""};
            }
            if (ztd::startswith((const char*)gtk_selection_data_get_data(sel_data), "cut"))
            {
                if (ztd::same(socket_property, "clipboard_copy_files"))
                {
                    gtk_selection_data_free(sel_data);
                    return {SOCKET_SUCCESS, ""};
                }
            }
            else if (ztd::same(socket_property, "clipboard_cut_files"))
            {
                gtk_selection_data_free(sel_data);
                return {SOCKET_SUCCESS, ""};
            }
            const char* clip_txt = gtk_clipboard_wait_for_text(clip);
            gtk_selection_data_free(sel_data);
            if (!clip_txt)
            {
                return {SOCKET_SUCCESS, ""};
            }
            // build fish array
            const std::vector<std::string> pathv = ztd::split(clip_txt, "");
            std::string str;
            for (const std::string_view path : pathv)
            {
                str.append(std::format("{} ", ztd::shell::quote(path)));
            }
            return {SOCKET_SUCCESS, std::format("({})", str)};
        }
        else if (ztd::same(socket_property, "selected_filenames") ||
                 ztd::same(socket_property, "selected_files"))
        {
            const std::vector<vfs::file_info> sel_files =
                ptk_file_browser_get_selected_files(file_browser);
            if (sel_files.empty())
            {
                return {SOCKET_SUCCESS, ""};
            }

            // build fish array
            std::string str;
            for (vfs::file_info file : sel_files)
            {
                file = vfs_file_info_ref(file);
                if (!file)
                {
                    continue;
                }
                str.append(std::format("{} ", ztd::shell::quote(file->get_name())));
                vfs_file_info_unref(file);
            }
            vfs_file_info_list_free(sel_files);
            return {SOCKET_SUCCESS, std::format("({})", str)};
        }
        else if (ztd::same(socket_property, "selected_pattern"))
        {
        }
        else if (ztd::same(socket_property, "current_dir"))
        {
            return {SOCKET_SUCCESS,
                    std::format("{}", ptk_file_browser_get_cwd(file_browser).string())};
        }
        else
        {
            return {SOCKET_FAILURE, std::format("invalid property {}", argv[i])};
        }
    }
    else if (ztd::same(socket_cmd, "set-task"))
    { // TASKNUM PROPERTY [VALUE]
        if (!(argv[i] && argv[i + 1]))
        {
            return {SOCKET_FAILURE, std::format("{} requires two arguments", socket_cmd)};
        }

        // find task
        GtkTreeIter it;
        PtkFileTask* ptask = nullptr;
        GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(main_window->task_view));
        if (gtk_tree_model_get_iter_first(model, &it))
        {
            do
            {
                gtk_tree_model_get(model, &it, MainWindowTaskCol::TASK_COL_DATA, &ptask, -1);
                const std::string str = std::format("{:p}", (void*)ptask);
                if (ztd::same(str, argv[i]))
                {
                    break;
                }
                ptask = nullptr;
            } while (gtk_tree_model_iter_next(model, &it));
        }
        if (!ptask)
        {
            return {SOCKET_INVALID, std::format("invalid task '{}'", argv[i])};
        }
        if (ptask->task->type != VFSFileTaskType::EXEC)
        {
            return {SOCKET_INVALID, std::format("internal task {} is read-only", argv[i])};
        }

        // set model value
        i32 j;
        if (ztd::same(argv[i + 1], "icon"))
        {
            ptk_file_task_lock(ptask);
            ptask->task->exec_icon = argv[i + 2];
            ptask->pause_change_view = ptask->pause_change = true;
            ptk_file_task_unlock(ptask);
            return {SOCKET_SUCCESS, ""};
        }
        else if (ztd::same(argv[i + 1], "count"))
        {
            j = MainWindowTaskCol::TASK_COL_COUNT;
        }
        else if (ztd::same(argv[i + 1], "directory") || ztd::same(argv[i + 1], "from"))
        {
            j = MainWindowTaskCol::TASK_COL_PATH;
        }
        else if (ztd::same(argv[i + 1], "item"))
        {
            j = MainWindowTaskCol::TASK_COL_FILE;
        }
        else if (ztd::same(argv[i + 1], "to"))
        {
            j = MainWindowTaskCol::TASK_COL_TO;
        }
        else if (ztd::same(argv[i + 1], "progress"))
        {
            if (!argv[i + 2])
            {
                ptask->task->percent = 50;
            }
            else
            {
                j = std::stoi(argv[i + 2]);
                if (j < 0)
                {
                    j = 0;
                }
                if (j > 100)
                {
                    j = 100;
                }
                ptask->task->percent = j;
            }
            ptask->task->custom_percent = !!argv[i + 2];
            ptask->pause_change_view = ptask->pause_change = true;
            return {SOCKET_SUCCESS, ""};
        }
        else if (ztd::same(argv[i + 1], "total"))
        {
            j = MainWindowTaskCol::TASK_COL_TOTAL;
        }
        else if (ztd::same(argv[i + 1], "curspeed"))
        {
            j = MainWindowTaskCol::TASK_COL_CURSPEED;
        }
        else if (ztd::same(argv[i + 1], "curremain"))
        {
            j = MainWindowTaskCol::TASK_COL_CUREST;
        }
        else if (ztd::same(argv[i + 1], "avgspeed"))
        {
            j = MainWindowTaskCol::TASK_COL_AVGSPEED;
        }
        else if (ztd::same(argv[i + 1], "avgremain"))
        {
            j = MainWindowTaskCol::TASK_COL_AVGEST;
        }
        else if (ztd::same(argv[i + 1], "elapsed") || ztd::same(argv[i + 1], "started") ||
                 ztd::same(argv[i + 1], "status"))
        {
            return {SOCKET_INVALID, std::format("task property '{}' is read-only", argv[i + 1])};
        }
        else if (ztd::same(argv[i + 1], "queue_state"))
        {
            if (!argv[i + 2] || ztd::same(argv[i + 2], "run"))
            {
                ptk_file_task_pause(ptask, VFSFileTaskState::RUNNING);
            }
            else if (ztd::same(argv[i + 2], "pause"))
            {
                ptk_file_task_pause(ptask, VFSFileTaskState::PAUSE);
            }
            else if (ztd::same(argv[i + 2], "queue") || ztd::same(argv[i + 2], "queued"))
            {
                ptk_file_task_pause(ptask, VFSFileTaskState::QUEUE);
            }
            else if (ztd::same(argv[i + 2], "stop"))
            {
                on_task_stop(nullptr,
                             main_window->task_view,
                             xset_get(xset::name::task_stop_all),
                             nullptr);
            }
            else
            {
                return {SOCKET_INVALID, std::format("invalid queue_state '{}'", argv[i + 2])};
            }
            main_task_start_queued(main_window->task_view, nullptr);
            return {SOCKET_SUCCESS, ""};
        }
        else if (ztd::same(argv[i + 1], "popup_handler"))
        {
            std::free(ptask->pop_handler);
            if (argv[i + 2] && argv[i + 2][0] != '\0')
            {
                ptask->pop_handler = ztd::strdup(argv[i + 2]);
            }
            else
            {
                ptask->pop_handler = nullptr;
            }
            return {SOCKET_SUCCESS, ""};
        }
        else
        {
            return {SOCKET_INVALID, std::format("invalid task property '{}'", argv[i + 1])};
        }
        gtk_list_store_set(GTK_LIST_STORE(model), &it, j, argv[i + 2], -1);
    }
    else if (ztd::same(socket_cmd, "get-task"))
    { // TASKNUM PROPERTY
        if (!(argv[i] && argv[i + 1]))
        {
            return {SOCKET_FAILURE, std::format("{} requires two arguments", socket_cmd)};
        }

        // find task
        GtkTreeIter it;
        PtkFileTask* ptask = nullptr;
        GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(main_window->task_view));
        if (gtk_tree_model_get_iter_first(model, &it))
        {
            do
            {
                gtk_tree_model_get(model, &it, MainWindowTaskCol::TASK_COL_DATA, &ptask, -1);
                const std::string str = std::format("{:p}", (void*)ptask);
                if (ztd::same(str, argv[i]))
                {
                    break;
                }
                ptask = nullptr;
            } while (gtk_tree_model_iter_next(model, &it));
        }
        if (!ptask)
        {
            return {SOCKET_INVALID, std::format("invalid task '{}'", argv[i])};
        }

        // get model value
        i32 j;
        if (ztd::same(argv[i + 1], "icon"))
        {
            ptk_file_task_lock(ptask);
            if (!ptask->task->exec_icon.empty())
            {
                return {SOCKET_SUCCESS, std::format("{}", ptask->task->exec_icon)};
            }
            ptk_file_task_unlock(ptask);
            return {SOCKET_SUCCESS, ""};
        }
        else if (ztd::same(argv[i + 1], "count"))
        {
            j = MainWindowTaskCol::TASK_COL_COUNT;
        }
        else if (ztd::same(argv[i + 1], "directory") || ztd::same(argv[i + 1], "from"))
        {
            j = MainWindowTaskCol::TASK_COL_PATH;
        }
        else if (ztd::same(argv[i + 1], "item"))
        {
            j = MainWindowTaskCol::TASK_COL_FILE;
        }
        else if (ztd::same(argv[i + 1], "to"))
        {
            j = MainWindowTaskCol::TASK_COL_TO;
        }
        else if (ztd::same(argv[i + 1], "progress"))
        {
            return {SOCKET_SUCCESS, std::format("{}", ptask->task->percent)};
        }
        else if (ztd::same(argv[i + 1], "total"))
        {
            j = MainWindowTaskCol::TASK_COL_TOTAL;
        }
        else if (ztd::same(argv[i + 1], "curspeed"))
        {
            j = MainWindowTaskCol::TASK_COL_CURSPEED;
        }
        else if (ztd::same(argv[i + 1], "curremain"))
        {
            j = MainWindowTaskCol::TASK_COL_CUREST;
        }
        else if (ztd::same(argv[i + 1], "avgspeed"))
        {
            j = MainWindowTaskCol::TASK_COL_AVGSPEED;
        }
        else if (ztd::same(argv[i + 1], "avgremain"))
        {
            j = MainWindowTaskCol::TASK_COL_AVGEST;
        }
        else if (ztd::same(argv[i + 1], "elapsed"))
        {
            j = MainWindowTaskCol::TASK_COL_ELAPSED;
        }
        else if (ztd::same(argv[i + 1], "started"))
        {
            j = MainWindowTaskCol::TASK_COL_STARTED;
        }
        else if (ztd::same(argv[i + 1], "status"))
        {
            j = MainWindowTaskCol::TASK_COL_STATUS;
        }
        else if (ztd::same(argv[i + 1], "queue_state"))
        {
            if (ptask->task->state_pause == VFSFileTaskState::RUNNING)
            {
                return {SOCKET_SUCCESS, "run"};
            }
            else if (ptask->task->state_pause == VFSFileTaskState::PAUSE)
            {
                return {SOCKET_SUCCESS, "pause"};
            }
            else if (ptask->task->state_pause == VFSFileTaskState::QUEUE)
            {
                return {SOCKET_SUCCESS, "queue"};
            }
            else
            { // failsafe
                return {SOCKET_SUCCESS, "stop"};
            }
        }
        else if (ztd::same(argv[i + 1], "popup_handler"))
        {
            if (ptask->pop_handler)
            {
                return {SOCKET_SUCCESS, std::format("{}", ptask->pop_handler)};
            }
            return {SOCKET_SUCCESS, ""};
        }
        else
        {
            return {SOCKET_INVALID, std::format("invalid task property '{}'", argv[i + 1])};
        }
        char* str2;
        gtk_tree_model_get(model, &it, j, &str2, -1);
        if (str2)
        {
            return {SOCKET_SUCCESS, std::format("{}", str2)};
        }
        std::free(str2);
    }
    else if (ztd::same(socket_cmd, "run-task"))
    { // TYPE [OPTIONS] ...
        if (!(argv[i] && argv[i + 1]))
        {
            return {SOCKET_FAILURE, std::format("{} requires two arguments", socket_cmd)};
        }

        if (ztd::same(socket_property, "cmd") || ztd::same(socket_property, "command"))
        {
            // custom command task
            // cmd [--task [--popup] [--scroll]] [--terminal]
            //                     [--user USER] [--title TITLE]
            //                     [--icon ICON] [--dir DIR] COMMAND
            // get opts
            bool opt_task = false;
            bool opt_popup = false;
            bool opt_scroll = false;
            bool opt_terminal = false;
            const char* opt_user = nullptr;
            const char* opt_title = nullptr;
            const char* opt_icon = nullptr;
            const char* opt_cwd = nullptr;

            i32 j;
            for (j = i + 1; argv[j] && argv[j][0] == '-'; ++j)
            {
                if (ztd::same(argv[j], "--task"))
                {
                    opt_task = true;
                }
                else if (ztd::same(argv[j], "--popup"))
                {
                    opt_popup = opt_task = true;
                }
                else if (ztd::same(argv[j], "--scroll"))
                {
                    opt_scroll = opt_task = true;
                }
                else if (ztd::same(argv[j], "--terminal"))
                {
                    opt_terminal = true;
                    // disabled due to potential misuse of password caching su programs
                    // else if (ztd::same(argv[j], "--user"))
                    //     opt_user = argv[++j];
                }
                else if (ztd::same(argv[j], "--title"))
                {
                    opt_title = argv[++j];
                }
                else if (ztd::same(argv[j], "--icon"))
                {
                    opt_icon = argv[++j];
                }
                else if (ztd::same(argv[j], "--dir"))
                {
                    opt_cwd = argv[++j];
                    if (!opt_cwd || !std::filesystem::is_directory(opt_cwd))
                    {
                        return {SOCKET_INVALID, std::format("no such directory '{}'", opt_cwd)};
                    }
                }
                else
                {
                    return {SOCKET_INVALID,
                            std::format("invalid {} task option '{}'", socket_property, argv[j])};
                }
            }
            if (!argv[j])
            {
                return {SOCKET_FAILURE, std::format("{} requires two arguments", socket_cmd)};
            }
            std::string cmd;
            while (argv[++j])
            {
                cmd.append(std::format(" {}", argv[j]));
            }

            PtkFileTask* ptask =
                ptk_file_exec_new(opt_title ? opt_title : cmd,
                                  opt_cwd ? opt_cwd : ptk_file_browser_get_cwd(file_browser),
                                  GTK_WIDGET(file_browser),
                                  file_browser->task_view);
            ptask->task->exec_browser = file_browser;
            ptask->task->exec_command = cmd;
            ptask->task->exec_as_user = opt_user;
            ptask->task->exec_icon = opt_icon;
            ptask->task->exec_terminal = opt_terminal;
            ptask->task->exec_keep_terminal = false;
            ptask->task->exec_sync = opt_task;
            ptask->task->exec_popup = opt_popup;
            ptask->task->exec_show_output = opt_popup;
            ptask->task->exec_show_error = true;
            ptask->task->exec_scroll_lock = !opt_scroll;
            ptask->task->exec_export = true;
            if (opt_popup)
            {
                gtk_window_present(GTK_WINDOW(main_window));
            }
            ptk_file_task_run(ptask);
            if (opt_task)
            {
                return {SOCKET_SUCCESS,
                        std::format("Note: $new_task_id not valid until approx one "
                                    "half second after task start\nnew_task_window={}\n"
                                    "new_task_id={}",
                                    (void*)main_window,
                                    (void*)ptask)};
            }
        }
        else if (ztd::same(socket_property, "edit"))
        {
            // edit FILE
            if (!argv[i + 1])
            {
                return {SOCKET_FAILURE, std::format("{} requires two arguments", socket_cmd)};
            }
            if (!std::filesystem::is_regular_file(argv[i + 1]))
            {
                return {SOCKET_INVALID, std::format("no such file '{}'", argv[i + 1])};
            }
            xset_edit(GTK_WIDGET(file_browser), argv[i + 1], false, true);
        }
        else if (ztd::same(socket_property, "mount") || ztd::same(socket_property, "unmount"))
        {
            // mount or unmount TARGET
            i32 j;
            for (j = i + 1; argv[j] && argv[j][0] == '-'; ++j)
            {
                return {SOCKET_INVALID,
                        std::format("invalid {} task option '{}'", socket_property, argv[j])};
            }
            if (!argv[j])
            {
                return {SOCKET_FAILURE,
                        std::format("task type {} requires TARGET argument", socket_cmd)};
            }

            // Resolve TARGET
            char* real_path = argv[j];

            if (!std::filesystem::exists(real_path))
            {
                return {SOCKET_INVALID, std::format("path does not exist '{}'", real_path)};
            }

            const auto real_path_stat = ztd::stat(real_path);
            vfs::volume vol = nullptr;
            if (ztd::same(socket_property, "unmount") && std::filesystem::is_directory(real_path))
            {
                // unmount DIR
                if (is_path_mountpoint(real_path))
                {
                    if (!real_path_stat.is_valid() || !real_path_stat.is_block_file())
                    {
                        // NON-block device - try to find vol by mount point
                        vol = vfs_volume_get_by_device(real_path);
                        if (!vol)
                        {
                            return {SOCKET_INVALID, std::format("invalid TARGET '{}'", argv[j])};
                        }
                    }
                }
            }
            else if (real_path_stat.is_valid() && real_path_stat.is_block_file())
            {
                // block device eg /dev/sda1
                vol = vfs_volume_get_by_device(real_path);
            }
            else
            {
                return {SOCKET_INVALID, std::format("invalid TARGET '{}'", argv[j])};
            }

            // Create command
            std::string cmd;
            if (vol)
            {
                // mount/unmount vol
                if (ztd::same(socket_property, "mount"))
                {
                    const auto check_mount_command = vol->device_mount_cmd();
                    if (check_mount_command)
                    {
                        cmd = check_mount_command.value();
                    }
                }
                else
                {
                    const auto check_unmount_command = vol->device_unmount_cmd();
                    if (check_unmount_command)
                    {
                        cmd = check_unmount_command.value();
                    }
                }
            }

            if (cmd.empty())
            {
                return {SOCKET_INVALID, std::format("invalid TARGET '{}'", argv[j])};
            }
            // Task
            PtkFileTask* ptask = ptk_file_exec_new(socket_property,
                                                   ptk_file_browser_get_cwd(file_browser),
                                                   GTK_WIDGET(file_browser),
                                                   file_browser->task_view);
            ptask->task->exec_browser = file_browser;
            ptask->task->exec_command = cmd;
            ptask->task->exec_terminal = false;
            ptask->task->exec_keep_terminal = false;
            ptask->task->exec_sync = true;
            ptask->task->exec_export = false;
            ptask->task->exec_show_error = true;
            ptask->task->exec_scroll_lock = false;
            ptk_file_task_run(ptask);
        }
        else if (ztd::same(socket_property, "copy") || ztd::same(socket_property, "move") ||
                 ztd::same(socket_property, "link") || ztd::same(socket_property, "delete") ||
                 ztd::same(socket_property, "trash"))
        {
            // built-in task
            // copy SOURCE FILENAME [...] TARGET
            // move SOURCE FILENAME [...] TARGET
            // link SOURCE FILENAME [...] TARGET
            // delete SOURCE FILENAME [...]
            // get opts
            const char* opt_cwd = nullptr;
            i32 j;
            for (j = i + 1; argv[j] && argv[j][0] == '-'; ++j)
            {
                if (ztd::same(argv[j], "--dir"))
                {
                    opt_cwd = argv[++j];
                    if (!opt_cwd || !std::filesystem::is_directory(opt_cwd))
                    {
                        return {SOCKET_INVALID, std::format("no such directory '{}'", opt_cwd)};
                    }
                }
                else
                {
                    return {SOCKET_INVALID,
                            std::format("invalid {} task option '{}'", socket_property, argv[j])};
                }
            }
            std::vector<std::filesystem::path> file_list;
            GList* l = nullptr; // file list
            char* target_dir = nullptr;
            for (; argv[j]; ++j)
            {
                if (!ztd::same(socket_property, "delete") && !argv[j + 1])
                {
                    // last argument - use as TARGET
                    if (argv[j][0] != '/')
                    {
                        g_list_foreach(l, (GFunc)std::free, nullptr);
                        g_list_free(l);
                        return {SOCKET_INVALID, std::format("no such directory '{}'", argv[j])};
                    }
                    target_dir = argv[j];
                    break;
                }
                else
                {
                    std::filesystem::path str;
                    if (ztd::startswith(argv[j], "/"))
                    { // absolute path
                        str = argv[j];
                    }
                    else
                    {
                        // relative path
                        if (!opt_cwd)
                        {
                            g_list_foreach(l, (GFunc)std::free, nullptr);
                            g_list_free(l);
                            return {SOCKET_INVALID,
                                    std::format("relative path '{}' requires {} option --dir DIR",
                                                argv[j],
                                                argv[i])};
                        }
                        str = std::filesystem::path() / opt_cwd / argv[j];
                    }
                    file_list.emplace_back(str);
                }
            }
            if (file_list.empty() || (!ztd::same(socket_property, "delete") && !target_dir))
            {
                return {SOCKET_INVALID,
                        std::format("task type {} requires FILE argument(s)", argv[i])};
            }
            VFSFileTaskType task_type;
            if (ztd::same(socket_property, "copy"))
            {
                task_type = VFSFileTaskType::COPY;
            }
            else if (ztd::same(socket_property, "move"))
            {
                task_type = VFSFileTaskType::MOVE;
            }
            else if (ztd::same(socket_property, "link"))
            {
                task_type = VFSFileTaskType::LINK;
            }
            else if (ztd::same(socket_property, "delete"))
            {
                task_type = VFSFileTaskType::DELETE;
            }
            else if (ztd::same(socket_property, "trash"))
            {
                task_type = VFSFileTaskType::TRASH;
            }
            else
            { // failsafe
                return {SOCKET_FAILURE, ""};
            }
            PtkFileTask* ptask =
                ptk_file_task_new(task_type,
                                  file_list,
                                  target_dir,
                                  GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(file_browser))),
                                  file_browser->task_view);
            ptk_file_task_run(ptask);
            return {SOCKET_SUCCESS,
                    std::format("# Note: $new_task_id not valid until approx one "
                                "half second after task  start\nnew_task_window={}\n"
                                "new_task_id={}",
                                (void*)main_window,
                                (void*)ptask)};
        }
        else
        {
            return {SOCKET_INVALID, std::format("invalid task type '{}'", argv[i])};
        }
    }
    else if (ztd::same(socket_cmd, "emit-key"))
    { // KEYCODE [KEYMOD]
        if (!argv[i])
        {
            return {SOCKET_FAILURE, std::format("command {} requires an argument", socket_cmd)};
        }
        // this only handles keys assigned to menu items
        const auto event = (GdkEventKey*)gdk_event_new(GdkEventType::GDK_KEY_PRESS);
        event->keyval = std::stoul(socket_property, nullptr, 0);
        event->state = argv[i + 1] ? std::stoul(argv[i + 1], nullptr, 0) : 0;
        if (event->keyval)
        {
            gtk_window_present(GTK_WINDOW(main_window));
            on_main_window_keypress(main_window, event, nullptr);
        }
        else
        {
            gdk_event_free((GdkEvent*)event);
            return {SOCKET_INVALID, std::format("invalid keycode '{}'", argv[i])};
        }
        gdk_event_free((GdkEvent*)event);
    }
    else if (ztd::same(socket_cmd, "activate"))
    {
        if (!argv[i])
        {
            return {SOCKET_FAILURE, std::format("command {} requires an argument", socket_cmd)};
        }
        xset_t set = xset_find_custom(argv[i]);
        if (!set)
        {
            return {SOCKET_INVALID,
                    std::format("custom command or submenu '{}' not found", argv[i])};
        }
        const xset_context_t context = xset_context_new();
        main_context_fill(file_browser, context);
        if (context && context->valid)
        {
            if (!xset_get_b(xset::name::context_dlg) &&
                xset_context_test(context, set->context.value(), false) !=
                    ItemPropContextState::CONTEXT_SHOW)
            {
                return {SOCKET_INVALID,
                        std::format("item '{}' context hidden or disabled", argv[i])};
            }
        }
        if (set->menu_style == xset::menu::submenu)
        {
            // show submenu as popup menu
            set = xset_get(set->child.value());
            GtkWidget* widget = gtk_menu_new();
            GtkAccelGroup* accel_group = gtk_accel_group_new();

            xset_add_menuitem(file_browser, GTK_WIDGET(widget), accel_group, set);
            g_idle_add((GSourceFunc)delayed_show_menu, widget);
        }
        else
        {
            // activate item
            on_main_window_keypress(nullptr, nullptr, set);
        }
    }
    else if (ztd::same(socket_cmd, "add-event") || ztd::same(socket_cmd, "replace-event") ||
             ztd::same(socket_cmd, "remove-event"))
    {
        xset_t set;

        if (!(argv[i] && argv[i + 1]))
        {
            return {SOCKET_FAILURE, std::format("{} requires two arguments", socket_cmd)};
        }
        if (!(set = xset_is(argv[i])))
        {
            return {SOCKET_INVALID, std::format("invalid event type '{}'", argv[i])};
        }
        // build command
        std::string str = (ztd::same(socket_cmd, "replace-event") ? "*" : "");
        for (i32 j = i + 1; argv[j]; ++j)
        {
            str.append(std::format("{}{}", j == i + 1 ? "" : " ", argv[j]));
        }
        // modify list
        GList* l = nullptr;
        if (ztd::same(socket_cmd, "remove-event"))
        {
            l = g_list_find_custom((GList*)set->ob2_data, str.data(), (GCompareFunc)ztd::compare);
            if (!l)
            {
                // remove replace event
                const std::string str2 = std::format("*{}", str);
                l = g_list_find_custom((GList*)set->ob2_data,
                                       str2.data(),
                                       (GCompareFunc)ztd::compare);
            }
            if (!l)
            {
                return {SOCKET_INVALID, "event handler not found"};
            }
            l = g_list_remove((GList*)set->ob2_data, l->data);
        }
        else
        {
            l = g_list_append((GList*)set->ob2_data, ztd::strdup(str));
        }
        set->ob2_data = (void*)l;
    }
    else if (ztd::same(socket_cmd, "help"))
    {
        return {SOCKET_SUCCESS, "For help run, 'man spacefm-socket'"};
    }
    else if (ztd::same(socket_cmd, "ping"))
    {
        return {SOCKET_SUCCESS, "pong"};
    }
    else
    {
        return {SOCKET_FAILURE, std::format("invalid socket method '{}'", socket_cmd)};
    }
    return {SOCKET_SUCCESS, ""};
}

static bool
run_event(MainWindow* main_window, PtkFileBrowser* file_browser, xset_t preset, xset::name event,
          i32 panel, i32 tab, const char* focus, i32 keyval, i32 button, i32 state, bool visible,
          xset_t set, char* ucmd)
{
    bool inhibit;
    i32 exit_status;

    const std::string event_name = xset::get_name_from_xsetname(event);

    if (!ucmd)
    {
        return false;
    }

    if (ucmd[0] == '*')
    {
        ucmd++;
        inhibit = true;
    }
    else
    {
        inhibit = false;
    }

    if (!preset && (event == xset::name::evt_start || event == xset::name::evt_exit ||
                    event == xset::name::evt_device))
    {
        std::string cmd = ucmd;
        cmd = ztd::replace(cmd, "%e", event_name);

        if (event == xset::name::evt_device)
        {
            if (!focus)
            {
                return false;
            }
            cmd = ztd::replace(cmd, "%f", focus);
            std::string change;
            switch (state)
            {
                case VFSVolumeState::ADDED:
                    change = "added";
                    break;
                case VFSVolumeState::REMOVED:
                    change = "removed";
                    break;
                default:
                    change = "changed";
                    break;
            }
            cmd = ztd::replace(cmd, "%v", change);
        }
        ztd::logger::info("EVENT {} >>> {}", event_name, cmd);
        const std::string command = std::format("{} -c {}", FISH_PATH, cmd);
        Glib::spawn_command_line_async(command);
        return false;
    }

    if (!main_window)
    {
        return false;
    }

    // replace vars
    std::string replace;
    if (set == event_handler->win_click)
    {
        replace = "%e %w %p %t %f %b %m";
        state = (state & (GdkModifierType::GDK_SHIFT_MASK | GdkModifierType::GDK_CONTROL_MASK |
                          GdkModifierType::GDK_MOD1_MASK | GdkModifierType::GDK_SUPER_MASK |
                          GdkModifierType::GDK_HYPER_MASK | GdkModifierType::GDK_META_MASK));
    }
    else if (set == event_handler->win_key)
    {
        replace = "%e %w %p %t %k %m";
    }
    else if (set == event_handler->pnl_show)
    {
        replace = "%e %w %p %t %f %v";
    }
    else if (set == event_handler->tab_chdir)
    {
        replace = "%e %w %p %t %d";
    }
    else
    {
        replace = "%e %w %p %t";
    }

    /**
     * %w  windowid
     * %p  panel
     * %t  tab
     * %f  focus
     * %e  event
     * %k  keycode
     * %m  modifier
     * %b  button
     * %v  visible
     * %d  cwd
     */
    std::string rep;
    std::string cmd = ucmd;
    if (ztd::contains(replace, "%f"))
    {
        if (!focus)
        {
            rep = std::format("panel{}", panel);
            cmd = ztd::replace(cmd, "%f", rep);
        }
        else
        {
            cmd = ztd::replace(cmd, "%f", focus);
        }
    }
    else if (ztd::contains(replace, "%w"))
    {
        rep = std::format("{:p}", (void*)main_window);
        cmd = ztd::replace(cmd, "%w", rep);
    }
    else if (ztd::contains(replace, "%p"))
    {
        rep = std::format("{}", panel);
        cmd = ztd::replace(cmd, "%p", rep);
    }
    else if (ztd::contains(replace, "%t"))
    {
        rep = std::format("{}", tab);
        cmd = ztd::replace(cmd, "%t", rep);
    }
    else if (ztd::contains(replace, "%v"))
    {
        cmd = ztd::replace(cmd, "%v", visible ? "1" : "0");
    }
    else if (ztd::contains(replace, "%k"))
    {
        rep = std::format("{:#x}", keyval);
        cmd = ztd::replace(cmd, "%k", rep);
    }
    else if (ztd::contains(replace, "%b"))
    {
        rep = std::format("{}", button);
        cmd = ztd::replace(cmd, "%b", rep);
    }
    else if (ztd::contains(replace, "%m"))
    {
        rep = std::format("{:#x}", state);
        cmd = ztd::replace(cmd, "%m", rep);
    }
    else if (ztd::contains(replace, "%d"))
    {
        if (file_browser)
        {
            rep = ztd::shell::quote(ptk_file_browser_get_cwd(file_browser).string());
            cmd = ztd::replace(cmd, "%d", rep);
        }
    }

    if (!inhibit)
    {
        ztd::logger::info("EVENT {} >>> {}", event_name, cmd);
        if (event == xset::name::evt_tab_close)
        {
            const std::string command = std::format("{} -c {}", FISH_PATH, cmd);
            // file_browser becomes invalid so spawn
            Glib::spawn_command_line_async(command);
        }
        else
        {
            // task
            PtkFileTask* ptask = ptk_file_exec_new(event_name,
                                                   ptk_file_browser_get_cwd(file_browser),
                                                   GTK_WIDGET(file_browser),
                                                   main_window->task_view);
            ptask->task->exec_browser = file_browser;
            ptask->task->exec_command = cmd;
            if (set->icon)
            {
                ptask->task->exec_icon = set->icon.value();
            }
            ptask->task->exec_sync = false;
            ptask->task->exec_export = true;
            ptk_file_task_run(ptask);
        }
        return false;
    }

    ztd::logger::info("REPLACE_EVENT {} >>> {}", event_name, cmd);

    inhibit = false;
    const std::string command = std::format("{} -c {}", FISH_PATH, cmd);
    Glib::spawn_command_line_sync(command, nullptr, nullptr, &exit_status);

    if (WIFEXITED(exit_status) && WEXITSTATUS(exit_status) == 0)
    {
        inhibit = true;
    }

    ztd::logger::info("REPLACE_EVENT ? {}", inhibit ? "true" : "false");
    return inhibit;
}

bool
main_window_event(void* mw, xset_t preset, xset::name event, i64 panel, i64 tab, const char* focus,
                  i32 keyval, i32 button, i32 state, bool visible)
{
    xset_t set;
    bool inhibit = false;

    // ztd::logger::info("main_window_event {}", xset::get_name_from_xsetname(event));

    if (preset)
    {
        set = preset;
    }
    else
    {
        set = xset_get(event);
        if (!set->s && !set->ob2_data)
        {
            return false;
        }
    }

    // get main_window, panel, and tab
    MainWindow* main_window;
    PtkFileBrowser* file_browser;
    if (!mw)
    {
        main_window = main_window_get_last_active();
    }
    else
    {
        main_window = MAIN_WINDOW(mw);
    }
    if (main_window)
    {
        file_browser =
            PTK_FILE_BROWSER_REINTERPRET(main_window_get_current_file_browser(main_window));
        if (!file_browser)
        {
            return false;
        }
        if (!panel)
        {
            panel = main_window->curpanel;
        }
        if (!tab)
        {
            tab = gtk_notebook_page_num(GTK_NOTEBOOK(main_window->panel[file_browser->mypanel - 1]),
                                        GTK_WIDGET(file_browser)) +
                  1;
        }
    }
    else
    {
        file_browser = nullptr;
    }

    // dynamic handlers
    if (set->ob2_data)
    {
        for (GList* l = (GList*)set->ob2_data; l; l = g_list_next(l))
        {
            if (run_event(main_window,
                          file_browser,
                          preset,
                          event,
                          panel,
                          tab,
                          focus,
                          keyval,
                          button,
                          state,
                          visible,
                          set,
                          (char*)l->data))
            {
                inhibit = true;
            }
        }
    }

    // Events menu handler
    return (run_event(main_window,
                      file_browser,
                      preset,
                      event,
                      panel,
                      tab,
                      focus,
                      keyval,
                      button,
                      state,
                      visible,
                      set,
                      set->s.value().data()) ||
            inhibit);
}
