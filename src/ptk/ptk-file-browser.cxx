/*
 *
 * License: See COPYING file
 *
 */

#include <malloc.h>

#include <fnmatch.h>
#include <fcntl.h>

#include <exo/exo.h>

#include "ptk-utils.hxx"
#include "ptk-file-misc.hxx"

#include "ptk-location-view.hxx"
#include "ptk-dir-tree-view.hxx"
#include "ptk-dir-tree.hxx"

#include "ptk-file-list.hxx"

#include "ptk-clipboard.hxx"

#include "ptk-file-menu.hxx"
#include "ptk-path-entry.hxx"
#include "main-window.hxx"

#include "utils.hxx"

static void ptk_file_browser_class_init(PtkFileBrowserClass* klass);
static void ptk_file_browser_init(PtkFileBrowser* file_browser);
static void ptk_file_browser_finalize(GObject* obj);
static void ptk_file_browser_get_property(GObject* obj, unsigned int prop_id, GValue* value,
                                          GParamSpec* pspec);
static void ptk_file_browser_set_property(GObject* obj, unsigned int prop_id, const GValue* value,
                                          GParamSpec* pspec);

/* Utility functions */
static GtkWidget* create_folder_view(PtkFileBrowser* file_browser, PtkFBViewMode view_mode);

static void init_list_view(PtkFileBrowser* file_browser, GtkTreeView* list_view);

static GtkWidget* ptk_file_browser_create_dir_tree(PtkFileBrowser* file_browser);

static void on_dir_file_listed(VFSDir* dir, bool is_cancelled, PtkFileBrowser* file_browser);

static void ptk_file_browser_update_model(PtkFileBrowser* file_browser);

/* Get GtkTreePath of the item at coordinate x, y */
static GtkTreePath* folder_view_get_tree_path_at_pos(PtkFileBrowser* file_browser, int x, int y);

/* signal handlers */
static void on_folder_view_item_activated(ExoIconView* iconview, GtkTreePath* path,
                                          PtkFileBrowser* file_browser);
static void on_folder_view_row_activated(GtkTreeView* tree_view, GtkTreePath* path,
                                         GtkTreeViewColumn* col, PtkFileBrowser* file_browser);
static void on_folder_view_item_sel_change(ExoIconView* iconview, PtkFileBrowser* file_browser);

static bool on_folder_view_button_press_event(GtkWidget* widget, GdkEventButton* event,
                                              PtkFileBrowser* file_browser);
static bool on_folder_view_button_release_event(GtkWidget* widget, GdkEventButton* event,
                                                PtkFileBrowser* file_browser);
static bool on_folder_view_popup_menu(GtkWidget* widget, PtkFileBrowser* file_browser);

void on_dir_tree_row_activated(GtkTreeView* view, GtkTreePath* path, GtkTreeViewColumn* column,
                               PtkFileBrowser* file_browser);

/* Drag & Drop */
static void on_folder_view_drag_data_received(GtkWidget* widget, GdkDragContext* drag_context,
                                              int x, int y, GtkSelectionData* sel_data,
                                              unsigned int info, unsigned int time,
                                              void* user_data);

static void on_folder_view_drag_data_get(GtkWidget* widget, GdkDragContext* drag_context,
                                         GtkSelectionData* sel_data, unsigned int info,
                                         unsigned int time, PtkFileBrowser* file_browser);

static void on_folder_view_drag_begin(GtkWidget* widget, GdkDragContext* drag_context,
                                      PtkFileBrowser* file_browser);

static bool on_folder_view_drag_motion(GtkWidget* widget, GdkDragContext* drag_context, int x,
                                       int y, unsigned int time, PtkFileBrowser* file_browser);

static bool on_folder_view_drag_leave(GtkWidget* widget, GdkDragContext* drag_context,
                                      unsigned int time, PtkFileBrowser* file_browser);

static bool on_folder_view_drag_drop(GtkWidget* widget, GdkDragContext* drag_context, int x, int y,
                                     unsigned int time, PtkFileBrowser* file_browser);

static void on_folder_view_drag_end(GtkWidget* widget, GdkDragContext* drag_context,
                                    PtkFileBrowser* file_browser);

/* Default signal handlers */
static void ptk_file_browser_before_chdir(PtkFileBrowser* file_browser, const char* path,
                                          bool* cancel);
static void ptk_file_browser_after_chdir(PtkFileBrowser* file_browser);
static void ptk_file_browser_content_change(PtkFileBrowser* file_browser);
static void ptk_file_browser_sel_change(PtkFileBrowser* file_browser);
static void ptk_file_browser_open_item(PtkFileBrowser* file_browser, const char* path, int action);
static void ptk_file_browser_pane_mode_change(PtkFileBrowser* file_browser);
static void focus_folder_view(PtkFileBrowser* file_browser);
static void enable_toolbar(PtkFileBrowser* file_browser);
static void show_thumbnails(PtkFileBrowser* file_browser, PtkFileList* list, bool is_big,
                            int max_file_size);

static int file_list_order_from_sort_order(PtkFBSortOrder order);

static GtkPanedClass* parent_class = nullptr;

enum PTKFileBrowserSignal
{
    BEFORE_CHDIR_SIGNAL,
    BEGIN_CHDIR_SIGNAL,
    AFTER_CHDIR_SIGNAL,
    OPEN_ITEM_SIGNAL,
    CONTENT_CHANGE_SIGNAL,
    SEL_CHANGE_SIGNAL,
    PANE_MODE_CHANGE_SIGNAL,
    N_SIGNALS
};

enum PTKFileBrowserResponse
{ // MOD
    RESPONSE_RUN = 100,
    RESPONSE_RUNTERMINAL = 101,
};

static void rebuild_toolbox(GtkWidget* widget, PtkFileBrowser* file_browser);
static void rebuild_side_toolbox(GtkWidget* widget, PtkFileBrowser* file_browser);

static unsigned int signals[N_SIGNALS] = {0};

static unsigned int folder_view_auto_scroll_timer = 0;
static GtkDirectionType folder_view_auto_scroll_direction = GTK_DIR_TAB_FORWARD;

/*  Drag & Drop/Clipboard targets  */
static GtkTargetEntry drag_targets[] = {{g_strdup("text/uri-list"), 0, 0}};

#define GDK_ACTION_ALL (GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK)

// must match main-window.c  main_window_socket_command
static const char* column_titles[] = {"Name", "Size", "Type", "Permission", "Owner", "Modified"};

static const char* column_names[] =
    {"detcol_name", "detcol_size", "detcol_type", "detcol_perm", "detcol_owner", "detcol_date"};

GType
ptk_file_browser_get_type()
{
    static GType type = G_TYPE_INVALID;
    if (G_UNLIKELY(type == G_TYPE_INVALID))
    {
        static const GTypeInfo info = {
            sizeof(PtkFileBrowserClass),
            nullptr,
            nullptr,
            (GClassInitFunc)ptk_file_browser_class_init,
            nullptr,
            nullptr,
            sizeof(PtkFileBrowser),
            0,
            (GInstanceInitFunc)ptk_file_browser_init,
            nullptr,
        };
        type = g_type_register_static(GTK_TYPE_VBOX, "PtkFileBrowser", &info, (GTypeFlags)0);
    }
    return type;
}

/* These g_cclosure_marshal functions are from gtkmarshal.c, the deprecated
 * functions renamed from gtk_* to g_cclosure_*, to match the naming convention
 * of the non-deprecated glib functions.   Added for gtk3 port. */
static void
g_cclosure_marshal_VOID__POINTER_POINTER(GClosure* closure, GValue* return_value G_GNUC_UNUSED,
                                         unsigned int n_param_values, const GValue* param_values,
                                         void* invocation_hint G_GNUC_UNUSED, void* marshal_data)
{
    GCClosure* cc = (GCClosure*)closure;
    void* data1;
    void* data2;

    g_return_if_fail(n_param_values == 3);

    if (G_CCLOSURE_SWAP_DATA(closure))
    {
        data1 = closure->data;
        data2 = g_value_peek_pointer(param_values + 0);
    }
    else
    {
        data1 = g_value_peek_pointer(param_values + 0);
        data2 = closure->data;
    }

    typedef void (
        *GMarshalFunc_VOID__POINTER_POINTER)(void* data1, void* arg_1, void* arg_2, void* data2);

    GMarshalFunc_VOID__POINTER_POINTER callback =
        (GMarshalFunc_VOID__POINTER_POINTER)(marshal_data ? marshal_data : cc->callback);

    callback(data1,
             g_value_get_pointer(param_values + 1),
             g_value_get_pointer(param_values + 2),
             data2);
}

static void
g_cclosure_marshal_VOID__POINTER_INT(GClosure* closure, GValue* return_value,
                                     unsigned int n_param_values, const GValue* param_values,
                                     void* invocation_hint, void* marshal_data)
{
    typedef void (
        *GMarshalFunc_VOID__POINTER_INT)(void* data1, void* arg_1, int arg_2, void* data2);
    GMarshalFunc_VOID__POINTER_INT callback;
    GCClosure* cc = (GCClosure*)closure;
    void* data1;
    void* data2;

    g_return_if_fail(n_param_values == 3);

    if (G_CCLOSURE_SWAP_DATA(closure))
    {
        data1 = closure->data;
        data2 = g_value_peek_pointer(param_values + 0);
    }
    else
    {
        data1 = g_value_peek_pointer(param_values + 0);
        data2 = closure->data;
    }
    callback = (GMarshalFunc_VOID__POINTER_INT)(marshal_data ? marshal_data : cc->callback);

    callback(data1,
             g_value_get_pointer(param_values + 1),
             g_value_get_int(param_values + 2),
             data2);
}

static void
ptk_file_browser_class_init(PtkFileBrowserClass* klass)
{
    GObjectClass* object_class = (GObjectClass*)klass;
    parent_class = (GtkPanedClass*)g_type_class_peek_parent(klass);

    object_class->set_property = ptk_file_browser_set_property;
    object_class->get_property = ptk_file_browser_get_property;
    object_class->finalize = ptk_file_browser_finalize;

    /* Signals */

    klass->before_chdir = ptk_file_browser_before_chdir;
    klass->after_chdir = ptk_file_browser_after_chdir;
    klass->open_item = ptk_file_browser_open_item;
    klass->content_change = ptk_file_browser_content_change;
    klass->sel_change = ptk_file_browser_sel_change;
    klass->pane_mode_change = ptk_file_browser_pane_mode_change;

    /* before-chdir is emitted when PtkFileBrowser is about to change
     * its working directory. The 1st param is the path of the dir (in UTF-8),
     * and the 2nd param is a bool*, which can be filled by the
     * signal handler with true to cancel the operation.
     */
    signals[BEFORE_CHDIR_SIGNAL] = g_signal_new("before-chdir",
                                                G_TYPE_FROM_CLASS(klass),
                                                G_SIGNAL_RUN_LAST,
                                                G_STRUCT_OFFSET(PtkFileBrowserClass, before_chdir),
                                                nullptr,
                                                nullptr,
                                                g_cclosure_marshal_VOID__POINTER_POINTER,
                                                G_TYPE_NONE,
                                                2,
                                                G_TYPE_POINTER,
                                                G_TYPE_POINTER);

    signals[BEGIN_CHDIR_SIGNAL] = g_signal_new("begin-chdir",
                                               G_TYPE_FROM_CLASS(klass),
                                               G_SIGNAL_RUN_LAST,
                                               G_STRUCT_OFFSET(PtkFileBrowserClass, begin_chdir),
                                               nullptr,
                                               nullptr,
                                               g_cclosure_marshal_VOID__VOID,
                                               G_TYPE_NONE,
                                               0);

    signals[AFTER_CHDIR_SIGNAL] = g_signal_new("after-chdir",
                                               G_TYPE_FROM_CLASS(klass),
                                               G_SIGNAL_RUN_LAST,
                                               G_STRUCT_OFFSET(PtkFileBrowserClass, after_chdir),
                                               nullptr,
                                               nullptr,
                                               g_cclosure_marshal_VOID__VOID,
                                               G_TYPE_NONE,
                                               0);

    /*
     * This signal is sent when a directory is about to be opened
     * arg1 is the path to be opened, and arg2 is the type of action,
     * ex: open in tab, open in terminal...etc.
     */
    signals[OPEN_ITEM_SIGNAL] = g_signal_new("open-item",
                                             G_TYPE_FROM_CLASS(klass),
                                             G_SIGNAL_RUN_LAST,
                                             G_STRUCT_OFFSET(PtkFileBrowserClass, open_item),
                                             nullptr,
                                             nullptr,
                                             g_cclosure_marshal_VOID__POINTER_INT,
                                             G_TYPE_NONE,
                                             2,
                                             G_TYPE_POINTER,
                                             G_TYPE_INT);

    signals[CONTENT_CHANGE_SIGNAL] =
        g_signal_new("content-change",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(PtkFileBrowserClass, content_change),
                     nullptr,
                     nullptr,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);

    signals[SEL_CHANGE_SIGNAL] = g_signal_new("sel-change",
                                              G_TYPE_FROM_CLASS(klass),
                                              G_SIGNAL_RUN_LAST,
                                              G_STRUCT_OFFSET(PtkFileBrowserClass, sel_change),
                                              nullptr,
                                              nullptr,
                                              g_cclosure_marshal_VOID__VOID,
                                              G_TYPE_NONE,
                                              0);

    signals[PANE_MODE_CHANGE_SIGNAL] =
        g_signal_new("pane-mode-change",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(PtkFileBrowserClass, pane_mode_change),
                     nullptr,
                     nullptr,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);
}

bool
ptk_file_browser_slider_release(GtkWidget* widget, GdkEventButton* event,
                                PtkFileBrowser* file_browser)
{
    int pos;
    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;
    int p = file_browser->mypanel;
    char mode = main_window->panel_context[p - 1];

    XSet* set = xset_get_panel_mode(p, "slider_positions", mode);

    if (widget == file_browser->hpane)
    {
        pos = gtk_paned_get_position(GTK_PANED(file_browser->hpane));
        if (!main_window->fullscreen)
        {
            g_free(set->x);
            set->x = g_strdup_printf("%d", pos);
        }
        main_window->panel_slide_x[p - 1] = pos;
        // printf("    slide_x = %d\n", pos );
    }
    else
    {
        // printf("ptk_file_browser_slider_release fb=%#x  (panel %d)  mode = %d\n", file_browser,
        // p, mode );
        pos = gtk_paned_get_position(GTK_PANED(file_browser->side_vpane_top));
        if (!main_window->fullscreen)
        {
            g_free(set->y);
            set->y = g_strdup_printf("%d", pos);
        }
        main_window->panel_slide_y[p - 1] = pos;
        // printf("    slide_y = %d  ", pos );

        pos = gtk_paned_get_position(GTK_PANED(file_browser->side_vpane_bottom));
        if (!main_window->fullscreen)
        {
            g_free(set->s);
            set->s = g_strdup_printf("%d", pos);
        }
        main_window->panel_slide_s[p - 1] = pos;
        // printf("slide_s = %d\n", pos );
    }
    return false;
}

void
ptk_file_browser_select_file(PtkFileBrowser* file_browser, const char* path)
{
    GtkTreeIter it;
    GtkTreeSelection* tree_sel;
    GtkTreeModel* model = nullptr;
    VFSFileInfo* file;

    PtkFileList* list = PTK_FILE_LIST(file_browser->file_list);
    if (file_browser->view_mode == PTK_FB_ICON_VIEW ||
        file_browser->view_mode == PTK_FB_COMPACT_VIEW)
    {
        exo_icon_view_unselect_all(EXO_ICON_VIEW(file_browser->folder_view));
        model = exo_icon_view_get_model(EXO_ICON_VIEW(file_browser->folder_view));
    }
    else if (file_browser->view_mode == PTK_FB_LIST_VIEW)
    {
        model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_browser->folder_view));
        tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
        gtk_tree_selection_unselect_all(tree_sel);
    }
    if (!model)
        return;
    char* name = g_path_get_basename(path);

    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            gtk_tree_model_get(model, &it, COL_FILE_INFO, &file, -1);
            if (file)
            {
                const char* file_name = vfs_file_info_get_name(file);
                if (!strcmp(file_name, name))
                {
                    GtkTreePath* tree_path = gtk_tree_model_get_path(GTK_TREE_MODEL(list), &it);
                    if (file_browser->view_mode == PTK_FB_ICON_VIEW ||
                        file_browser->view_mode == PTK_FB_COMPACT_VIEW)
                    {
                        exo_icon_view_select_path(EXO_ICON_VIEW(file_browser->folder_view),
                                                  tree_path);
                        exo_icon_view_set_cursor(EXO_ICON_VIEW(file_browser->folder_view),
                                                 tree_path,
                                                 nullptr,
                                                 false);
                        exo_icon_view_scroll_to_path(EXO_ICON_VIEW(file_browser->folder_view),
                                                     tree_path,
                                                     true,
                                                     .25,
                                                     0);
                    }
                    else if (file_browser->view_mode == PTK_FB_LIST_VIEW)
                    {
                        gtk_tree_selection_select_path(tree_sel, tree_path);
                        gtk_tree_view_set_cursor(GTK_TREE_VIEW(file_browser->folder_view),
                                                 tree_path,
                                                 nullptr,
                                                 false);
                        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(file_browser->folder_view),
                                                     tree_path,
                                                     nullptr,
                                                     true,
                                                     .25,
                                                     0);
                    }
                    gtk_tree_path_free(tree_path);
                    vfs_file_info_unref(file);
                    break;
                }
                vfs_file_info_unref(file);
            }
        } while (gtk_tree_model_iter_next(model, &it));
    }
    g_free(name);
}

static void
save_command_history(GtkEntry* entry)
{
    EntryData* edata = (EntryData*)g_object_get_data(G_OBJECT(entry), "edata");
    if (!edata)
        return;
    const char* text = gtk_entry_get_text(GTK_ENTRY(entry));

    // remove duplicates
    GList* l;
    while ((l = g_list_find_custom(xset_cmd_history, text, (GCompareFunc)g_strcmp0)))
    {
        g_free((char*)l->data);
        xset_cmd_history = g_list_delete_link(xset_cmd_history, l);
    }
    xset_cmd_history = g_list_prepend(xset_cmd_history, g_strdup(text));
    // shorten to 200 entries
    while (g_list_length(xset_cmd_history) > 200)
    {
        l = g_list_last(xset_cmd_history);
        g_free((char*)l->data);
        xset_cmd_history = g_list_delete_link(xset_cmd_history, l);
    }
}

static bool
on_address_bar_focus_in(GtkWidget* entry, GdkEventFocus* evt, PtkFileBrowser* file_browser)
{
    ptk_file_browser_focus_me(file_browser);
    return false;
}

static void
on_address_bar_activate(GtkWidget* entry, PtkFileBrowser* file_browser)
{
    const char* text = gtk_entry_get_text(GTK_ENTRY(entry));

    gtk_editable_select_region((GtkEditable*)entry, 0, 0); // clear selection

    // Convert to on-disk encoding
    char* dir_path = g_filename_from_utf8(text, -1, nullptr, nullptr, nullptr);
    char* final_path = vfs_file_resolve_path(ptk_file_browser_get_cwd(file_browser), dir_path);
    g_free(dir_path);

    if (text[0] == '\0')
    {
        g_free(final_path);
        return;
    }

    char* str;
    bool final_path_exists = g_file_test(final_path, G_FILE_TEST_EXISTS);

    if (!final_path_exists &&
        (text[0] == '$' || text[0] == '+' || text[0] == '&' || text[0] == '!' || text[0] == '\0'))
    {
        // command
        char* command;
        char* trim_command;
        bool as_root = false;
        bool in_terminal = false;
        bool as_task = true;
        char* prefix = g_strdup("");
        while (text[0] == '$' || text[0] == '+' || text[0] == '&' || text[0] == '!')
        {
            if (text[0] == '+')
                in_terminal = true;
            else if (text[0] == '&')
                as_task = false;
            else if (text[0] == '!')
                as_root = true;

            str = prefix;
            prefix = g_strdup_printf("%s%c", str, text[0]);
            g_free(str);
            text++;
        }
        bool is_space = text[0] == ' ';
        command = g_strdup(text);
        trim_command = g_strstrip(command);
        if (trim_command[0] == '\0')
        {
            g_free(command);
            g_free(prefix);
            ptk_path_entry_help(entry, GTK_WIDGET(file_browser));
            gtk_editable_set_position(GTK_EDITABLE(entry), -1);
            return;
        }

        save_command_history(GTK_ENTRY(entry));

        // task
        char* task_name;
        const char* cwd;
        task_name = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
        cwd = ptk_file_browser_get_cwd(file_browser);
        PtkFileTask* task;
        task = ptk_file_exec_new(task_name, cwd, GTK_WIDGET(file_browser), file_browser->task_view);
        g_free(task_name);
        // don't free cwd!
        task->task->exec_browser = file_browser;
        task->task->exec_command = replace_line_subs(trim_command);
        g_free(command);
        if (as_root)
            task->task->exec_as_user = g_strdup_printf("root");
        if (!as_task)
            task->task->exec_sync = false;
        else
            task->task->exec_sync = !in_terminal;
        task->task->exec_show_output = true;
        task->task->exec_show_error = true;
        task->task->exec_export = true;
        task->task->exec_terminal = in_terminal;
        task->task->exec_keep_terminal = as_task;
        // task->task->exec_keep_tmp = true;
        ptk_file_task_run(task);
        // gtk_widget_grab_focus( GTK_WIDGET( file_browser->folder_view ) );

        // reset entry text
        str = prefix;
        prefix = g_strdup_printf("%s%s", str, is_space ? " " : "");
        g_free(str);
        gtk_entry_set_text(GTK_ENTRY(entry), prefix);
        g_free(prefix);
        gtk_editable_set_position(GTK_EDITABLE(entry), -1);
    }
    else if (!final_path_exists && text[0] == '%')
    {
        str = g_strdup(++text);
        g_strstrip(str);
        if (str && str[0] != '\0')
        {
            save_command_history(GTK_ENTRY(entry));
            ptk_file_browser_select_pattern(nullptr, file_browser, str);
        }
        g_free(str);
    }
    else if ((text[0] != '/' && strstr(text, ":/")) || g_str_has_prefix(text, "//"))
    {
        save_command_history(GTK_ENTRY(entry));
        str = g_strdup(text);
        ptk_location_view_mount_network(file_browser, str, false, false);
        g_free(str);
        return;
    }
    else
    {
        // path?
        // clean double slashes
        while (strstr(final_path, "//"))
        {
            str = final_path;
            final_path = replace_string(str, "//", "/", false);
            g_free(str);
        }
        if (g_file_test(final_path, G_FILE_TEST_IS_DIR))
        {
            // open dir
            if (strcmp(final_path, ptk_file_browser_get_cwd(file_browser)))
                ptk_file_browser_chdir(file_browser, final_path, PTK_FB_CHDIR_ADD_HISTORY);
            gtk_widget_grab_focus(GTK_WIDGET(file_browser->folder_view));
        }
        else if (final_path_exists)
        {
            struct stat statbuf;
            if (stat(final_path, &statbuf) == 0 && S_ISBLK(statbuf.st_mode) &&
                ptk_location_view_open_block(final_path, false))
            {
                // ptk_location_view_open_block opened device
            }
            else
            {
                // open dir and select file
                dir_path = g_path_get_dirname(final_path);
                if (strcmp(dir_path, ptk_file_browser_get_cwd(file_browser)))
                {
                    g_free(file_browser->select_path);
                    file_browser->select_path = g_strdup(final_path);
                    ptk_file_browser_chdir(file_browser, dir_path, PTK_FB_CHDIR_ADD_HISTORY);
                }
                else
                    ptk_file_browser_select_file(file_browser, final_path);
                g_free(dir_path);
            }
            gtk_widget_grab_focus(GTK_WIDGET(file_browser->folder_view));
        }
        gtk_editable_set_position(GTK_EDITABLE(entry), -1);

        // inhibit auto seek because if multiple completions will change dir
        EntryData* edata = (EntryData*)g_object_get_data(G_OBJECT(entry), "edata");
        if (edata && edata->seek_timer)
        {
            g_source_remove(edata->seek_timer);
            edata->seek_timer = 0;
        }
    }
    g_free(final_path);
}

void
ptk_file_browser_add_toolbar_widget(void* set_ptr, GtkWidget* widget)
{ // store the toolbar widget created by set for later change of status
    XSet* set = (XSet*)set_ptr;

    if (!(set && !set->lock && set->browser && set->tool && GTK_IS_WIDGET(widget)))
        return;

    unsigned char x;

    switch (set->tool)
    {
        case XSET_TOOL_UP:
            x = 0;
            break;
        case XSET_TOOL_BACK:
        case XSET_TOOL_BACK_MENU:
            x = 1;
            break;
        case XSET_TOOL_FWD:
        case XSET_TOOL_FWD_MENU:
            x = 2;
            break;
        case XSET_TOOL_DEVICES:
            x = 3;
            break;
        case XSET_TOOL_BOOKMARKS:
            x = 4;
            break;
        case XSET_TOOL_TREE:
            x = 5;
            break;
        case XSET_TOOL_SHOW_HIDDEN:
            x = 6;
            break;
        case XSET_TOOL_CUSTOM:
            if (set->menu_style == XSET_MENU_CHECK)
            {
                x = 7;
                // attach set pointer to custom checkboxes so we can find it
                g_object_set_data(G_OBJECT(widget), "set", set);
            }
            else
                return;
            break;
        case XSET_TOOL_SHOW_THUMB:
            x = 8;
            break;
        case XSET_TOOL_LARGE_ICONS:
            x = 9;
            break;
        default:
            return;
    }

    set->browser->toolbar_widgets[x] = g_slist_append(set->browser->toolbar_widgets[x], widget);
}

void
ptk_file_browser_update_toolbar_widgets(PtkFileBrowser* file_browser, void* set_ptr, char tool_type)
{
    if (!PTK_IS_FILE_BROWSER(file_browser))
        return;

    unsigned char x;
    GSList* l;
    GtkWidget* widget;
    XSet* set = (XSet*)set_ptr;

    if (set && !set->lock && set->menu_style == XSET_MENU_CHECK && set->tool == XSET_TOOL_CUSTOM)
    {
        // a custom checkbox is being updated
        for (l = file_browser->toolbar_widgets[7]; l; l = l->next)
        {
            if ((XSet*)g_object_get_data(G_OBJECT(l->data), "set") == set)
            {
                widget = GTK_WIDGET(l->data);
                if (GTK_IS_TOGGLE_BUTTON(widget))
                {
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), set->b == XSET_B_TRUE);
                    return;
                }
            }
        }
        g_warning("ptk_file_browser_update_toolbar_widget widget not found for set");
        return;
    }
    else if (set_ptr)
    {
        g_warning("ptk_file_browser_update_toolbar_widget invalid set_ptr or set");
        return;
    }

    // builtin tool
    bool b;

    switch (tool_type)
    {
        case XSET_TOOL_UP:
            const char* cwd;
            x = 0;
            cwd = ptk_file_browser_get_cwd(file_browser);
            b = !cwd || (cwd && strcmp(cwd, "/"));
            break;
        case XSET_TOOL_BACK:
        case XSET_TOOL_BACK_MENU:
            x = 1;
            b = file_browser->curHistory && file_browser->curHistory->prev;
            break;
        case XSET_TOOL_FWD:
        case XSET_TOOL_FWD_MENU:
            x = 2;
            b = file_browser->curHistory && file_browser->curHistory->next;
            break;
        case XSET_TOOL_DEVICES:
            x = 3;
            b = !!file_browser->side_dev;
            break;
        case XSET_TOOL_BOOKMARKS:
            x = 4;
            b = !!file_browser->side_book;
            break;
        case XSET_TOOL_TREE:
            x = 5;
            b = !!file_browser->side_dir;
            break;
        case XSET_TOOL_SHOW_HIDDEN:
            x = 6;
            b = file_browser->show_hidden_files;
            break;
        case XSET_TOOL_SHOW_THUMB:
            x = 8;
            b = app_settings.show_thumbnail;
            break;
        case XSET_TOOL_LARGE_ICONS:
            x = 9;
            b = file_browser->large_icons;
            break;
        default:
            g_warning("ptk_file_browser_update_toolbar_widget invalid tool_type");
            return;
    }

    // update all widgets in list
    for (l = file_browser->toolbar_widgets[x]; l; l = l->next)
    {
        widget = GTK_WIDGET(l->data);
        if (GTK_IS_TOGGLE_BUTTON(widget))
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), b);
        else if (GTK_IS_WIDGET(widget))
            gtk_widget_set_sensitive(widget, b);
        else
        {
            g_warning("ptk_file_browser_update_toolbar_widget invalid widget");
        }
    }
}

static void
enable_toolbar(PtkFileBrowser* file_browser)
{
    ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSET_TOOL_BACK);
    ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSET_TOOL_FWD);
    ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSET_TOOL_UP);
    ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSET_TOOL_DEVICES);
    ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSET_TOOL_BOOKMARKS);
    ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSET_TOOL_TREE);
    ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSET_TOOL_SHOW_HIDDEN);
    ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSET_TOOL_SHOW_THUMB);
    ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSET_TOOL_LARGE_ICONS);
}

static void
rebuild_toolbox(GtkWidget* widget, PtkFileBrowser* file_browser)
{
    // printf(" rebuild_toolbox\n");
    if (!file_browser)
        return;

    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;
    int p = file_browser->mypanel;
    char mode = main_window ? main_window->panel_context[p - 1] : 0;

    bool show_tooltips = !xset_get_b_panel(1, "tool_l");

    // destroy
    if (file_browser->toolbar)
    {
        if (GTK_IS_WIDGET(file_browser->toolbar))
            gtk_widget_destroy(file_browser->toolbar);
        file_browser->toolbar = nullptr;
        file_browser->path_bar = nullptr;
    }

    if (!file_browser->path_bar)
    {
        file_browser->path_bar = ptk_path_entry_new(file_browser);
        g_signal_connect(file_browser->path_bar,
                         "activate",
                         G_CALLBACK(on_address_bar_activate),
                         file_browser);
        g_signal_connect(file_browser->path_bar,
                         "focus-in-event",
                         G_CALLBACK(on_address_bar_focus_in),
                         file_browser);
    }

    // create toolbar
    file_browser->toolbar = gtk_toolbar_new();
    gtk_box_pack_start(GTK_BOX(file_browser->toolbox), file_browser->toolbar, true, true, 0);
    gtk_toolbar_set_style(GTK_TOOLBAR(file_browser->toolbar), GTK_TOOLBAR_ICONS);
    if (app_settings.tool_icon_size > 0 && app_settings.tool_icon_size <= GTK_ICON_SIZE_DIALOG)
        gtk_toolbar_set_icon_size(GTK_TOOLBAR(file_browser->toolbar),
                                  (GtkIconSize)app_settings.tool_icon_size);

    // fill left toolbar
    xset_fill_toolbar(GTK_WIDGET(file_browser),
                      file_browser,
                      file_browser->toolbar,
                      xset_get_panel(p, "tool_l"),
                      show_tooltips);

    // add pathbar
    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkToolItem* toolitem = gtk_tool_item_new();
    gtk_tool_item_set_expand(toolitem, true);
    gtk_toolbar_insert(GTK_TOOLBAR(file_browser->toolbar), toolitem, -1);
    gtk_container_add(GTK_CONTAINER(toolitem), hbox);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(file_browser->path_bar), true, true, 5);

    // fill right toolbar
    xset_fill_toolbar(GTK_WIDGET(file_browser),
                      file_browser,
                      file_browser->toolbar,
                      xset_get_panel(p, "tool_r"),
                      show_tooltips);

    // show
    if (xset_get_b_panel_mode(p, "show_toolbox", mode))
        gtk_widget_show_all(file_browser->toolbox);
}

static void
rebuild_side_toolbox(GtkWidget* widget, PtkFileBrowser* file_browser)
{
    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;
    int p = file_browser->mypanel;
    char mode = main_window ? main_window->panel_context[p - 1] : 0;

    bool show_tooltips = !xset_get_b_panel(1, "tool_l");

    // destroy
    if (file_browser->side_toolbar)
        gtk_widget_destroy(file_browser->side_toolbar);

    // create side toolbar
    file_browser->side_toolbar = gtk_toolbar_new();

    gtk_box_pack_start(GTK_BOX(file_browser->side_toolbox),
                       file_browser->side_toolbar,
                       true,
                       true,
                       0);
    gtk_toolbar_set_style(GTK_TOOLBAR(file_browser->side_toolbar), GTK_TOOLBAR_ICONS);
    if (app_settings.tool_icon_size > 0 && app_settings.tool_icon_size <= GTK_ICON_SIZE_DIALOG)
        gtk_toolbar_set_icon_size(GTK_TOOLBAR(file_browser->side_toolbar),
                                  (GtkIconSize)app_settings.tool_icon_size);
    // fill side toolbar
    xset_fill_toolbar(GTK_WIDGET(file_browser),
                      file_browser,
                      file_browser->side_toolbar,
                      xset_get_panel(p, "tool_s"),
                      show_tooltips);

    // show
    if (xset_get_b_panel_mode(p, "show_sidebar", mode))
        gtk_widget_show_all(file_browser->side_toolbox);
}

void
ptk_file_browser_rebuild_toolbars(PtkFileBrowser* file_browser)
{
    int i;
    for (i = 0; i < G_N_ELEMENTS(file_browser->toolbar_widgets); i++)
    {
        g_slist_free(file_browser->toolbar_widgets[i]);
        file_browser->toolbar_widgets[i] = nullptr;
    }
    if (file_browser->toolbar)
    {
        rebuild_toolbox(nullptr, file_browser);
        char* disp_path = g_filename_display_name(ptk_file_browser_get_cwd(file_browser));
        gtk_entry_set_text(GTK_ENTRY(file_browser->path_bar), disp_path);
        g_free(disp_path);
    }
    if (file_browser->side_toolbar)
        rebuild_side_toolbox(nullptr, file_browser);

    enable_toolbar(file_browser);
}

static bool
on_status_bar_button_press(GtkWidget* widget, GdkEventButton* event, PtkFileBrowser* file_browser)
{
    focus_folder_view(file_browser);
    if (event->type == GDK_BUTTON_PRESS)
    {
        if ((event_handler.win_click->s || event_handler.win_click->ob2_data) &&
            main_window_event(file_browser->main_window,
                              event_handler.win_click,
                              "evt_win_click",
                              0,
                              0,
                              "statusbar",
                              0,
                              event->button,
                              event->state,
                              true))
            return true;
        if (event->button == 2)
        {
            const char* setname[] = {"status_name", "status_path", "status_info", "status_hide"};
            int i;
            for (i = 0; i < G_N_ELEMENTS(setname); i++)
            {
                if (xset_get_b(setname[i]))
                {
                    if (i < 2)
                    {
                        GList* sel_files = ptk_file_browser_get_selected_files(file_browser);
                        if (!sel_files)
                            return true;
                        if (i == 0)
                            ptk_clipboard_copy_name(ptk_file_browser_get_cwd(file_browser),
                                                    sel_files);
                        else
                            ptk_clipboard_copy_as_text(ptk_file_browser_get_cwd(file_browser),
                                                       sel_files);
                        g_list_foreach(sel_files, (GFunc)vfs_file_info_unref, nullptr);
                        g_list_free(sel_files);
                    }
                    else if (i == 2)
                        ptk_file_browser_file_properties(file_browser, 0);
                    else if (i == 3)
                        focus_panel(nullptr, file_browser->main_window, -3);
                }
            }
            return true;
        }
    }
    return false;
}

static void
on_status_effect_change(GtkMenuItem* item, PtkFileBrowser* file_browser)
{
    set_panel_focus(nullptr, file_browser);
}

static void
on_status_middle_click_config(GtkMenuItem* menuitem, XSet* set)
{
    const char* setname[] = {"status_name", "status_path", "status_info", "status_hide"};
    int i;
    for (i = 0; i < G_N_ELEMENTS(setname); i++)
    {
        if (!strcmp(set->name, setname[i]))
            set->b = XSET_B_TRUE;
        else
            xset_set_b(setname[i], false);
    }
}

static void
on_status_bar_popup(GtkWidget* widget, GtkWidget* menu, PtkFileBrowser* file_browser)
{
    XSetContext* context = xset_context_new();
    main_context_fill(file_browser, context);
    GtkAccelGroup* accel_group = gtk_accel_group_new();
    char* desc =
        g_strdup_printf("separator status_border status_text panel%d_icon_status status_middle",
                        file_browser->mypanel);

    xset_set_cb("status_border", (GFunc)on_status_effect_change, file_browser);
    xset_set_cb("status_text", (GFunc)on_status_effect_change, file_browser);
    xset_set_cb_panel(file_browser->mypanel,
                      "icon_status",
                      (GFunc)on_status_effect_change,
                      file_browser);
    XSet* set = xset_get("status_name");
    xset_set_cb("status_name", (GFunc)on_status_middle_click_config, set);
    xset_set_ob2(set, nullptr, nullptr);
    XSet* set_radio = set;
    set = xset_get("status_path");
    xset_set_cb("status_path", (GFunc)on_status_middle_click_config, set);
    xset_set_ob2(set, nullptr, set_radio);
    set = xset_get("status_info");
    xset_set_cb("status_info", (GFunc)on_status_middle_click_config, set);
    xset_set_ob2(set, nullptr, set_radio);
    set = xset_get("status_hide");
    xset_set_cb("status_hide", (GFunc)on_status_middle_click_config, set);
    xset_set_ob2(set, nullptr, set_radio);

    xset_add_menu(file_browser, menu, accel_group, desc);
    g_free(desc);
    gtk_widget_show_all(menu);
    g_signal_connect(menu, "key-press-event", G_CALLBACK(xset_menu_keypress), nullptr);
}

static void
ptk_file_browser_init(PtkFileBrowser* file_browser)
{
    file_browser->mypanel = 0; // don't load font yet in ptk_path_entry_new
    file_browser->path_bar = ptk_path_entry_new(file_browser);
    g_signal_connect(file_browser->path_bar,
                     "activate",
                     G_CALLBACK(on_address_bar_activate),
                     file_browser);
    g_signal_connect(file_browser->path_bar,
                     "focus-in-event",
                     G_CALLBACK(on_address_bar_focus_in),
                     file_browser);

    // toolbox
    file_browser->toolbar = nullptr;
    file_browser->toolbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(file_browser), file_browser->toolbox, false, false, 0);

    // lists area
    file_browser->hpane = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    file_browser->side_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(file_browser->side_vbox, 140, -1);
    file_browser->folder_view_scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_paned_pack1(GTK_PANED(file_browser->hpane), file_browser->side_vbox, false, false);
    gtk_paned_pack2(GTK_PANED(file_browser->hpane), file_browser->folder_view_scroll, true, true);

    // fill side
    file_browser->side_toolbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    file_browser->side_toolbar = nullptr;
    file_browser->side_vpane_top = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    file_browser->side_vpane_bottom = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    file_browser->side_dir_scroll = gtk_scrolled_window_new(nullptr, nullptr);
    file_browser->side_book_scroll = gtk_scrolled_window_new(nullptr, nullptr);
    file_browser->side_dev_scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_box_pack_start(GTK_BOX(file_browser->side_vbox),
                       file_browser->side_toolbox,
                       false,
                       false,
                       0);
    gtk_box_pack_start(GTK_BOX(file_browser->side_vbox),
                       file_browser->side_vpane_top,
                       true,
                       true,
                       0);
    // see https://github.com/BwackNinja/spacefm/issues/21
    gtk_paned_pack1(GTK_PANED(file_browser->side_vpane_top),
                    file_browser->side_dev_scroll,
                    false,
                    false);
    gtk_paned_pack2(GTK_PANED(file_browser->side_vpane_top),
                    file_browser->side_vpane_bottom,
                    true,
                    false);
    gtk_paned_pack1(GTK_PANED(file_browser->side_vpane_bottom),
                    file_browser->side_book_scroll,
                    false,
                    false);
    gtk_paned_pack2(GTK_PANED(file_browser->side_vpane_bottom),
                    file_browser->side_dir_scroll,
                    true,
                    false);

    // status bar
    file_browser->status_bar = gtk_statusbar_new();

    GList* children = gtk_container_get_children(GTK_CONTAINER(file_browser->status_bar));
    file_browser->status_frame = GTK_FRAME(children->data);
    g_list_free(children);
    children = gtk_container_get_children(
        GTK_CONTAINER(gtk_statusbar_get_message_area(GTK_STATUSBAR(file_browser->status_bar))));
    file_browser->status_label = GTK_LABEL(children->data);
    g_list_free(children);
    // don't know panel yet
    file_browser->status_image = xset_get_image("gtk-yes", GTK_ICON_SIZE_MENU);
    gtk_box_pack_start(GTK_BOX(file_browser->status_bar),
                       file_browser->status_image,
                       false,
                       false,
                       0);
    // required for button event
    gtk_label_set_selectable(file_browser->status_label, true);
    gtk_widget_set_can_focus(GTK_WIDGET(file_browser->status_label), false);
    gtk_widget_set_hexpand(GTK_WIDGET(file_browser->status_label), true);
    gtk_widget_set_halign(GTK_WIDGET(file_browser->status_label), GTK_ALIGN_FILL);
    gtk_widget_set_halign(GTK_WIDGET(file_browser->status_label), GTK_ALIGN_START);
    gtk_widget_set_valign(GTK_WIDGET(file_browser->status_label), GTK_ALIGN_CENTER);

    g_signal_connect(G_OBJECT(file_browser->status_label),
                     "button-press-event",
                     G_CALLBACK(on_status_bar_button_press),
                     file_browser);
    g_signal_connect(G_OBJECT(file_browser->status_label),
                     "populate-popup",
                     G_CALLBACK(on_status_bar_popup),
                     file_browser);

    // pack fb vbox
    gtk_box_pack_start(GTK_BOX(file_browser), file_browser->hpane, true, true, 0);
    // TODO pack task frames
    gtk_box_pack_start(GTK_BOX(file_browser), file_browser->status_bar, false, false, 0);

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_browser->folder_view_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_browser->side_dir_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_browser->side_book_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_browser->side_dev_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);

    g_signal_connect(file_browser->hpane,
                     "button-release-event",
                     G_CALLBACK(ptk_file_browser_slider_release),
                     file_browser);
    g_signal_connect(file_browser->side_vpane_top,
                     "button-release-event",
                     G_CALLBACK(ptk_file_browser_slider_release),
                     file_browser);
    g_signal_connect(file_browser->side_vpane_bottom,
                     "button-release-event",
                     G_CALLBACK(ptk_file_browser_slider_release),
                     file_browser);
}

static void
ptk_file_browser_finalize(GObject* obj)
{
    int i;
    PtkFileBrowser* file_browser = PTK_FILE_BROWSER(obj);
    // printf("ptk_file_browser_finalize\n");
    if (file_browser->dir)
    {
        g_signal_handlers_disconnect_matched(file_browser->dir,
                                             G_SIGNAL_MATCH_DATA,
                                             0,
                                             0,
                                             nullptr,
                                             nullptr,
                                             file_browser);
        g_object_unref(file_browser->dir);
    }

    /* Remove all idle handlers which are not called yet. */
    do
    {
    } while (g_source_remove_by_user_data(file_browser));

    if (file_browser->file_list)
    {
        g_signal_handlers_disconnect_matched(file_browser->file_list,
                                             G_SIGNAL_MATCH_DATA,
                                             0,
                                             0,
                                             nullptr,
                                             nullptr,
                                             file_browser);
        g_object_unref(G_OBJECT(file_browser->file_list));
    }

    g_free(file_browser->status_bar_custom);
    g_free(file_browser->seek_name);
    file_browser->seek_name = nullptr;
    g_free(file_browser->book_set_name);
    file_browser->book_set_name = nullptr;
    g_free(file_browser->select_path);
    file_browser->select_path = nullptr;
    for (i = 0; i < G_N_ELEMENTS(file_browser->toolbar_widgets); i++)
    {
        g_slist_free(file_browser->toolbar_widgets[i]);
        file_browser->toolbar_widgets[i] = nullptr;
    }

    G_OBJECT_CLASS(parent_class)->finalize(obj);

    /* Ensuring free space at the end of the heap is freed to the OS,
     * mainly to deal with the possibility that killing the browser results in
     * thousands of large thumbnails being freed, but the memory not actually
     * released by SpaceFM */
#if defined(__GLIBC__)
    malloc_trim(0);
#endif
}

static void
ptk_file_browser_get_property(GObject* obj, unsigned int prop_id, GValue* value, GParamSpec* pspec)
{
}

static void
ptk_file_browser_set_property(GObject* obj, unsigned int prop_id, const GValue* value,
                              GParamSpec* pspec)
{
}

void
ptk_file_browser_update_views(GtkWidget* item, PtkFileBrowser* file_browser)
{
    // printf("ptk_file_browser_update_views fb=%p  (panel %d)\n", file_browser,
    // file_browser->mypanel );

    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;
    // hide/show browser widgets based on user settings
    int p = file_browser->mypanel;
    char mode = main_window->panel_context[p - 1];
    bool need_enable_toolbar = false;

    if (xset_get_b_panel_mode(p, "show_toolbox", mode))
    {
        if ((event_handler.pnl_show->s || event_handler.pnl_show->ob2_data) &&
            (!file_browser->toolbar || !gtk_widget_get_visible(file_browser->toolbox)))
            main_window_event(main_window,
                              event_handler.pnl_show,
                              "evt_pnl_show",
                              0,
                              0,
                              "toolbar",
                              0,
                              0,
                              0,
                              true);
        if (!file_browser->toolbar)
        {
            rebuild_toolbox(nullptr, file_browser);
            need_enable_toolbar = true;
        }
        gtk_widget_show_all(file_browser->toolbox);
    }
    else
    {
        if ((event_handler.pnl_show->s || event_handler.pnl_show->ob2_data) &&
            file_browser->toolbox && gtk_widget_get_visible(file_browser->toolbox))
            main_window_event(main_window,
                              event_handler.pnl_show,
                              "evt_pnl_show",
                              0,
                              0,
                              "toolbar",
                              0,
                              0,
                              0,
                              false);
        gtk_widget_hide(file_browser->toolbox);
    }

    if (xset_get_b_panel_mode(p, "show_sidebar", mode))
    {
        if ((event_handler.pnl_show->s || event_handler.pnl_show->ob2_data) &&
            (!file_browser->side_toolbox || !gtk_widget_get_visible(file_browser->side_toolbox)))
            main_window_event(main_window,
                              event_handler.pnl_show,
                              "evt_pnl_show",
                              0,
                              0,
                              "sidetoolbar",
                              0,
                              0,
                              0,
                              true);
        if (!file_browser->side_toolbar)
        {
            rebuild_side_toolbox(nullptr, file_browser);
            need_enable_toolbar = true;
        }
        gtk_widget_show_all(file_browser->side_toolbox);
    }
    else
    {
        if ((event_handler.pnl_show->s || event_handler.pnl_show->ob2_data) &&
            file_browser->side_toolbar && file_browser->side_toolbox &&
            gtk_widget_get_visible(file_browser->side_toolbox))
            main_window_event(main_window,
                              event_handler.pnl_show,
                              "evt_pnl_show",
                              0,
                              0,
                              "sidetoolbar",
                              0,
                              0,
                              0,
                              false);
        /*  toolboxes must be destroyed together for toolbar_widgets[]
        if ( file_browser->side_toolbar )
        {
            gtk_widget_destroy( file_browser->side_toolbar );
            file_browser->side_toolbar = nullptr;
        }
        */
        gtk_widget_hide(file_browser->side_toolbox);
    }

    if (xset_get_b_panel_mode(p, "show_dirtree", mode))
    {
        if ((event_handler.pnl_show->s || event_handler.pnl_show->ob2_data) &&
            (!file_browser->side_dir_scroll ||
             !gtk_widget_get_visible(file_browser->side_dir_scroll)))
            main_window_event(main_window,
                              event_handler.pnl_show,
                              "evt_pnl_show",
                              0,
                              0,
                              "dirtree",
                              0,
                              0,
                              0,
                              true);
        if (!file_browser->side_dir)
        {
            file_browser->side_dir = ptk_file_browser_create_dir_tree(file_browser);
            gtk_container_add(GTK_CONTAINER(file_browser->side_dir_scroll), file_browser->side_dir);
        }
        gtk_widget_show_all(file_browser->side_dir_scroll);
        if (file_browser->side_dir && file_browser->file_list)
            ptk_dir_tree_view_chdir(GTK_TREE_VIEW(file_browser->side_dir),
                                    ptk_file_browser_get_cwd(file_browser));
    }
    else
    {
        if ((event_handler.pnl_show->s || event_handler.pnl_show->ob2_data) &&
            file_browser->side_dir_scroll && gtk_widget_get_visible(file_browser->side_dir_scroll))
            main_window_event(main_window,
                              event_handler.pnl_show,
                              "evt_pnl_show",
                              0,
                              0,
                              "dirtree",
                              0,
                              0,
                              0,
                              false);
        gtk_widget_hide(file_browser->side_dir_scroll);
        if (file_browser->side_dir)
            gtk_widget_destroy(file_browser->side_dir);
        file_browser->side_dir = nullptr;
    }

    if (xset_get_b_panel_mode(p, "show_book", mode))
    {
        if ((event_handler.pnl_show->s || event_handler.pnl_show->ob2_data) &&
            (!file_browser->side_book_scroll ||
             !gtk_widget_get_visible(file_browser->side_book_scroll)))
            main_window_event(main_window,
                              event_handler.pnl_show,
                              "evt_pnl_show",
                              0,
                              0,
                              "bookmarks",
                              0,
                              0,
                              0,
                              true);
        if (!file_browser->side_book)
        {
            file_browser->side_book = ptk_bookmark_view_new(file_browser);
            gtk_container_add(GTK_CONTAINER(file_browser->side_book_scroll),
                              file_browser->side_book);
        }
        gtk_widget_show_all(file_browser->side_book_scroll);
    }
    else
    {
        if ((event_handler.pnl_show->s || event_handler.pnl_show->ob2_data) &&
            file_browser->side_book_scroll &&
            gtk_widget_get_visible(file_browser->side_book_scroll))
            main_window_event(main_window,
                              event_handler.pnl_show,
                              "evt_pnl_show",
                              0,
                              0,
                              "bookmarks",
                              0,
                              0,
                              0,
                              false);
        gtk_widget_hide(file_browser->side_book_scroll);
        if (file_browser->side_book)
            gtk_widget_destroy(file_browser->side_book);
        file_browser->side_book = nullptr;
    }

    if (xset_get_b_panel_mode(p, "show_devmon", mode))
    {
        if ((event_handler.pnl_show->s || event_handler.pnl_show->ob2_data) &&
            (!file_browser->side_dev_scroll ||
             !gtk_widget_get_visible(file_browser->side_dev_scroll)))
            main_window_event(main_window,
                              event_handler.pnl_show,
                              "evt_pnl_show",
                              0,
                              0,
                              "devices",
                              0,
                              0,
                              0,
                              true);
        if (!file_browser->side_dev)
        {
            file_browser->side_dev = ptk_location_view_new(file_browser);
            gtk_container_add(GTK_CONTAINER(file_browser->side_dev_scroll), file_browser->side_dev);
        }
        gtk_widget_show_all(file_browser->side_dev_scroll);
    }
    else
    {
        if ((event_handler.pnl_show->s || event_handler.pnl_show->ob2_data) &&
            file_browser->side_dev_scroll && gtk_widget_get_visible(file_browser->side_dev_scroll))
            main_window_event(main_window,
                              event_handler.pnl_show,
                              "evt_pnl_show",
                              0,
                              0,
                              "devices",
                              0,
                              0,
                              0,
                              false);
        gtk_widget_hide(file_browser->side_dev_scroll);
        if (file_browser->side_dev)
            gtk_widget_destroy(file_browser->side_dev);
        file_browser->side_dev = nullptr;
    }

    if (xset_get_b_panel_mode(p, "show_book", mode) ||
        xset_get_b_panel_mode(p, "show_dirtree", mode))
        gtk_widget_show(file_browser->side_vpane_bottom);
    else
        gtk_widget_hide(file_browser->side_vpane_bottom);

    if (xset_get_b_panel_mode(p, "show_devmon", mode) ||
        xset_get_b_panel_mode(p, "show_dirtree", mode) ||
        xset_get_b_panel_mode(p, "show_book", mode))
        gtk_widget_show(file_browser->side_vbox);
    else
        gtk_widget_hide(file_browser->side_vbox);

    if (need_enable_toolbar)
        enable_toolbar(file_browser);
    else
    {
        // toggle sidepane toolbar buttons
        ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSET_TOOL_DEVICES);
        ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSET_TOOL_BOOKMARKS);
        ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSET_TOOL_TREE);
    }

    // set slider positions
    int pos;
    // hpane
    pos = main_window->panel_slide_x[p - 1];
    if (pos < 100)
        pos = -1;
    // printf( "    set slide_x = %d  \n", pos );
    if (pos > 0)
        gtk_paned_set_position(GTK_PANED(file_browser->hpane), pos);

    // side_vpane_top
    pos = main_window->panel_slide_y[p - 1];
    if (pos < 20)
        pos = -1;
    // printf( "    slide_y = %d  ", pos );
    gtk_paned_set_position(GTK_PANED(file_browser->side_vpane_top), pos);

    // side_vpane_bottom
    pos = main_window->panel_slide_s[p - 1];
    if (pos < 20)
        pos = -1;
    // printf( "slide_s = %d\n", pos );
    gtk_paned_set_position(GTK_PANED(file_browser->side_vpane_bottom), pos);

    // Large Icons - option for Detailed and Compact list views
    bool large_icons =
        xset_get_b_panel(p, "list_icons") || xset_get_b_panel_mode(p, "list_large", mode);
    if (large_icons != !!file_browser->large_icons)
    {
        if (file_browser->folder_view)
        {
            // force rebuild of folder_view for icon size change
            gtk_widget_destroy(file_browser->folder_view);
            file_browser->folder_view = nullptr;
        }
        file_browser->large_icons = large_icons;
        ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSET_TOOL_LARGE_ICONS);
    }

    // List Styles
    if (xset_get_b_panel(p, "list_detailed"))
    {
        ptk_file_browser_view_as_list(file_browser);

        // Set column widths for this panel context
        GtkTreeViewColumn* col;
        int j, width;
        const char* title;
        XSet* set;

        if (GTK_IS_TREE_VIEW(file_browser->folder_view))
        {
            // printf("    set widths   mode = %d\n", mode);
            int i;
            for (i = 0; i < 6; i++)
            {
                col = gtk_tree_view_get_column(GTK_TREE_VIEW(file_browser->folder_view), i);
                if (!col)
                    break;
                title = gtk_tree_view_column_get_title(col);
                for (j = 0; j < 6; j++)
                {
                    if (!strcmp(title, column_titles[j]))
                        break;
                }
                if (j != 6)
                {
                    // get column width for this panel context
                    set = xset_get_panel_mode(p, column_names[j], mode);
                    width = set->y ? strtol(set->y, nullptr, 10) : 100;
                    // printf("        %d\t%s\n", width, title );
                    if (width)
                    {
                        gtk_tree_view_column_set_fixed_width(col, width);
                        // printf("upd set_width %s %d\n", column_names[j], width );
                    }
                    // set column visibility
                    gtk_tree_view_column_set_visible(col, set->b == XSET_B_TRUE || j == 0);
                }
            }
        }
    }
    else if (xset_get_b_panel(p, "list_icons"))
        ptk_file_browser_view_as_icons(file_browser);
    else if (xset_get_b_panel(p, "list_compact"))
        ptk_file_browser_view_as_compact_list(file_browser);
    else
    {
        xset_set_panel(p, "list_detailed", "b", "1");
        ptk_file_browser_view_as_list(file_browser);
    }

    // Show Hidden
    ptk_file_browser_show_hidden_files(file_browser, xset_get_b_panel(p, "show_hidden"));

    // printf("ptk_file_browser_update_views fb=%p DONE\n", file_browser);
}

GtkWidget*
ptk_file_browser_new(int curpanel, GtkWidget* notebook, GtkWidget* task_view, void* main_window)
{
    int i;
    PtkFBViewMode view_mode;
    PangoFontDescription* font_desc;
    PtkFileBrowser* file_browser = (PtkFileBrowser*)g_object_new(PTK_TYPE_FILE_BROWSER, nullptr);

    file_browser->mypanel = curpanel;
    file_browser->mynotebook = notebook;
    file_browser->main_window = main_window;
    file_browser->task_view = task_view;
    file_browser->sel_change_idle = 0;
    file_browser->inhibit_focus = file_browser->busy = false;
    file_browser->seek_name = nullptr;
    file_browser->book_set_name = nullptr;
    for (i = 0; i < G_N_ELEMENTS(file_browser->toolbar_widgets); i++)
        file_browser->toolbar_widgets[i] = nullptr;

    if (xset_get_b_panel(curpanel, "list_detailed"))
        view_mode = PTK_FB_LIST_VIEW;
    else if (xset_get_b_panel(curpanel, "list_icons"))
    {
        view_mode = PTK_FB_ICON_VIEW;
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_browser->folder_view_scroll),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
    }
    else if (xset_get_b_panel(curpanel, "list_compact"))
    {
        view_mode = PTK_FB_COMPACT_VIEW;
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_browser->folder_view_scroll),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
    }
    else
    {
        xset_set_panel(curpanel, "list_detailed", "b", "1");
        view_mode = PTK_FB_LIST_VIEW;
    }

    file_browser->view_mode = view_mode; // sfm was after next line
    // Large Icons - option for Detailed and Compact list views
    file_browser->large_icons =
        view_mode == PTK_FB_ICON_VIEW ||
        xset_get_b_panel_mode(
            file_browser->mypanel,
            "list_large",
            ((FMMainWindow*)main_window)->panel_context[file_browser->mypanel - 1]);
    file_browser->folder_view = create_folder_view(file_browser, view_mode);

    gtk_container_add(GTK_CONTAINER(file_browser->folder_view_scroll), file_browser->folder_view);

    file_browser->side_dir = nullptr;
    file_browser->side_book = nullptr;
    file_browser->side_dev = nullptr;

    file_browser->select_path = nullptr;
    file_browser->status_bar_custom = nullptr;

    // gtk_widget_show_all( file_browser->folder_view_scroll );

    // set status bar icon
    char* icon_name;
    XSet* set = xset_get_panel(curpanel, "icon_status");
    if (set->icon && set->icon[0] != '\0')
        icon_name = set->icon;
    else
        icon_name = g_strdup("gtk-yes");
    gtk_image_set_from_icon_name(GTK_IMAGE(file_browser->status_image),
                                 icon_name,
                                 GTK_ICON_SIZE_MENU);

    gtk_widget_show_all(GTK_WIDGET(file_browser));

    return GTK_IS_WIDGET(file_browser) ? (GtkWidget*)file_browser : nullptr;
}

static void
ptk_file_browser_update_tab_label(PtkFileBrowser* file_browser)
{
    GtkWidget* label;
    GtkContainer* hbox;
    // GtkImage* icon;
    GtkLabel* text;
    GList* children;
    char* name;

    label = gtk_notebook_get_tab_label(GTK_NOTEBOOK(file_browser->mynotebook),
                                       GTK_WIDGET(file_browser));
    hbox = GTK_CONTAINER(gtk_bin_get_child(GTK_BIN(label)));
    children = gtk_container_get_children(hbox);
    // icon = GTK_IMAGE(children->data);
    text = GTK_LABEL(children->next->data);
    g_list_free(children);

    /* TODO: Change the icon */

    name = g_path_get_basename(ptk_file_browser_get_cwd(file_browser));
    gtk_label_set_text(text, name);
    gtk_label_set_ellipsize(text, PANGO_ELLIPSIZE_MIDDLE);
    if (strlen(name) < 30)
    {
        gtk_label_set_ellipsize(text, PANGO_ELLIPSIZE_NONE);
        gtk_label_set_width_chars(text, -1);
    }
    else
        gtk_label_set_width_chars(text, 30);
    g_free(name);
}

void
ptk_file_browser_select_last(PtkFileBrowser* file_browser) // MOD added
{
    // printf("ptk_file_browser_select_last\n");
    // select one file?
    if (file_browser->select_path)
    {
        ptk_file_browser_select_file(file_browser, file_browser->select_path);
        g_free(file_browser->select_path);
        file_browser->select_path = nullptr;
        return;
    }

    // select previously selected files
    int elementn = -1;
    GList* l;
    GList* element = nullptr;
    // printf("    search for %s\n", (char*)file_browser->curHistory->data );

    if (file_browser->history && file_browser->histsel && file_browser->curHistory &&
        (l = g_list_last(file_browser->history)))
    {
        if (l->data && !strcmp((char*)l->data, (char*)file_browser->curHistory->data))
        {
            elementn = g_list_position(file_browser->history, l);
            if (elementn != -1)
            {
                element = g_list_nth(file_browser->histsel, elementn);
                // skip the current history item if sellist empty since it was just created
                if (!element->data)
                {
                    // printf( "        found current empty\n");
                    element = nullptr;
                }
                // else printf( "        found current NON-empty\n");
            }
        }
        if (!element)
        {
            while ((l = l->prev))
            {
                if (l->data && !strcmp((char*)l->data, (char*)file_browser->curHistory->data))
                {
                    elementn = g_list_position(file_browser->history, l);
                    // printf ("        found elementn=%d\n", elementn );
                    if (elementn != -1)
                        element = g_list_nth(file_browser->histsel, elementn);
                    break;
                }
            }
        }
    }

    /*
        if ( element )
        {
            g_debug ("element OK" );
            if ( element->data )
                g_debug ("element->data OK" );
            else
                g_debug ("element->data nullptr" );
        }
        else
            g_debug ("element nullptr" );
        g_debug ("histsellen=%d", g_list_length( file_browser->histsel ) );
    */
    if (element && element->data)
    {
        // printf("    select files\n");
        PtkFileList* list = PTK_FILE_LIST(file_browser->file_list);
        GtkTreeSelection* tree_sel;
        bool firstsel = true;
        if (file_browser->view_mode == PTK_FB_LIST_VIEW)
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
        for (l = (GList*)element->data; l; l = l->next)
        {
            if (l->data)
            {
                // g_debug ("find a file");
                GtkTreeIter it;
                GtkTreePath* tp;
                VFSFileInfo* file = (VFSFileInfo*)l->data;
                if (ptk_file_list_find_iter(list, &it, file))
                {
                    // g_debug ("found file");
                    tp = gtk_tree_model_get_path(GTK_TREE_MODEL(list), &it);
                    if (file_browser->view_mode == PTK_FB_ICON_VIEW ||
                        file_browser->view_mode == PTK_FB_COMPACT_VIEW)
                    {
                        exo_icon_view_select_path(EXO_ICON_VIEW(file_browser->folder_view), tp);
                        if (firstsel)
                        {
                            exo_icon_view_set_cursor(EXO_ICON_VIEW(file_browser->folder_view),
                                                     tp,
                                                     nullptr,
                                                     false);
                            exo_icon_view_scroll_to_path(EXO_ICON_VIEW(file_browser->folder_view),
                                                         tp,
                                                         true,
                                                         .25,
                                                         0);
                            firstsel = false;
                        }
                    }
                    else if (file_browser->view_mode == PTK_FB_LIST_VIEW)
                    {
                        gtk_tree_selection_select_path(tree_sel, tp);
                        if (firstsel)
                        {
                            gtk_tree_view_set_cursor(GTK_TREE_VIEW(file_browser->folder_view),
                                                     tp,
                                                     nullptr,
                                                     false);
                            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(file_browser->folder_view),
                                                         tp,
                                                         nullptr,
                                                         true,
                                                         .25,
                                                         0);
                            firstsel = false;
                        }
                    }
                    gtk_tree_path_free(tp);
                }
            }
        }
    }
}

bool
ptk_file_browser_chdir(PtkFileBrowser* file_browser, const char* folder_path, PtkFBChdirMode mode)
{
    bool cancel = false;
    GtkWidget* folder_view = file_browser->folder_view;
    // printf("ptk_file_browser_chdir\n");
    char* path;
    char* msg;

    bool inhibit_focus = file_browser->inhibit_focus;
    // file_browser->button_press = false;
    file_browser->is_drag = false;
    file_browser->menu_shown = false;
    if (file_browser->view_mode == PTK_FB_LIST_VIEW || app_settings.single_click)
        /* sfm 1.0.6 don't reset skip_release for Icon/Compact to prevent file
           under cursor being selected when entering dir with double-click.
           Reset is conditional here to avoid possible but unlikely unintended
           breakage elsewhere. */
        file_browser->skip_release = false;

    if (!folder_path)
        return false;

    if (folder_path)
    {
        path = g_strdup(folder_path);
        /* remove redundent '/' */
        if (strcmp(path, "/"))
        {
            char* path_end = path + strlen(path) - 1;
            for (; path_end > path; --path_end)
            {
                if (*path_end != '/')
                    break;
                else
                    *path_end = '\0';
            }
        }

        // convert ~ to /home/user for smarter bookmarks
        if (g_str_has_prefix(path, "~/") || !g_strcmp0(path, "~"))
        {
            msg = g_strdup_printf("%s%s", g_get_home_dir(), path + 1);
            g_free(path);
            path = msg;
        }
    }
    else
        path = nullptr;

    if (!path || !g_file_test(path, (G_FILE_TEST_IS_DIR)))
    {
        if (!inhibit_focus)
        {
            msg = g_strdup_printf("Directory doesn't exist\n\n%s", path);
            ptk_show_error(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(file_browser))),
                           "Error",
                           msg);
            if (path)
                g_free(path);
            g_free(msg);
        }
        return false;
    }

    if (!have_x_access(path))
    {
        if (!inhibit_focus)
        {
            msg = g_strdup_printf("Unable to access %s\n\n%s",
                                  path,
                                  g_markup_escape_text(g_strerror(errno), -1));
            ptk_show_error(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(file_browser))),
                           "Error",
                           msg);
            g_free(msg);
        }
        return false;
    }

    g_signal_emit(file_browser, signals[BEFORE_CHDIR_SIGNAL], 0, path, &cancel);

    if (cancel)
        return false;

    // MOD remember selected files
    // g_debug ("@@@@@@@@@@@ remember: %s", ptk_file_browser_get_cwd( file_browser ) );
    if (file_browser->curhistsel && file_browser->curhistsel->data)
    {
        // g_debug ("free curhistsel");
        g_list_foreach((GList*)file_browser->curhistsel->data, (GFunc)vfs_file_info_unref, nullptr);
        g_list_free((GList*)file_browser->curhistsel->data);
    }
    if (file_browser->curhistsel)
    {
        file_browser->curhistsel->data = ptk_file_browser_get_selected_files(file_browser);

        // g_debug("set curhistsel %d", g_list_position( file_browser->histsel,
        //                                    file_browser->curhistsel ) );
        // if ( file_browser->curhistsel->data )
        //    g_debug ("curhistsel->data OK" );
        // else
        //    g_debug ("curhistsel->data nullptr" );
    }

    switch (mode)
    {
        case PTK_FB_CHDIR_ADD_HISTORY:
            if (!file_browser->curHistory || strcmp((char*)file_browser->curHistory->data, path))
            {
                /* Has forward history */
                if (file_browser->curHistory && file_browser->curHistory->next)
                {
                    /* clear old forward history */
                    g_list_foreach(file_browser->curHistory->next, (GFunc)g_free, nullptr);
                    g_list_free(file_browser->curHistory->next);
                    file_browser->curHistory->next = nullptr;
                }
                // MOD added - make histsel shadow file_browser->history
                if (file_browser->curhistsel && file_browser->curhistsel->next)
                {
                    // g_debug("@@@@@@@@@@@ free forward");
                    GList* l;
                    for (l = file_browser->curhistsel->next; l; l = l->next)
                    {
                        if (l->data)
                        {
                            // g_debug("free forward item");
                            g_list_foreach((GList*)l->data, (GFunc)vfs_file_info_unref, nullptr);
                            g_list_free((GList*)l->data);
                        }
                    }
                    g_list_free(file_browser->curhistsel->next);
                    file_browser->curhistsel->next = nullptr;
                }
                /* Add path to history if there is no forward history */
                file_browser->history = g_list_append(file_browser->history, path);
                file_browser->curHistory = g_list_last(file_browser->history);
                // MOD added - make histsel shadow file_browser->history
                GList* sellist = nullptr;
                file_browser->histsel = g_list_append(file_browser->histsel, sellist);
                file_browser->curhistsel = g_list_last(file_browser->histsel);
            }
            break;
        case PTK_FB_CHDIR_BACK:
            file_browser->curHistory = file_browser->curHistory->prev;
            file_browser->curhistsel = file_browser->curhistsel->prev;
            break;
        case PTK_FB_CHDIR_FORWARD:
            file_browser->curHistory = file_browser->curHistory->next;
            file_browser->curhistsel = file_browser->curhistsel->next;
            break;
        case PTK_FB_CHDIR_NORMAL:
        case PTK_FB_CHDIR_NO_HISTORY:
        default:
            break;
    }

    // remove old dir object
    if (file_browser->dir)
    {
        g_signal_handlers_disconnect_matched(file_browser->dir,
                                             G_SIGNAL_MATCH_DATA,
                                             0,
                                             0,
                                             nullptr,
                                             nullptr,
                                             file_browser);
        g_object_unref(file_browser->dir);
    }

    switch (file_browser->view_mode)
    {
        case PTK_FB_ICON_VIEW:
        case PTK_FB_COMPACT_VIEW:
            exo_icon_view_set_model(EXO_ICON_VIEW(folder_view), nullptr);
            break;
        case PTK_FB_LIST_VIEW:
            gtk_tree_view_set_model(GTK_TREE_VIEW(folder_view), nullptr);
            break;
        default:
            break;
    }

    // load new dir
    file_browser->busy = true;
    file_browser->dir = vfs_dir_get_by_path(path);

    if (!file_browser->curHistory || path != (char*)file_browser->curHistory->data)
        g_free(path);

    g_signal_emit(file_browser, signals[BEGIN_CHDIR_SIGNAL], 0);

    if (vfs_dir_is_file_listed(file_browser->dir))
    {
        on_dir_file_listed(file_browser->dir, false, file_browser);
        file_browser->busy = false;
    }
    else
        file_browser->busy = true;

    g_signal_connect(file_browser->dir,
                     "file-listed",
                     G_CALLBACK(on_dir_file_listed),
                     file_browser);

    ptk_file_browser_update_tab_label(file_browser);

    char* disp_path = g_filename_display_name(ptk_file_browser_get_cwd(file_browser));
    if (!inhibit_focus)
        gtk_entry_set_text(GTK_ENTRY(file_browser->path_bar), disp_path);

    g_free(disp_path);

    enable_toolbar(file_browser);
    return true;
}

static void
on_history_menu_item_activate(GtkWidget* menu_item, PtkFileBrowser* file_browser)
{
    GList* l = (GList*)g_object_get_data(G_OBJECT(menu_item), "path");
    GList* tmp = file_browser->curHistory;
    file_browser->curHistory = l;

    if (!ptk_file_browser_chdir(file_browser, (char*)l->data, PTK_FB_CHDIR_NO_HISTORY))
        file_browser->curHistory = tmp;
    else
    {
        // MOD sync curhistsel
        int elementn = -1;
        elementn = g_list_position(file_browser->history, file_browser->curHistory);
        if (elementn != -1)
            file_browser->curhistsel = g_list_nth(file_browser->histsel, elementn);
        else
            g_debug("missing history item - ptk-file-browser.c");
    }
}

static GtkWidget*
add_history_menu_item(PtkFileBrowser* file_browser, GtkWidget* menu, GList* l)
{
    GtkWidget* menu_item;
    // GtkWidget* folder_image;
    char* disp_name;
    disp_name = g_filename_display_basename((char*)l->data);
    menu_item = gtk_menu_item_new_with_label(disp_name);
    g_object_set_data(G_OBJECT(menu_item), "path", l);
    // folder_image = gtk_image_new_from_icon_name("gnome-fs-directory", GTK_ICON_SIZE_MENU);
    g_signal_connect(menu_item,
                     "activate",
                     G_CALLBACK(on_history_menu_item_activate),
                     file_browser);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    return menu_item;
}

void
ptk_file_browser_show_history_menu(PtkFileBrowser* file_browser, bool is_back_history,
                                   GdkEventButton* event)
{
    GtkWidget* menu = gtk_menu_new();
    GList* l;
    bool has_items = false;

    if (is_back_history)
    {
        // back history
        for (l = file_browser->curHistory->prev; l != nullptr; l = l->prev)
        {
            add_history_menu_item(file_browser, GTK_WIDGET(menu), l);
            if (!has_items)
                has_items = true;
        }
    }
    else
    {
        // forward history
        for (l = file_browser->curHistory->next; l != nullptr; l = l->next)
        {
            add_history_menu_item(file_browser, GTK_WIDGET(menu), l);
            if (!has_items)
                has_items = true;
        }
    }
    if (has_items)
    {
        gtk_widget_show_all(GTK_WIDGET(menu));
        gtk_menu_popup_at_pointer(GTK_MENU(menu), nullptr);
    }
    else
        gtk_widget_destroy(menu);
}

static bool
ptk_file_browser_content_changed(PtkFileBrowser* file_browser)
{
    g_signal_emit(file_browser, signals[CONTENT_CHANGE_SIGNAL], 0);
    return false;
}

static void
on_folder_content_changed(VFSDir* dir, VFSFileInfo* file, PtkFileBrowser* file_browser)
{
    if (file == nullptr)
    {
        // The current directory itself changed
        if (!g_file_test(ptk_file_browser_get_cwd(file_browser), G_FILE_TEST_IS_DIR))
            // current directory doesn't exist - was renamed
            on_close_notebook_page(nullptr, file_browser);
    }
    else
        g_idle_add((GSourceFunc)ptk_file_browser_content_changed, file_browser);
}

static void
on_file_deleted(VFSDir* dir, VFSFileInfo* file, PtkFileBrowser* file_browser)
{
    if (file == nullptr)
    {
        // The directory itself was deleted
        on_close_notebook_page(nullptr, file_browser);
        // ptk_file_browser_chdir( file_browser, g_get_home_dir(), PTK_FB_CHDIR_ADD_HISTORY);
    }
    else
    {
        on_folder_content_changed(dir, file, file_browser);
    }
}

static void
on_sort_col_changed(GtkTreeSortable* sortable, PtkFileBrowser* file_browser)
{
    int col;

    gtk_tree_sortable_get_sort_column_id(sortable, &col, &file_browser->sort_type);

    switch (col)
    {
        case COL_FILE_NAME:
            col = PTK_FB_SORT_BY_NAME;
            break;
        case COL_FILE_SIZE:
            col = PTK_FB_SORT_BY_SIZE;
            break;
        case COL_FILE_MTIME:
            col = PTK_FB_SORT_BY_MTIME;
            break;
        case COL_FILE_DESC:
            col = PTK_FB_SORT_BY_TYPE;
            break;
        case COL_FILE_PERM:
            col = PTK_FB_SORT_BY_PERM;
            break;
        case COL_FILE_OWNER:
            col = PTK_FB_SORT_BY_OWNER;
            break;
        default:
            break;
    }
    file_browser->sort_order = (PtkFBSortOrder)col;
    // MOD enable following to make column click permanent sort
    //    app_settings.sort_order = col;
    //    if ( file_browser )
    //        ptk_file_browser_set_sort_order( PTK_FILE_BROWSER( file_browser ),
    //        app_settings.sort_order );

    char* val = g_strdup_printf("%d", col);
    xset_set_panel(file_browser->mypanel, "list_detailed", "x", val);
    g_free(val);
    val = g_strdup_printf("%d", file_browser->sort_type);
    xset_set_panel(file_browser->mypanel, "list_detailed", "y", val);
    g_free(val);
}

static void
ptk_file_browser_update_model(PtkFileBrowser* file_browser)
{
    PtkFileList* list = ptk_file_list_new(file_browser->dir, file_browser->show_hidden_files);
    GtkTreeModel* old_list = file_browser->file_list;
    file_browser->file_list = GTK_TREE_MODEL(list);
    if (old_list)
        g_object_unref(G_OBJECT(old_list));

    ptk_file_browser_read_sort_extra(file_browser); // sfm
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(list),
                                         file_list_order_from_sort_order(file_browser->sort_order),
                                         file_browser->sort_type);

    show_thumbnails(file_browser, list, file_browser->large_icons, file_browser->max_thumbnail);
    g_signal_connect(list, "sort-column-changed", G_CALLBACK(on_sort_col_changed), file_browser);

    switch (file_browser->view_mode)
    {
        case PTK_FB_ICON_VIEW:
        case PTK_FB_COMPACT_VIEW:
            exo_icon_view_set_model(EXO_ICON_VIEW(file_browser->folder_view), GTK_TREE_MODEL(list));
            break;
        case PTK_FB_LIST_VIEW:
            gtk_tree_view_set_model(GTK_TREE_VIEW(file_browser->folder_view), GTK_TREE_MODEL(list));
            break;
        default:
            break;
    }

    // try to smooth list bounce created by delayed re-appearance of column headers
    // while( gtk_events_pending() )
    //    gtk_main_iteration();
}

static void
on_dir_file_listed(VFSDir* dir, bool is_cancelled, PtkFileBrowser* file_browser)
{
    file_browser->n_sel_files = 0;

    if (G_LIKELY(!is_cancelled))
    {
        g_signal_connect(dir, "file-created", G_CALLBACK(on_folder_content_changed), file_browser);
        g_signal_connect(dir, "file-deleted", G_CALLBACK(on_file_deleted), file_browser);
        g_signal_connect(dir, "file-changed", G_CALLBACK(on_folder_content_changed), file_browser);
    }

    ptk_file_browser_update_model(file_browser);
    file_browser->busy = false;

    /* Ensuring free space at the end of the heap is freed to the OS,
     * mainly to deal with the possibility that changing the directory results in
     * thousands of large thumbnails being freed, but the memory not actually
     * released by SpaceFM */
#if defined(__GLIBC__)
    malloc_trim(0);
#endif

    g_signal_emit(file_browser, signals[AFTER_CHDIR_SIGNAL], 0);
    // g_signal_emit( file_browser, signals[ CONTENT_CHANGE_SIGNAL ], 0 );
    g_signal_emit(file_browser, signals[SEL_CHANGE_SIGNAL], 0);

    if (file_browser->side_dir)
        ptk_dir_tree_view_chdir(GTK_TREE_VIEW(file_browser->side_dir),
                                ptk_file_browser_get_cwd(file_browser));

    /*
        if ( file_browser->side_pane )
        if ( ptk_file_browser_is_side_pane_visible( file_browser ) )
        {
            side_pane_chdir( file_browser,
                             ptk_file_browser_get_cwd( file_browser ) );
        }
    */
    if (file_browser->side_dev)
        ptk_location_view_chdir(GTK_TREE_VIEW(file_browser->side_dev),
                                ptk_file_browser_get_cwd(file_browser));
    if (file_browser->side_book)
        ptk_bookmark_view_chdir(GTK_TREE_VIEW(file_browser->side_book), file_browser, true);

    // FIXME:  This is already done in update_model, but is there any better way to
    //            reduce unnecessary code?
    if (file_browser->view_mode == PTK_FB_COMPACT_VIEW)
    { // sfm why is this needed for compact view???
        if (G_LIKELY(!is_cancelled) && file_browser->file_list)
        {
            show_thumbnails(file_browser,
                            PTK_FILE_LIST(file_browser->file_list),
                            file_browser->large_icons,
                            file_browser->max_thumbnail);
        }
    }
}

void
ptk_file_browser_canon(PtkFileBrowser* file_browser, const char* path)
{
    const char* cwd = ptk_file_browser_get_cwd(file_browser);
    char buf[PATH_MAX + 1];
    char* canon = realpath(path, buf);
    if (!canon || !g_strcmp0(canon, cwd) || !g_strcmp0(canon, path))
        return;

    if (g_file_test(canon, G_FILE_TEST_IS_DIR))
    {
        // open dir
        ptk_file_browser_chdir(file_browser, canon, PTK_FB_CHDIR_ADD_HISTORY);
        gtk_widget_grab_focus(GTK_WIDGET(file_browser->folder_view));
    }
    else if (g_file_test(canon, G_FILE_TEST_EXISTS))
    {
        // open dir and select file
        char* dir_path = g_path_get_dirname(canon);
        if (dir_path && strcmp(dir_path, cwd))
        {
            g_free(file_browser->select_path);
            file_browser->select_path = g_strdup(canon);
            ptk_file_browser_chdir(file_browser, dir_path, PTK_FB_CHDIR_ADD_HISTORY);
        }
        else
            ptk_file_browser_select_file(file_browser, canon);
        g_free(dir_path);
        gtk_widget_grab_focus(GTK_WIDGET(file_browser->folder_view));
    }
}

const char*
ptk_file_browser_get_cwd(PtkFileBrowser* file_browser)
{
    if (!file_browser->curHistory)
        return nullptr;
    return (const char*)file_browser->curHistory->data;
}

void
ptk_file_browser_go_back(GtkWidget* item, PtkFileBrowser* file_browser)
{
    focus_folder_view(file_browser);
    /* there is no back history */
    if (!file_browser->curHistory || !file_browser->curHistory->prev)
        return;
    const char* path = (const char*)file_browser->curHistory->prev->data;
    ptk_file_browser_chdir(file_browser, path, PTK_FB_CHDIR_BACK);
}

void
ptk_file_browser_go_forward(GtkWidget* item, PtkFileBrowser* file_browser)
{
    focus_folder_view(file_browser);
    /* If there is no forward history */
    if (!file_browser->curHistory || !file_browser->curHistory->next)
        return;
    const char* path = (const char*)file_browser->curHistory->next->data;
    ptk_file_browser_chdir(file_browser, path, PTK_FB_CHDIR_FORWARD);
}

void
ptk_file_browser_go_up(GtkWidget* item, PtkFileBrowser* file_browser)
{
    focus_folder_view(file_browser);
    char* parent_dir = g_path_get_dirname(ptk_file_browser_get_cwd(file_browser));
    if (strcmp(parent_dir, ptk_file_browser_get_cwd(file_browser)))
        ptk_file_browser_chdir(file_browser, parent_dir, PTK_FB_CHDIR_ADD_HISTORY);
    g_free(parent_dir);
}

void
ptk_file_browser_go_home(GtkWidget* item, PtkFileBrowser* file_browser)
{
    focus_folder_view(file_browser);
    ptk_file_browser_chdir(PTK_FILE_BROWSER(file_browser),
                           g_get_home_dir(),
                           PTK_FB_CHDIR_ADD_HISTORY);
}

void
ptk_file_browser_go_default(GtkWidget* item, PtkFileBrowser* file_browser)
{
    focus_folder_view(file_browser);
    const char* path = xset_get_s("go_set_default");
    if (path && path[0] != '\0')
        ptk_file_browser_chdir(PTK_FILE_BROWSER(file_browser), path, PTK_FB_CHDIR_ADD_HISTORY);
    else if (geteuid() != 0)
        ptk_file_browser_chdir(PTK_FILE_BROWSER(file_browser),
                               g_get_home_dir(),
                               PTK_FB_CHDIR_ADD_HISTORY);
    else
        ptk_file_browser_chdir(PTK_FILE_BROWSER(file_browser), "/", PTK_FB_CHDIR_ADD_HISTORY);
}

void
ptk_file_browser_set_default_folder(GtkWidget* item, PtkFileBrowser* file_browser)
{
    xset_set("go_set_default", "s", ptk_file_browser_get_cwd(file_browser));
}

void
ptk_file_browser_select_all(GtkWidget* item, PtkFileBrowser* file_browser)
{
    GtkTreeSelection* tree_sel;

    switch (file_browser->view_mode)
    {
        case PTK_FB_ICON_VIEW:
        case PTK_FB_COMPACT_VIEW:
            exo_icon_view_select_all(EXO_ICON_VIEW(file_browser->folder_view));
            break;
        case PTK_FB_LIST_VIEW:
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
            gtk_tree_selection_select_all(tree_sel);
            break;
        default:
            break;
    }
}

void
ptk_file_browser_unselect_all(GtkWidget* item, PtkFileBrowser* file_browser)
{
    GtkTreeSelection* tree_sel;

    switch (file_browser->view_mode)
    {
        case PTK_FB_ICON_VIEW:
        case PTK_FB_COMPACT_VIEW:
            exo_icon_view_unselect_all(EXO_ICON_VIEW(file_browser->folder_view));
            break;
        case PTK_FB_LIST_VIEW:
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
            gtk_tree_selection_unselect_all(tree_sel);
            break;
        default:
            break;
    }
}

static bool
invert_selection(GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* it,
                 PtkFileBrowser* file_browser)
{
    GtkTreeSelection* tree_sel;

    switch (file_browser->view_mode)
    {
        case PTK_FB_ICON_VIEW:
        case PTK_FB_COMPACT_VIEW:
            if (exo_icon_view_path_is_selected(EXO_ICON_VIEW(file_browser->folder_view), path))
                exo_icon_view_unselect_path(EXO_ICON_VIEW(file_browser->folder_view), path);
            else
                exo_icon_view_select_path(EXO_ICON_VIEW(file_browser->folder_view), path);
            break;
        case PTK_FB_LIST_VIEW:
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
            if (gtk_tree_selection_path_is_selected(tree_sel, path))
                gtk_tree_selection_unselect_path(tree_sel, path);
            else
                gtk_tree_selection_select_path(tree_sel, path);
            break;
        default:
            break;
    }
    return false;
}

void
ptk_file_browser_invert_selection(GtkWidget* item, PtkFileBrowser* file_browser)
{
    GtkTreeModel* model;
    GtkTreeSelection* tree_sel;

    switch (file_browser->view_mode)
    {
        case PTK_FB_ICON_VIEW:
        case PTK_FB_COMPACT_VIEW:
            model = exo_icon_view_get_model(EXO_ICON_VIEW(file_browser->folder_view));
            g_signal_handlers_block_matched(file_browser->folder_view,
                                            G_SIGNAL_MATCH_FUNC,
                                            0,
                                            0,
                                            nullptr,
                                            (void*)on_folder_view_item_sel_change,
                                            nullptr);
            gtk_tree_model_foreach(model, (GtkTreeModelForeachFunc)invert_selection, file_browser);
            g_signal_handlers_unblock_matched(file_browser->folder_view,
                                              G_SIGNAL_MATCH_FUNC,
                                              0,
                                              0,
                                              nullptr,
                                              (void*)on_folder_view_item_sel_change,
                                              nullptr);
            on_folder_view_item_sel_change(EXO_ICON_VIEW(file_browser->folder_view), file_browser);
            break;
        case PTK_FB_LIST_VIEW:
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
            g_signal_handlers_block_matched(tree_sel,
                                            G_SIGNAL_MATCH_FUNC,
                                            0,
                                            0,
                                            nullptr,
                                            (void*)on_folder_view_item_sel_change,
                                            nullptr);
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_browser->folder_view));
            gtk_tree_model_foreach(model, (GtkTreeModelForeachFunc)invert_selection, file_browser);
            g_signal_handlers_unblock_matched(tree_sel,
                                              G_SIGNAL_MATCH_FUNC,
                                              0,
                                              0,
                                              nullptr,
                                              (void*)on_folder_view_item_sel_change,
                                              nullptr);
            on_folder_view_item_sel_change((ExoIconView*)tree_sel, file_browser);
            break;
        default:
            break;
    }
}

void
ptk_file_browser_select_pattern(GtkWidget* item, PtkFileBrowser* file_browser,
                                const char* search_key)
{
    GtkTreeModel* model;
    GtkTreePath* path;
    GtkTreeIter it;
    GtkTreeSelection* tree_sel;
    VFSFileInfo* file;
    const char* key;

    if (search_key)
        key = search_key;
    else
    {
        // get pattern from user  (store in ob1 so it's not saved)
        XSet* set = xset_get("select_patt");
        if (!xset_text_dialog(
                GTK_WIDGET(file_browser),
                "Select By Pattern",
                false,
                "Enter pattern to select files and directories:\n\nIf your pattern contains any "
                "uppercase characters, the matching will be case sensitive.\n\nExample:  "
                "*sp*e?m*\n\nTIP: You can also enter '%% PATTERN' in the path bar.",
                nullptr,
                set->ob1,
                &set->ob1,
                nullptr,
                false,
                nullptr) ||
            !set->ob1)
            return;
        key = set->ob1;
    }

    // case insensitive search ?
    bool icase = false;
    char* lower_key = g_utf8_strdown(key, -1);
    if (!strcmp(lower_key, key))
    {
        // key is all lowercase so do icase search
        icase = true;
    }
    g_free(lower_key);

    // get model, treesel, and stop signals
    switch (file_browser->view_mode)
    {
        case PTK_FB_ICON_VIEW:
        case PTK_FB_COMPACT_VIEW:
            model = exo_icon_view_get_model(EXO_ICON_VIEW(file_browser->folder_view));
            g_signal_handlers_block_matched(file_browser->folder_view,
                                            G_SIGNAL_MATCH_FUNC,
                                            0,
                                            0,
                                            nullptr,
                                            (void*)on_folder_view_item_sel_change,
                                            nullptr);
            break;
        case PTK_FB_LIST_VIEW:
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
            g_signal_handlers_block_matched(tree_sel,
                                            G_SIGNAL_MATCH_FUNC,
                                            0,
                                            0,
                                            nullptr,
                                            (void*)on_folder_view_item_sel_change,
                                            nullptr);
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_browser->folder_view));
            break;
        default:
            break;
    }

    // test rows
    bool first_select = true;
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            // get file
            gtk_tree_model_get(model, &it, COL_FILE_INFO, &file, -1);
            if (!file)
                continue;

            // test name
            char* name = (char*)vfs_file_info_get_disp_name(file);
            if (icase)
                name = g_utf8_strdown(name, -1);

            bool select = fnmatch(key, name, 0) == 0;

            if (icase)
                g_free(name);

            // do selection and scroll to first selected
            path = gtk_tree_model_get_path(GTK_TREE_MODEL(PTK_FILE_LIST(file_browser->file_list)),
                                           &it);

            switch (file_browser->view_mode)
            {
                case PTK_FB_ICON_VIEW:
                case PTK_FB_COMPACT_VIEW:
                    // select
                    if (exo_icon_view_path_is_selected(EXO_ICON_VIEW(file_browser->folder_view),
                                                       path))
                    {
                        if (!select)
                            exo_icon_view_unselect_path(EXO_ICON_VIEW(file_browser->folder_view),
                                                        path);
                    }
                    else if (select)
                        exo_icon_view_select_path(EXO_ICON_VIEW(file_browser->folder_view), path);

                    // scroll to first and set cursor
                    if (first_select && select)
                    {
                        exo_icon_view_set_cursor(EXO_ICON_VIEW(file_browser->folder_view),
                                                 path,
                                                 nullptr,
                                                 false);
                        exo_icon_view_scroll_to_path(EXO_ICON_VIEW(file_browser->folder_view),
                                                     path,
                                                     true,
                                                     .25,
                                                     0);
                        first_select = false;
                    }
                    break;
                case PTK_FB_LIST_VIEW:
                    // select
                    if (gtk_tree_selection_path_is_selected(tree_sel, path))
                    {
                        if (!select)
                            gtk_tree_selection_unselect_path(tree_sel, path);
                    }
                    else if (select)
                        gtk_tree_selection_select_path(tree_sel, path);

                    // scroll to first and set cursor
                    if (first_select && select)
                    {
                        gtk_tree_view_set_cursor(GTK_TREE_VIEW(file_browser->folder_view),
                                                 path,
                                                 nullptr,
                                                 false);
                        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(file_browser->folder_view),
                                                     path,
                                                     nullptr,
                                                     true,
                                                     .25,
                                                     0);
                        first_select = false;
                    }
                    break;
                default:
                    break;
            }
            gtk_tree_path_free(path);
        } while (gtk_tree_model_iter_next(model, &it));
    }

    // restore signals and trigger sel change
    switch (file_browser->view_mode)
    {
        case PTK_FB_ICON_VIEW:
        case PTK_FB_COMPACT_VIEW:
            g_signal_handlers_unblock_matched(file_browser->folder_view,
                                              G_SIGNAL_MATCH_FUNC,
                                              0,
                                              0,
                                              nullptr,
                                              (void*)on_folder_view_item_sel_change,
                                              nullptr);
            on_folder_view_item_sel_change(EXO_ICON_VIEW(file_browser->folder_view), file_browser);
            break;
        case PTK_FB_LIST_VIEW:
            g_signal_handlers_unblock_matched(tree_sel,
                                              G_SIGNAL_MATCH_FUNC,
                                              0,
                                              0,
                                              nullptr,
                                              (void*)on_folder_view_item_sel_change,
                                              nullptr);
            on_folder_view_item_sel_change((ExoIconView*)tree_sel, file_browser);
            break;
        default:
            break;
    }
    focus_folder_view(file_browser);
}

void
ptk_file_browser_select_file_list(PtkFileBrowser* file_browser, char** filename, bool do_select)
{
    // If do_select, select all filenames, unselect others
    // if !do_select, unselect filenames, leave others unchanged
    // If !*filename select or unselect all
    GtkTreeModel* model;
    GtkTreeSelection* tree_sel;

    if (!filename || !*filename)
    {
        if (do_select)
            ptk_file_browser_select_all(nullptr, file_browser);
        else
            ptk_file_browser_unselect_all(nullptr, file_browser);
        return;
    }

    // get model, treesel, and stop signals
    switch (file_browser->view_mode)
    {
        case PTK_FB_ICON_VIEW:
        case PTK_FB_COMPACT_VIEW:
            model = exo_icon_view_get_model(EXO_ICON_VIEW(file_browser->folder_view));
            g_signal_handlers_block_matched(file_browser->folder_view,
                                            G_SIGNAL_MATCH_FUNC,
                                            0,
                                            0,
                                            nullptr,
                                            (void*)on_folder_view_item_sel_change,
                                            nullptr);
            break;
        case PTK_FB_LIST_VIEW:
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
            g_signal_handlers_block_matched(tree_sel,
                                            G_SIGNAL_MATCH_FUNC,
                                            0,
                                            0,
                                            nullptr,
                                            (void*)on_folder_view_item_sel_change,
                                            nullptr);
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_browser->folder_view));
            break;
        default:
            break;
    }

    // test rows
    bool first_select = true;
    GtkTreeIter it;
    VFSFileInfo* file;
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        bool select;
        do
        {
            // get file
            gtk_tree_model_get(model, &it, COL_FILE_INFO, &file, -1);
            if (!file)
                continue;

            // test name
            char* name = (char*)vfs_file_info_get_disp_name(file);
            char** test_name = filename;
            while (*test_name)
            {
                if (!strcmp(*test_name, name))
                    break;
                test_name++;
            }
            if (*test_name)
                select = do_select;
            else
                select = !do_select;

            // do selection and scroll to first selected
            GtkTreePath* path =
                gtk_tree_model_get_path(GTK_TREE_MODEL(PTK_FILE_LIST(file_browser->file_list)),
                                        &it);

            switch (file_browser->view_mode)
            {
                case PTK_FB_ICON_VIEW:
                case PTK_FB_COMPACT_VIEW:
                    // select
                    if (exo_icon_view_path_is_selected(EXO_ICON_VIEW(file_browser->folder_view),
                                                       path))
                    {
                        if (!select)
                            exo_icon_view_unselect_path(EXO_ICON_VIEW(file_browser->folder_view),
                                                        path);
                    }
                    else if (select && do_select)
                        exo_icon_view_select_path(EXO_ICON_VIEW(file_browser->folder_view), path);

                    // scroll to first and set cursor
                    if (first_select && select && do_select)
                    {
                        exo_icon_view_set_cursor(EXO_ICON_VIEW(file_browser->folder_view),
                                                 path,
                                                 nullptr,
                                                 false);
                        exo_icon_view_scroll_to_path(EXO_ICON_VIEW(file_browser->folder_view),
                                                     path,
                                                     true,
                                                     .25,
                                                     0);
                        first_select = false;
                    }
                    break;
                case PTK_FB_LIST_VIEW:
                    // select
                    if (gtk_tree_selection_path_is_selected(tree_sel, path))
                    {
                        if (!select)
                            gtk_tree_selection_unselect_path(tree_sel, path);
                    }
                    else if (select && do_select)
                        gtk_tree_selection_select_path(tree_sel, path);

                    // scroll to first and set cursor
                    if (first_select && select && do_select)
                    {
                        gtk_tree_view_set_cursor(GTK_TREE_VIEW(file_browser->folder_view),
                                                 path,
                                                 nullptr,
                                                 false);
                        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(file_browser->folder_view),
                                                     path,
                                                     nullptr,
                                                     true,
                                                     .25,
                                                     0);
                        first_select = false;
                    }
                    break;
                default:
                    break;
            }
            gtk_tree_path_free(path);
        } while (gtk_tree_model_iter_next(model, &it));
    }

    // restore signals and trigger sel change
    switch (file_browser->view_mode)
    {
        case PTK_FB_ICON_VIEW:
        case PTK_FB_COMPACT_VIEW:
            g_signal_handlers_unblock_matched(file_browser->folder_view,
                                              G_SIGNAL_MATCH_FUNC,
                                              0,
                                              0,
                                              nullptr,
                                              (void*)on_folder_view_item_sel_change,
                                              nullptr);
            on_folder_view_item_sel_change(EXO_ICON_VIEW(file_browser->folder_view), file_browser);
            break;
        case PTK_FB_LIST_VIEW:
            g_signal_handlers_unblock_matched(tree_sel,
                                              G_SIGNAL_MATCH_FUNC,
                                              0,
                                              0,
                                              nullptr,
                                              (void*)on_folder_view_item_sel_change,
                                              nullptr);
            on_folder_view_item_sel_change((ExoIconView*)tree_sel, file_browser);
            break;
        default:
            break;
    }
    focus_folder_view(file_browser);
}

void
ptk_file_browser_seek_path(PtkFileBrowser* file_browser, const char* seek_dir,
                           const char* seek_name)
{
    // change to dir seek_dir if needed; select first dir or else file with
    // prefix seek_name
    const char* cwd = ptk_file_browser_get_cwd(file_browser);

    if (seek_dir && g_strcmp0(cwd, seek_dir))
    {
        // change dir
        g_free(file_browser->seek_name);
        file_browser->seek_name = g_strdup(seek_name);
        file_browser->inhibit_focus = true;
        if (!ptk_file_browser_chdir(file_browser, seek_dir, PTK_FB_CHDIR_ADD_HISTORY))
        {
            file_browser->inhibit_focus = false;
            g_free(file_browser->seek_name);
            file_browser->seek_name = nullptr;
        }
        // return here to allow dir to load
        // finishes seek in main-window.c on_file_browser_after_chdir()
        return;
    }

    // no change dir was needed or was called from on_file_browser_after_chdir()
    // select seek name
    ptk_file_browser_unselect_all(nullptr, file_browser);

    if (!(seek_name && seek_name[0]))
        return;

    // get model, treesel, and stop signals
    GtkTreeModel* model;
    GtkTreeIter it;
    GtkTreeSelection* tree_sel;

    switch (file_browser->view_mode)
    {
        case PTK_FB_ICON_VIEW:
        case PTK_FB_COMPACT_VIEW:
            model = exo_icon_view_get_model(EXO_ICON_VIEW(file_browser->folder_view));
            g_signal_handlers_block_matched(file_browser->folder_view,
                                            G_SIGNAL_MATCH_FUNC,
                                            0,
                                            0,
                                            nullptr,
                                            (void*)on_folder_view_item_sel_change,
                                            nullptr);
            break;
        case PTK_FB_LIST_VIEW:
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
            g_signal_handlers_block_matched(tree_sel,
                                            G_SIGNAL_MATCH_FUNC,
                                            0,
                                            0,
                                            nullptr,
                                            (void*)on_folder_view_item_sel_change,
                                            nullptr);
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_browser->folder_view));
            break;
        default:
            break;
    }

    if (!GTK_IS_TREE_MODEL(model))
        goto _restore_sig;

    // test rows - give preference to matching dir, else match file
    GtkTreeIter it_file;
    GtkTreeIter it_dir;
    it_file.stamp = 0;
    it_dir.stamp = 0;
    VFSFileInfo* file;
    if (gtk_tree_model_get_iter_first(model, &it))
    {
        do
        {
            // get file
            gtk_tree_model_get(model, &it, COL_FILE_INFO, &file, -1);
            if (!file)
                continue;

            // test name
            char* name = (char*)vfs_file_info_get_disp_name(file);
            if (!g_strcmp0(name, seek_name))
            {
                // exact match (may be file or dir)
                it_dir = it;
                break;
            }
            if (g_str_has_prefix(name, seek_name))
            {
                // prefix found
                if (vfs_file_info_is_dir(file))
                {
                    if (!it_dir.stamp)
                        it_dir = it;
                }
                else if (!it_file.stamp)
                    it_file = it;
            }
        } while (gtk_tree_model_iter_next(model, &it));
    }

    if (it_dir.stamp)
        it = it_dir;
    else
        it = it_file;
    if (!it.stamp)
        goto _restore_sig;

    // do selection and scroll to selected
    GtkTreePath* path;
    path = gtk_tree_model_get_path(GTK_TREE_MODEL(PTK_FILE_LIST(file_browser->file_list)), &it);
    if (!path)
        goto _restore_sig;

    switch (file_browser->view_mode)
    {
        case PTK_FB_ICON_VIEW:
        case PTK_FB_COMPACT_VIEW:
            // select
            exo_icon_view_select_path(EXO_ICON_VIEW(file_browser->folder_view), path);

            // scroll and set cursor
            exo_icon_view_set_cursor(EXO_ICON_VIEW(file_browser->folder_view),
                                     path,
                                     nullptr,
                                     false);
            exo_icon_view_scroll_to_path(EXO_ICON_VIEW(file_browser->folder_view),
                                         path,
                                         true,
                                         .25,
                                         0);
            break;
        case PTK_FB_LIST_VIEW:
            // select
            gtk_tree_selection_select_path(tree_sel, path);

            // scroll and set cursor
            gtk_tree_view_set_cursor(GTK_TREE_VIEW(file_browser->folder_view),
                                     path,
                                     nullptr,
                                     false);
            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(file_browser->folder_view),
                                         path,
                                         nullptr,
                                         true,
                                         .25,
                                         0);
            break;
        default:
            break;
    }
    gtk_tree_path_free(path);

_restore_sig:
    // restore signals and trigger sel change
    switch (file_browser->view_mode)
    {
        case PTK_FB_ICON_VIEW:
        case PTK_FB_COMPACT_VIEW:
            g_signal_handlers_unblock_matched(file_browser->folder_view,
                                              G_SIGNAL_MATCH_FUNC,
                                              0,
                                              0,
                                              nullptr,
                                              (void*)on_folder_view_item_sel_change,
                                              nullptr);
            on_folder_view_item_sel_change(EXO_ICON_VIEW(file_browser->folder_view), file_browser);
            break;
        case PTK_FB_LIST_VIEW:
            g_signal_handlers_unblock_matched(tree_sel,
                                              G_SIGNAL_MATCH_FUNC,
                                              0,
                                              0,
                                              nullptr,
                                              (void*)on_folder_view_item_sel_change,
                                              nullptr);
            on_folder_view_item_sel_change((ExoIconView*)tree_sel, file_browser);
            break;
        default:
            break;
    }
}

/* signal handlers */

static void
on_folder_view_item_activated(ExoIconView* iconview, GtkTreePath* path,
                              PtkFileBrowser* file_browser)
{
    ptk_file_browser_open_selected_files(file_browser);
}

static void
on_folder_view_row_activated(GtkTreeView* tree_view, GtkTreePath* path, GtkTreeViewColumn* col,
                             PtkFileBrowser* file_browser)
{
    // file_browser->button_press = false;
    ptk_file_browser_open_selected_files(file_browser);
}

static bool
on_folder_view_item_sel_change_idle(PtkFileBrowser* file_browser)
{
    if (!GTK_IS_WIDGET(file_browser))
        return false;

    file_browser->n_sel_files = 0;
    file_browser->sel_size = 0;

    GtkTreeModel* model;
    GList* sel_files = folder_view_get_selected_items(file_browser, &model);

    GList* sel;
    for (sel = sel_files; sel; sel = g_list_next(sel))
    {
        VFSFileInfo* file;
        GtkTreeIter it;
        if (gtk_tree_model_get_iter(model, &it, (GtkTreePath*)sel->data))
        {
            gtk_tree_model_get(model, &it, COL_FILE_INFO, &file, -1);
            if (file)
            {
                file_browser->sel_size += vfs_file_info_get_size(file);
                vfs_file_info_unref(file);
            }
            ++file_browser->n_sel_files;
        }
    }

    g_list_foreach(sel_files, (GFunc)gtk_tree_path_free, nullptr);
    g_list_free(sel_files);

    g_signal_emit(file_browser, signals[SEL_CHANGE_SIGNAL], 0);
    file_browser->sel_change_idle = 0;
    return false;
}

static void
on_folder_view_item_sel_change(ExoIconView* iconview, PtkFileBrowser* file_browser)
{
    /* //sfm on_folder_view_item_sel_change fires for each selected file
     * when a file is clicked - causes hang if thousands of files are selected
     * So add only one g_idle_add at a time
     */
    if (file_browser->sel_change_idle)
        return;

    file_browser->sel_change_idle =
        g_idle_add((GSourceFunc)on_folder_view_item_sel_change_idle, file_browser);
}

static void
show_popup_menu(PtkFileBrowser* file_browser, GdkEventButton* event)
{
    char* file_path = nullptr;
    VFSFileInfo* file;

    const char* cwd = ptk_file_browser_get_cwd(file_browser);
    GList* sel_files = ptk_file_browser_get_selected_files(file_browser);
    if (!sel_files)
    {
        file = nullptr;
    }
    else
    {
        file = vfs_file_info_ref((VFSFileInfo*)sel_files->data);
        file_path = g_build_filename(cwd, vfs_file_info_get_name(file), nullptr);
    }

    /*
    int button;
    uint32_t time;
    if (event)
    {
        button = event->button;
        time = event->time;
    }
    else
    {
        button = 0;
        time = gtk_get_current_event_time();
    }
    */
    char* dir_name = nullptr;
    GtkWidget* popup =
        ptk_file_menu_new(file_browser, file_path, file, dir_name ? dir_name : cwd, sel_files);
    if (popup)
        gtk_menu_popup_at_pointer(GTK_MENU(popup), nullptr);
    if (file)
        vfs_file_info_unref(file);

    if (file_path)
        g_free(file_path);
    if (dir_name)
        g_free(dir_name);
}

/* invoke popup menu via shortcut key */
static bool
on_folder_view_popup_menu(GtkWidget* widget, PtkFileBrowser* file_browser)
{
    show_popup_menu(file_browser, nullptr);
    return true;
}

static bool
on_folder_view_button_press_event(GtkWidget* widget, GdkEventButton* event,
                                  PtkFileBrowser* file_browser)
{
    GtkTreeModel* model = nullptr;
    GtkTreePath* tree_path = nullptr;
    GtkTreeViewColumn* col = nullptr;
    GtkTreeSelection* tree_sel;
    bool ret = false;

    if (file_browser->menu_shown)
        file_browser->menu_shown = false;

    if (event->type == GDK_BUTTON_PRESS)
    {
        focus_folder_view(file_browser);
        // file_browser->button_press = true;

        if ((event_handler.win_click->s || event_handler.win_click->ob2_data) &&
            main_window_event(file_browser->main_window,
                              event_handler.win_click,
                              "evt_win_click",
                              0,
                              0,
                              "filelist",
                              0,
                              event->button,
                              event->state,
                              true))
        {
            file_browser->skip_release = true;
            return true;
        }

        if (event->button == 4 || event->button == 5 || event->button == 8 ||
            event->button == 9) // sfm
        {
            if (event->button == 4 || event->button == 8)
                ptk_file_browser_go_back(nullptr, file_browser);
            else
                ptk_file_browser_go_forward(nullptr, file_browser);
            return true;
        }

        // Alt - Left/Right Click
        if (((event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK)) ==
             GDK_MOD1_MASK) &&
            (event->button == 1 || event->button == 3)) // sfm
        {
            if (event->button == 1)
                ptk_file_browser_go_back(nullptr, file_browser);
            else
                ptk_file_browser_go_forward(nullptr, file_browser);
            return true;
        }

        switch (file_browser->view_mode)
        {
            case PTK_FB_ICON_VIEW:
            case PTK_FB_COMPACT_VIEW:
                tree_path =
                    exo_icon_view_get_path_at_pos(EXO_ICON_VIEW(widget), event->x, event->y);
                model = exo_icon_view_get_model(EXO_ICON_VIEW(widget));

                /* deselect selected files when right click on blank area */
                if (!tree_path && event->button == 3)
                    exo_icon_view_unselect_all(EXO_ICON_VIEW(widget));
                break;
            case PTK_FB_LIST_VIEW:
                model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
                gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget),
                                              event->x,
                                              event->y,
                                              &tree_path,
                                              &col,
                                              nullptr,
                                              nullptr);
                tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));

                if (col && gtk_tree_view_column_get_sort_column_id(col) != COL_FILE_NAME &&
                    tree_path) // MOD
                {
                    gtk_tree_path_free(tree_path);
                    tree_path = nullptr;
                }
                break;
            default:
                break;
        }

        /* an item is clicked, get its file path */
        VFSFileInfo* file;
        GtkTreeIter it;
        char* file_path;
        if (tree_path && gtk_tree_model_get_iter(model, &it, tree_path))
        {
            gtk_tree_model_get(model, &it, COL_FILE_INFO, &file, -1);
            file_path = g_build_filename(ptk_file_browser_get_cwd(file_browser),
                                         vfs_file_info_get_name(file),
                                         nullptr);
        }
        else /* no item is clicked */
        {
            file = nullptr;
            file_path = nullptr;
        }

        /* middle button */
        if (event->button == 2 && file_path) /* middle click on a item */
        {
            /* open in new tab if its a directory */
            if (G_LIKELY(file_path))
            {
                if (g_file_test(file_path, G_FILE_TEST_IS_DIR))
                {
                    g_signal_emit(file_browser,
                                  signals[OPEN_ITEM_SIGNAL],
                                  0,
                                  file_path,
                                  PTK_OPEN_NEW_TAB);
                }
            }
            ret = true;
        }
        else if (event->button == 3) /* right click */
        {
            /* cancel all selection, and select the item if it's not selected */
            switch (file_browser->view_mode)
            {
                case PTK_FB_ICON_VIEW:
                case PTK_FB_COMPACT_VIEW:
                    if (tree_path &&
                        !exo_icon_view_path_is_selected(EXO_ICON_VIEW(widget), tree_path))
                    {
                        exo_icon_view_unselect_all(EXO_ICON_VIEW(widget));
                        exo_icon_view_select_path(EXO_ICON_VIEW(widget), tree_path);
                    }
                    break;
                case PTK_FB_LIST_VIEW:
                    if (tree_path && !gtk_tree_selection_path_is_selected(tree_sel, tree_path))
                    {
                        gtk_tree_selection_unselect_all(tree_sel);
                        gtk_tree_selection_select_path(tree_sel, tree_path);
                    }
                    break;
                default:
                    break;
            }

            show_popup_menu(file_browser, event);
            /* FIXME if approx 5000 are selected, right-click sometimes unselects all
             * after this button_press function returns - why?  a gtk or exo bug?
             * Always happens with above show_popup_menu call disabled
             * Only when this occurs, cursor is automatically set to current row and
             * treesel 'changed' signal fires
             * Stopping changed signal had no effect
             * Using connect rather than connect_after had no effect
             * Removing signal connect had no effect
             * FIX: inhibit button release */
            ret = file_browser->menu_shown = true;
        }
        if (file)
            vfs_file_info_unref(file);
        g_free(file_path);
        gtk_tree_path_free(tree_path);
    }
    else if (event->type == GDK_2BUTTON_PRESS && event->button == 1)
    {
        // double click event -  button = 0
        if ((event_handler.win_click->s || event_handler.win_click->ob2_data) &&
            main_window_event(file_browser->main_window,
                              event_handler.win_click,
                              "evt_win_click",
                              0,
                              0,
                              "filelist",
                              0,
                              0,
                              event->state,
                              true))
            return true;

        if (file_browser->view_mode == PTK_FB_LIST_VIEW)
            /* set ret true to prevent drag_begin starting in this tab after
             * fuseiso mount.  Why?
             * row_activated occurs before GDK_2BUTTON_PRESS so use
             * file_browser->button_press to determine if row was already
             * activated or user clicked on non-row */
            ret = true;
        else if (!app_settings.single_click)
            /* sfm 1.0.6 set skip_release for Icon/Compact to prevent file
             * under cursor being selected when entering dir with double-click.
             * Also see conditional reset of skip_release in
             * ptk_file_browser_chdir(). See also
             * on_folder_view_button_release_event() */
            file_browser->skip_release = true;
    }
    return ret;
}

static bool
on_folder_view_button_release_event(GtkWidget* widget, GdkEventButton* event,
                                    PtkFileBrowser* file_browser) // sfm
{ // on left-click release on file, if not dnd or rubberbanding, unselect files
    GtkTreeModel* model;
    GtkTreePath* tree_path = nullptr;
    GtkTreeSelection* tree_sel;

    if (file_browser->is_drag || event->button != 1 || file_browser->skip_release ||
        (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK)))
    {
        if (file_browser->skip_release)
            file_browser->skip_release = false;
        // this fixes bug where right-click shows menu and release unselects files
        bool ret = file_browser->menu_shown && event->button != 1;
        if (file_browser->menu_shown)
            file_browser->menu_shown = false;
        return ret;
    }

#if 0
    switch (file_browser->view_mode)
    {
        case PTK_FB_ICON_VIEW:
        case PTK_FB_COMPACT_VIEW:
            //if (exo_icon_view_is_rubber_banding_active(EXO_ICON_VIEW(widget)))
            //    return false;
            /* Conditional on single_click below was removed 1.0.2 708f0988 bc it
             * caused a left-click to not unselect other files.  However, this
             * caused file under cursor to be selected when entering directory by
             * double-click in Icon/Compact styles.  To correct this, 1.0.6
             * conditionally sets skip_release on GDK_2BUTTON_PRESS, and doesn't
             * reset skip_release in ptk_file_browser_chdir(). */
            tree_path = exo_icon_view_get_path_at_pos(EXO_ICON_VIEW(widget), event->x, event->y);
            model = exo_icon_view_get_model(EXO_ICON_VIEW(widget));
            if (tree_path)
            {
                // unselect all but one file
                exo_icon_view_unselect_all(EXO_ICON_VIEW(widget));
                exo_icon_view_select_path(EXO_ICON_VIEW(widget), tree_path);
            }
            break;
        case PTK_FB_LIST_VIEW:
            if (gtk_tree_view_is_rubber_banding_active(GTK_TREE_VIEW(widget)))
                return false;
            if (app_settings.single_click)
            {
                model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
                gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget),
                                              event->x,
                                              event->y,
                                              &tree_path,
                                              nullptr,
                                              nullptr,
                                              nullptr);
                tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
                if (tree_path && tree_sel && gtk_tree_selection_count_selected_rows(tree_sel) > 1)
                {
                    // unselect all but one file
                    gtk_tree_selection_unselect_all(tree_sel);
                    gtk_tree_selection_select_path(tree_sel, tree_path);
                }
            }
            break;
        default:
            break;
    }
#endif

    gtk_tree_path_free(tree_path);
    return false;
}

static bool
on_dir_tree_update_sel(PtkFileBrowser* file_browser)
{
    if (!file_browser->side_dir)
        return false;
    char* dir_path = ptk_dir_tree_view_get_selected_dir(GTK_TREE_VIEW(file_browser->side_dir));

    if (dir_path)
    {
        if (strcmp(dir_path, ptk_file_browser_get_cwd(file_browser)))
        {
            if (ptk_file_browser_chdir(file_browser, dir_path, PTK_FB_CHDIR_ADD_HISTORY))
                gtk_entry_set_text(GTK_ENTRY(file_browser->path_bar), dir_path);
        }
        g_free(dir_path);
    }
    return false;
}

void
on_dir_tree_row_activated(GtkTreeView* view, GtkTreePath* path, GtkTreeViewColumn* column,
                          PtkFileBrowser* file_browser)
{
    g_idle_add((GSourceFunc)on_dir_tree_update_sel, file_browser);
}

void
ptk_file_browser_new_tab(GtkMenuItem* item, PtkFileBrowser* file_browser)
{
    const char* dir_path;

    focus_folder_view(file_browser);
    if (xset_get_s("go_set_default"))
        dir_path = xset_get_s("go_set_default");
    else
        dir_path = g_get_home_dir();

    if (!g_file_test(dir_path, G_FILE_TEST_IS_DIR))
        g_signal_emit(file_browser, signals[OPEN_ITEM_SIGNAL], 0, "/", PTK_OPEN_NEW_TAB);
    else
    {
        g_signal_emit(file_browser, signals[OPEN_ITEM_SIGNAL], 0, dir_path, PTK_OPEN_NEW_TAB);
    }
}

void
ptk_file_browser_new_tab_here(GtkMenuItem* item, PtkFileBrowser* file_browser)
{
    focus_folder_view(file_browser);
    const char* dir_path = ptk_file_browser_get_cwd(file_browser);
    if (!g_file_test(dir_path, G_FILE_TEST_IS_DIR))
    {
        if (xset_get_s("go_set_default"))
            dir_path = xset_get_s("go_set_default");
        else
            dir_path = g_get_home_dir();
    }
    if (!g_file_test(dir_path, G_FILE_TEST_IS_DIR))
        g_signal_emit(file_browser, signals[OPEN_ITEM_SIGNAL], 0, "/", PTK_OPEN_NEW_TAB);
    else
    {
        g_signal_emit(file_browser, signals[OPEN_ITEM_SIGNAL], 0, dir_path, PTK_OPEN_NEW_TAB);
    }
}

void
ptk_file_browser_save_column_widths(GtkTreeView* view, PtkFileBrowser* file_browser)
{
    if (!(GTK_IS_WIDGET(file_browser) && GTK_IS_TREE_VIEW(view)))
        return;

    if (file_browser->view_mode != PTK_FB_LIST_VIEW)
        return;

    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;

    // if the window was opened maximized and stayed maximized, or the window is
    // unmaximized and not fullscreen, save the columns
    if ((!main_window->maximized || main_window->opened_maximized) && !main_window->fullscreen)
    {
        int p = file_browser->mypanel;
        char mode = main_window->panel_context[p - 1];
        // printf("*** save_columns  fb=%#x (panel %d)  mode = %d\n", file_browser, p, mode);
        int i;
        for (i = 0; i < 6; i++)
        {
            GtkTreeViewColumn* col = gtk_tree_view_get_column(view, i);
            if (!col)
                return;
            const char* title = gtk_tree_view_column_get_title(col);
            int j;
            for (j = 0; j < 6; j++)
            {
                if (!strcmp(title, column_titles[j]))
                    break;
            }
            if (j != 6)
            {
                // save column width for this panel context
                XSet* set = xset_get_panel_mode(p, column_names[j], mode);
                int width = gtk_tree_view_column_get_width(col);
                if (width > 0)
                {
                    g_free(set->y);
                    set->y = g_strdup_printf("%d", width);
                    // printf("        %d\t%s\n", width, title );
                }
            }
        }
    }
}

static void
on_folder_view_columns_changed(GtkTreeView* view, PtkFileBrowser* file_browser)
{
    // user dragged a column to a different position - save positions
    if (!(GTK_IS_WIDGET(file_browser) && GTK_IS_TREE_VIEW(view)))
        return;

    if (file_browser->view_mode != PTK_FB_LIST_VIEW)
        return;

    int i;
    for (i = 0; i < 6; i++)
    {
        GtkTreeViewColumn* col = gtk_tree_view_get_column(view, i);
        if (!col)
            return;
        const char* title = gtk_tree_view_column_get_title(col);
        int j;
        for (j = 0; j < 6; j++)
        {
            if (!strcmp(title, column_titles[j]))
                break;
        }
        if (j != 6)
        {
            // save column position
            XSet* set = xset_get_panel(file_browser->mypanel, column_names[j]);
            g_free(set->x);
            set->x = g_strdup_printf("%d", i);
        }
    }
}

static void
on_folder_view_destroy(GtkTreeView* view, PtkFileBrowser* file_browser)
{
    unsigned int id = g_signal_lookup("columns-changed", G_TYPE_FROM_INSTANCE(view));
    if (id)
    {
        unsigned long hand =
            g_signal_handler_find((void*)view, G_SIGNAL_MATCH_ID, id, 0, nullptr, nullptr, nullptr);
        if (hand)
            g_signal_handler_disconnect((void*)view, hand);
    }
}

static bool
folder_view_search_equal(GtkTreeModel* model, int col, const char* key, GtkTreeIter* it,
                         void* search_data)
{
    char* name;
    char* lower_name = nullptr;
    bool no_match;

    if (col != COL_FILE_NAME)
        return true;

    gtk_tree_model_get(model, it, col, &name, -1);

    if (!name || !key)
        return true;

    char* lower_key = g_utf8_strdown(key, -1);
    if (!strcmp(lower_key, key))
    {
        // key is all lowercase so do icase search
        lower_name = g_utf8_strdown(name, -1);
        name = lower_name;
    }

    if (strchr(key, '*') || strchr(key, '?'))
    {
        char* key2 = g_strdup_printf("*%s*", key);
        no_match = fnmatch(key2, name, 0) != 0;
        g_free(key2);
    }
    else
    {
        bool end = g_str_has_suffix(key, "$");
        bool start = !end && (strlen(key) < 3);
        char* key2 = g_strdup(key);
        char* keyp = key2;
        if (key[0] == '^')
        {
            keyp++;
            start = true;
        }
        if (end)
            key2[strlen(key2) - 1] = '\0';
        if (start && end)
            no_match = !strstr(name, keyp);
        else if (start)
            no_match = !g_str_has_prefix(name, keyp);
        else if (end)
            no_match = !g_str_has_suffix(name, keyp);
        else
            no_match = !strstr(name, key);
        g_free(key2);
    }
    g_free(lower_name);
    g_free(lower_key);
    return no_match; // return false for match
}

static GtkWidget*
create_folder_view(PtkFileBrowser* file_browser, PtkFBViewMode view_mode)
{
    GtkWidget* folder_view = nullptr;
    GtkTreeSelection* tree_sel;
    GtkCellRenderer* renderer;
    int big_icon_size, small_icon_size, icon_size = 0;

    PangoAttrList* attr_list;
    attr_list = pango_attr_list_new();
#if PANGO_VERSION_CHECK(1, 44, 0)
    pango_attr_list_insert(attr_list, pango_attr_insert_hyphens_new(false));
#endif

    vfs_mime_type_get_icon_size(&big_icon_size, &small_icon_size);

    switch (view_mode)
    {
        case PTK_FB_ICON_VIEW:
        case PTK_FB_COMPACT_VIEW:
            folder_view = exo_icon_view_new();

            if (view_mode == PTK_FB_COMPACT_VIEW)
            {
                icon_size = file_browser->large_icons ? big_icon_size : small_icon_size;

                exo_icon_view_set_layout_mode((ExoIconView*)folder_view, EXO_ICON_VIEW_LAYOUT_COLS);
                exo_icon_view_set_orientation((ExoIconView*)folder_view,
                                              GTK_ORIENTATION_HORIZONTAL);
            }
            else
            {
                icon_size = big_icon_size;

                exo_icon_view_set_column_spacing((ExoIconView*)folder_view, 4);
                exo_icon_view_set_item_width((ExoIconView*)folder_view,
                                             icon_size < 110 ? 110 : icon_size);
            }

            exo_icon_view_set_selection_mode((ExoIconView*)folder_view, GTK_SELECTION_MULTIPLE);

            // search
            exo_icon_view_set_enable_search((ExoIconView*)folder_view, true);
            exo_icon_view_set_search_column((ExoIconView*)folder_view, COL_FILE_NAME);
            exo_icon_view_set_search_equal_func(
                (ExoIconView*)folder_view,
                (ExoIconViewSearchEqualFunc)folder_view_search_equal,
                nullptr,
                nullptr);

            exo_icon_view_set_single_click((ExoIconView*)folder_view, file_browser->single_click);
            exo_icon_view_set_single_click_timeout(
                (ExoIconView*)folder_view,
                app_settings.no_single_hover ? 0 : SINGLE_CLICK_TIMEOUT);

            gtk_cell_layout_clear(GTK_CELL_LAYOUT(folder_view));

            file_browser->icon_render = renderer = gtk_cell_renderer_pixbuf_new();

            /* add the icon renderer */
            g_object_set(G_OBJECT(renderer), "follow_state", true, nullptr);
            gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(folder_view), renderer, false);
            gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(folder_view),
                                          renderer,
                                          "pixbuf",
                                          file_browser->large_icons ? COL_FILE_BIG_ICON
                                                                    : COL_FILE_SMALL_ICON);

            /* add the name renderer */
            renderer = gtk_cell_renderer_text_new();

            if (view_mode == PTK_FB_COMPACT_VIEW)
            {
                g_object_set(G_OBJECT(renderer),
                             "xalign",
                             0.0,
                             "yalign",
                             0.5,
                             "font",
                             config_settings.font_view_compact,
                             "size-set",
                             true,
                             nullptr);
            }
            else
            {
                g_object_set(G_OBJECT(renderer),
                             "alignment",
                             PANGO_ALIGN_CENTER,
                             "wrap-mode",
                             PANGO_WRAP_WORD_CHAR,
                             "wrap-width",
                             105, // FIXME prob shouldnt hard code this
                                  // icon_size < 105 ? 109 : icon_size, // breaks with cjk text
                             "xalign",
                             0.5,
                             "yalign",
                             0.0,
                             "attributes",
                             attr_list,
                             "font",
                             config_settings.font_view_icon,
                             "size-set",
                             true,
                             nullptr);
            }
            gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(folder_view), renderer, true);
            gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(folder_view),
                                          renderer,
                                          "text",
                                          COL_FILE_NAME);

            exo_icon_view_enable_model_drag_source(
                EXO_ICON_VIEW(folder_view),
                GdkModifierType(GDK_CONTROL_MASK | GDK_BUTTON1_MASK | GDK_BUTTON3_MASK),
                drag_targets,
                G_N_ELEMENTS(drag_targets),
                (GdkDragAction)GDK_ACTION_ALL);

            exo_icon_view_enable_model_drag_dest(EXO_ICON_VIEW(folder_view),
                                                 drag_targets,
                                                 G_N_ELEMENTS(drag_targets),
                                                 (GdkDragAction)GDK_ACTION_ALL);

            g_signal_connect((void*)folder_view,
                             "item-activated",
                             G_CALLBACK(on_folder_view_item_activated),
                             file_browser);

            g_signal_connect_after((void*)folder_view,
                                   "selection-changed",
                                   G_CALLBACK(on_folder_view_item_sel_change),
                                   file_browser);

            break;
        case PTK_FB_LIST_VIEW:
            folder_view = gtk_tree_view_new();

            init_list_view(file_browser, GTK_TREE_VIEW(folder_view));

            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(folder_view));
            gtk_tree_selection_set_mode(tree_sel, GTK_SELECTION_MULTIPLE);

            if (xset_get_b("rubberband"))
                gtk_tree_view_set_rubber_banding((GtkTreeView*)folder_view, true);

            // Search
            gtk_tree_view_set_enable_search((GtkTreeView*)folder_view, true);
            gtk_tree_view_set_search_column((GtkTreeView*)folder_view, COL_FILE_NAME);
            gtk_tree_view_set_search_equal_func(
                (GtkTreeView*)folder_view,
                (GtkTreeViewSearchEqualFunc)folder_view_search_equal,
                nullptr,
                nullptr);

            // gtk_tree_view_set_single_click((GtkTreeView*)folder_view,
            // file_browser->single_click); gtk_tree_view_set_single_click_timeout(
            //    (GtkTreeView*)folder_view,
            //    app_settings.no_single_hover ? 0 : SINGLE_CLICK_TIMEOUT);

            icon_size = file_browser->large_icons ? big_icon_size : small_icon_size;

            gtk_tree_view_enable_model_drag_source(
                GTK_TREE_VIEW(folder_view),
                GdkModifierType(GDK_CONTROL_MASK | GDK_BUTTON1_MASK | GDK_BUTTON3_MASK),
                drag_targets,
                G_N_ELEMENTS(drag_targets),
                (GdkDragAction)GDK_ACTION_ALL);

            gtk_tree_view_enable_model_drag_dest(GTK_TREE_VIEW(folder_view),
                                                 drag_targets,
                                                 G_N_ELEMENTS(drag_targets),
                                                 (GdkDragAction)GDK_ACTION_ALL);

            g_signal_connect((void*)folder_view,
                             "row_activated",
                             G_CALLBACK(on_folder_view_row_activated),
                             file_browser);

            g_signal_connect_after((void*)tree_sel,
                                   "changed",
                                   G_CALLBACK(on_folder_view_item_sel_change),
                                   file_browser);
            // MOD
            g_signal_connect((void*)folder_view,
                             "columns-changed",
                             G_CALLBACK(on_folder_view_columns_changed),
                             file_browser);
            g_signal_connect((void*)folder_view,
                             "destroy",
                             G_CALLBACK(on_folder_view_destroy),
                             file_browser);
            break;
        default:
            break;
    }

    gtk_cell_renderer_set_fixed_size(file_browser->icon_render, icon_size, icon_size);

    g_signal_connect((void*)folder_view,
                     "button-press-event",
                     G_CALLBACK(on_folder_view_button_press_event),
                     file_browser);
    g_signal_connect((void*)folder_view,
                     "button-release-event",
                     G_CALLBACK(on_folder_view_button_release_event),
                     file_browser);

    g_signal_connect((void*)folder_view,
                     "popup-menu",
                     G_CALLBACK(on_folder_view_popup_menu),
                     file_browser);

    /* init drag & drop support */

    g_signal_connect((void*)folder_view,
                     "drag-data-received",
                     G_CALLBACK(on_folder_view_drag_data_received),
                     file_browser);

    g_signal_connect((void*)folder_view,
                     "drag-data-get",
                     G_CALLBACK(on_folder_view_drag_data_get),
                     file_browser);

    g_signal_connect((void*)folder_view,
                     "drag-begin",
                     G_CALLBACK(on_folder_view_drag_begin),
                     file_browser);

    g_signal_connect((void*)folder_view,
                     "drag-motion",
                     G_CALLBACK(on_folder_view_drag_motion),
                     file_browser);

    g_signal_connect((void*)folder_view,
                     "drag-leave",
                     G_CALLBACK(on_folder_view_drag_leave),
                     file_browser);

    g_signal_connect((void*)folder_view,
                     "drag-drop",
                     G_CALLBACK(on_folder_view_drag_drop),
                     file_browser);

    g_signal_connect((void*)folder_view,
                     "drag-end",
                     G_CALLBACK(on_folder_view_drag_end),
                     file_browser);

    return folder_view;
}

static void
init_list_view(PtkFileBrowser* file_browser, GtkTreeView* list_view)
{
    GtkTreeViewColumn* col;
    GtkCellRenderer* pix_renderer;

    int cols[] = {COL_FILE_NAME,
                  COL_FILE_SIZE,
                  COL_FILE_DESC,
                  COL_FILE_PERM,
                  COL_FILE_OWNER,
                  COL_FILE_MTIME};

    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;
    int p = file_browser->mypanel;
    char mode = main_window->panel_context[p - 1];

    int i;
    for (i = 0; i < G_N_ELEMENTS(cols); i++)
    {
        col = gtk_tree_view_column_new();
        gtk_tree_view_column_set_resizable(col, true);

        GtkCellRenderer* renderer = gtk_cell_renderer_text_new();

        // column order
        int j;
        for (j = 0; j < G_N_ELEMENTS(cols); j++)
        {
            if (xset_get_int_panel(p, column_names[j], "x") == i)
                break;
        }
        if (j == G_N_ELEMENTS(cols))
            j = i; // failsafe
        else
        {
            // column width
            gtk_tree_view_column_set_min_width(col, 50);
            gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_FIXED);
            XSet* set = xset_get_panel_mode(p, column_names[j], mode);
            int width = set->y ? strtol(set->y, nullptr, 10) : 100;
            if (width)
            {
                if (cols[j] == COL_FILE_NAME && !app_settings.always_show_tabs &&
                    file_browser->view_mode == PTK_FB_LIST_VIEW &&
                    gtk_notebook_get_n_pages(GTK_NOTEBOOK(file_browser->mynotebook)) == 1)
                {
                    // when tabs are added, the width of the notebook decreases
                    // by a few pixels, meaning there is not enough space for
                    // all columns - this causes a horizontal scrollbar to
                    // appear on new and sometimes first tab
                    // so shave some pixels off first columns
                    gtk_tree_view_column_set_fixed_width(col, width - 6);

                    // below causes increasing reduction of column every time new tab is
                    // added and closed - undesirable
                    PtkFileBrowser* first_fb = (PtkFileBrowser*)gtk_notebook_get_nth_page(
                        GTK_NOTEBOOK(file_browser->mynotebook),
                        0);

                    if (first_fb && first_fb->view_mode == PTK_FB_LIST_VIEW &&
                        GTK_IS_TREE_VIEW(first_fb->folder_view))
                    {
                        GtkTreeViewColumn* first_col =
                            gtk_tree_view_get_column(GTK_TREE_VIEW(first_fb->folder_view), 0);
                        if (first_col)
                        {
                            int first_width = gtk_tree_view_column_get_width(first_col);
                            if (first_width > 10)
                                gtk_tree_view_column_set_fixed_width(first_col, first_width - 6);
                        }
                    }
                }
                else
                {
                    gtk_tree_view_column_set_fixed_width(col, width);
                    // printf("init set_width %s %d\n", column_names[j], width );
                }
            }
        }

        if (cols[j] == COL_FILE_NAME)
        {
            g_object_set(G_OBJECT(renderer),
                         /* "editable", true, */
                         "ellipsize",
                         PANGO_ELLIPSIZE_END,
                         nullptr);
            /*
            g_signal_connect( renderer, "editing-started",
                              G_CALLBACK( on_filename_editing_started ), nullptr );
            */
            file_browser->icon_render = pix_renderer = gtk_cell_renderer_pixbuf_new();

            gtk_tree_view_column_pack_start(col, pix_renderer, false);
            gtk_tree_view_column_set_attributes(col,
                                                pix_renderer,
                                                "pixbuf",
                                                file_browser->large_icons ? COL_FILE_BIG_ICON
                                                                          : COL_FILE_SMALL_ICON,
                                                nullptr);

            gtk_tree_view_column_set_expand(col, true);
            gtk_tree_view_column_set_sizing(col, GTK_TREE_VIEW_COLUMN_FIXED);
            gtk_tree_view_column_set_min_width(col, 150);
            gtk_tree_view_column_set_reorderable(col, false);
            // gtk_tree_view_set_activable_column((GtkTreeView*)list_view, col);
        }
        else
        {
            gtk_tree_view_column_set_reorderable(col, true);
            gtk_tree_view_column_set_visible(col, xset_get_b_panel_mode(p, column_names[j], mode));
        }

        if (cols[j] == COL_FILE_SIZE)
            gtk_cell_renderer_set_alignment(renderer, 1, 0.5);

        gtk_tree_view_column_pack_start(col, renderer, true);
        gtk_tree_view_column_set_attributes(col, renderer, "text", cols[j], nullptr);
        gtk_tree_view_append_column(list_view, col);
        gtk_tree_view_column_set_title(col, column_titles[j]);
        gtk_tree_view_column_set_sort_indicator(col, true);
        gtk_tree_view_column_set_sort_column_id(col, cols[j]);
        gtk_tree_view_column_set_sort_order(col, GTK_SORT_DESCENDING);
    }
}

void
ptk_file_browser_refresh(GtkWidget* item, PtkFileBrowser* file_browser)
{
    if (file_browser->busy)
        // a dir is already loading
        return;

    if (!g_file_test(ptk_file_browser_get_cwd(file_browser), G_FILE_TEST_IS_DIR))
    {
        on_close_notebook_page(nullptr, file_browser);
        return;
    }

    // save cursor's file path for later re-selection
    GtkTreePath* tree_path = nullptr;
    GtkTreeModel* model = nullptr;
    GtkTreeIter it;
    VFSFileInfo* file;
    char* cursor_path = nullptr;

    switch (file_browser->view_mode)
    {
        case PTK_FB_ICON_VIEW:
        case PTK_FB_COMPACT_VIEW:
            exo_icon_view_get_cursor(EXO_ICON_VIEW(file_browser->folder_view), &tree_path, nullptr);
            model = exo_icon_view_get_model(EXO_ICON_VIEW(file_browser->folder_view));
            break;
        case PTK_FB_LIST_VIEW:
            gtk_tree_view_get_cursor(GTK_TREE_VIEW(file_browser->folder_view), &tree_path, nullptr);
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_browser->folder_view));
            break;
        default:
            break;
    }

    if (tree_path && model && gtk_tree_model_get_iter(model, &it, tree_path))
    {
        gtk_tree_model_get(model, &it, COL_FILE_INFO, &file, -1);
        if (file)
        {
            cursor_path = g_build_filename(ptk_file_browser_get_cwd(file_browser),
                                           vfs_file_info_get_name(file),
                                           nullptr);
        }
    }
    gtk_tree_path_free(tree_path);

    // these steps are similar to chdir
    // remove old dir object
    if (file_browser->dir)
    {
        g_signal_handlers_disconnect_matched(file_browser->dir,
                                             G_SIGNAL_MATCH_DATA,
                                             0,
                                             0,
                                             nullptr,
                                             nullptr,
                                             file_browser);
        g_object_unref(file_browser->dir);
        file_browser->dir = nullptr;
    }

    // destroy file list and create new one
    ptk_file_browser_update_model(file_browser);

    /* Ensuring free space at the end of the heap is freed to the OS,
     * mainly to deal with the possibility thousands of large thumbnails
     * have been freed but the memory not actually released by SpaceFM */
#if defined(__GLIBC__)
    malloc_trim(0);
#endif

    // begin load dir
    file_browser->busy = true;
    file_browser->dir = vfs_dir_get_by_path(ptk_file_browser_get_cwd(file_browser));
    g_signal_emit(file_browser, signals[BEGIN_CHDIR_SIGNAL], 0);
    if (vfs_dir_is_file_listed(file_browser->dir))
    {
        on_dir_file_listed(file_browser->dir, false, file_browser);
        if (cursor_path)
            ptk_file_browser_select_file(file_browser, cursor_path);
        file_browser->busy = false;
    }
    else
    {
        file_browser->busy = true;
        g_free(file_browser->select_path);
        file_browser->select_path = g_strdup(cursor_path);
    }
    g_signal_connect(file_browser->dir,
                     "file-listed",
                     G_CALLBACK(on_dir_file_listed),
                     file_browser);

    g_free(cursor_path);
}

unsigned int
ptk_file_browser_get_n_all_files(PtkFileBrowser* file_browser)
{
    return file_browser->dir ? file_browser->dir->n_files : 0;
}

unsigned int
ptk_file_browser_get_n_visible_files(PtkFileBrowser* file_browser)
{
    return file_browser->file_list
               ? gtk_tree_model_iter_n_children(file_browser->file_list, nullptr)
               : 0;
}

GList*
folder_view_get_selected_items(PtkFileBrowser* file_browser, GtkTreeModel** model)
{
    GtkTreeSelection* tree_sel;

    switch (file_browser->view_mode)
    {
        case PTK_FB_ICON_VIEW:
        case PTK_FB_COMPACT_VIEW:
            *model = exo_icon_view_get_model(EXO_ICON_VIEW(file_browser->folder_view));
            return exo_icon_view_get_selected_items(EXO_ICON_VIEW(file_browser->folder_view));
            break;
        case PTK_FB_LIST_VIEW:
            tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_browser->folder_view));
            return gtk_tree_selection_get_selected_rows(tree_sel, model);
            break;
        default:
            break;
    }
    return nullptr;
}

static char*
folder_view_get_drop_dir(PtkFileBrowser* file_browser, int x, int y)
{
    GtkTreePath* tree_path = nullptr;
    GtkTreeModel* model = nullptr;
    GtkTreeViewColumn* col;
    GtkTreeIter it;
    VFSFileInfo* file;
    char* dest_path = nullptr;

    switch (file_browser->view_mode)
    {
        case PTK_FB_ICON_VIEW:
        case PTK_FB_COMPACT_VIEW:
            exo_icon_view_widget_to_icon_coords(EXO_ICON_VIEW(file_browser->folder_view),
                                                x,
                                                y,
                                                &x,
                                                &y);
            tree_path = folder_view_get_tree_path_at_pos(file_browser, x, y);
            model = exo_icon_view_get_model(EXO_ICON_VIEW(file_browser->folder_view));
            break;
        case PTK_FB_LIST_VIEW:
            // if drag is in progress, get the dest row path
            gtk_tree_view_get_drag_dest_row(GTK_TREE_VIEW(file_browser->folder_view),
                                            &tree_path,
                                            nullptr);
            if (!tree_path)
            {
                // no drag in progress, get drop path
                gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(file_browser->folder_view),
                                              x,
                                              y,
                                              nullptr,
                                              &col,
                                              nullptr,
                                              nullptr);
                if (col == gtk_tree_view_get_column(GTK_TREE_VIEW(file_browser->folder_view), 0))
                {
                    gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(file_browser->folder_view),
                                                      x,
                                                      y,
                                                      &tree_path,
                                                      nullptr);
                    model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_browser->folder_view));
                }
            }
            else
                model = gtk_tree_view_get_model(GTK_TREE_VIEW(file_browser->folder_view));
            break;
        default:
            break;
    }

    if (tree_path)
    {
        if (G_UNLIKELY(!gtk_tree_model_get_iter(model, &it, tree_path)))
            return nullptr;

        gtk_tree_model_get(model, &it, COL_FILE_INFO, &file, -1);
        if (file)
        {
            if (vfs_file_info_is_dir(file))
            {
                dest_path = g_build_filename(ptk_file_browser_get_cwd(file_browser),
                                             vfs_file_info_get_name(file),
                                             nullptr);
                /*  this isn't needed?
                                // dest_path is a link? resolve
                                if ( g_file_test( dest_path, G_FILE_TEST_IS_SYMLINK ) )
                                {
                                    char* old_dest = dest_path;
                                    dest_path = g_file_read_link( old_dest, nullptr );
                                    g_free( old_dest );
                                }
                */
            }
            else /* Drop on a file, not directory */
            {
                /* Return current directory */
                dest_path = g_strdup(ptk_file_browser_get_cwd(file_browser));
            }
            vfs_file_info_unref(file);
        }
        gtk_tree_path_free(tree_path);
    }
    else
    {
        dest_path = g_strdup(ptk_file_browser_get_cwd(file_browser));
    }
    return dest_path;
}

static void
on_folder_view_drag_data_received(GtkWidget* widget, GdkDragContext* drag_context, int x, int y,
                                  GtkSelectionData* sel_data, unsigned int info, unsigned int time,
                                  void* user_data)
{
    PtkFileBrowser* file_browser = (PtkFileBrowser*)user_data;
    char* dest_dir;
    char* file_path;
    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name(widget, "drag-data-received");

    if ((gtk_selection_data_get_length(sel_data) >= 0) &&
        (gtk_selection_data_get_format(sel_data) == 8))
    {
        // (list view) use stored x and y because == 0 for update drag status
        //             when is last row (gtk2&3 bug?)
        // and because exo_icon_view has no get_drag_dest_row
        dest_dir =
            folder_view_get_drop_dir(file_browser, file_browser->drag_x, file_browser->drag_y);
        // printf("FB dest_dir = %s\n", dest_dir );
        if (dest_dir)
        {
            char** list;
            char** puri;
            puri = list = gtk_selection_data_get_uris(sel_data);

            if (file_browser->pending_drag_status)
            {
                // We only want to update drag status, not really want to drop
                dev_t dest_dev;
                ino_t dest_inode;
                struct stat statbuf; // skip stat
                if (stat(dest_dir, &statbuf) == 0)
                {
                    dest_dev = statbuf.st_dev;
                    dest_inode = statbuf.st_ino;
                    if (file_browser->drag_source_dev == 0)
                    {
                        file_browser->drag_source_dev = dest_dev;
                        for (; *puri; ++puri)
                        {
                            file_path = g_filename_from_uri(*puri, nullptr, nullptr);
                            if (file_path && stat(file_path, &statbuf) == 0)
                            {
                                if (statbuf.st_dev != dest_dev)
                                {
                                    // different devices - store source device
                                    file_browser->drag_source_dev = statbuf.st_dev;
                                    g_free(file_path);
                                    break;
                                }
                                else if (file_browser->drag_source_inode == 0)
                                {
                                    // same device - store source parent inode
                                    char* src_dir = g_path_get_dirname(file_path);
                                    if (src_dir && stat(src_dir, &statbuf) == 0)
                                    {
                                        file_browser->drag_source_inode = statbuf.st_ino;
                                    }
                                    g_free(src_dir);
                                }
                            }
                            g_free(file_path);
                        }
                    }
                    if (file_browser->drag_source_dev != dest_dev ||
                        file_browser->drag_source_inode == dest_inode)
                        // src and dest are on different devices or same dir
                        gdk_drag_status(drag_context, GDK_ACTION_COPY, time);
                    else
                        gdk_drag_status(drag_context, GDK_ACTION_MOVE, time);
                }
                else
                    // stat failed
                    gdk_drag_status(drag_context, GDK_ACTION_COPY, time);

                g_free(dest_dir);
                g_strfreev(list);
                file_browser->pending_drag_status = 0;
                return;
            }
            if (puri)
            {
                GList* files = nullptr;
                if ((gdk_drag_context_get_selected_action(drag_context) &
                     (GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK)) == 0)
                {
                    gdk_drag_status(drag_context, GDK_ACTION_COPY, time); // sfm correct?  was MOVE
                }
                gtk_drag_finish(drag_context, true, false, time);

                while (*puri)
                {
                    if (**puri == '/')
                        file_path = g_strdup(*puri);
                    else
                        file_path = g_filename_from_uri(*puri, nullptr, nullptr);

                    if (file_path)
                        files = g_list_prepend(files, file_path);
                    ++puri;
                }
                g_strfreev(list);

                VFSFileTaskType file_action;
                switch (gdk_drag_context_get_selected_action(drag_context))
                {
                    case GDK_ACTION_COPY:
                        file_action = VFS_FILE_TASK_COPY;
                        break;
                    case GDK_ACTION_MOVE:
                        file_action = VFS_FILE_TASK_MOVE;
                        break;
                    case GDK_ACTION_LINK:
                        file_action = VFS_FILE_TASK_LINK;
                        break;
                        /* FIXME:
                          GDK_ACTION_DEFAULT, GDK_ACTION_PRIVATE, and GDK_ACTION_ASK are not handled
                        */
                    default:
                        break;
                }

                if (files)
                {
                    /* g_print( "dest_dir = %s\n", dest_dir ); */

                    /* We only want to update drag status, not really want to drop */
                    if (file_browser->pending_drag_status)
                    {
                        struct stat statbuf; // skip stat
                        if (stat(dest_dir, &statbuf) == 0)
                        {
                            file_browser->pending_drag_status = 0;
                        }
                        g_list_foreach(files, (GFunc)g_free, nullptr);
                        g_list_free(files);
                        g_free(dest_dir);
                        return;
                    }
                    else /* Accept the drop and perform file actions */
                    {
                        GtkWidget* parent_win = gtk_widget_get_toplevel(GTK_WIDGET(file_browser));
                        PtkFileTask* task = ptk_file_task_new(file_action,
                                                              files,
                                                              dest_dir,
                                                              GTK_WINDOW(parent_win),
                                                              file_browser->task_view);
                        ptk_file_task_run(task);
                    }
                }
                g_free(dest_dir);
                gtk_drag_finish(drag_context, true, false, time);
                return;
            }
        }
        g_free(dest_dir);
    }

    /* If we are only getting drag status, not finished. */
    if (file_browser->pending_drag_status)
    {
        file_browser->pending_drag_status = 0;
        return;
    }
    gtk_drag_finish(drag_context, false, false, time);
}

static void
on_folder_view_drag_data_get(GtkWidget* widget, GdkDragContext* drag_context,
                             GtkSelectionData* sel_data, unsigned int info, unsigned int time,
                             PtkFileBrowser* file_browser)
{
    GdkAtom type = gdk_atom_intern("text/uri-list", false);
    GString* uri_list = g_string_sized_new(8192);
    GList* sels = ptk_file_browser_get_selected_files(file_browser);
    GList* sel;
    VFSFileInfo* file;
    char* full_path;

    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name(widget, "drag-data-get");

    // drag_context->suggested_action = GDK_ACTION_MOVE;

    for (sel = sels; sel; sel = g_list_next(sel))
    {
        file = (VFSFileInfo*)sel->data;
        full_path = g_build_filename(ptk_file_browser_get_cwd(file_browser),
                                     vfs_file_info_get_name(file),
                                     nullptr);
        char* uri = g_filename_to_uri(full_path, nullptr, nullptr);
        g_free(full_path);
        g_string_append(uri_list, uri);
        g_free(uri);

        g_string_append(uri_list, "\n");
    }
    g_list_foreach(sels, (GFunc)vfs_file_info_unref, nullptr);
    g_list_free(sels);
    gtk_selection_data_set(sel_data, type, 8, (unsigned char*)uri_list->str, uri_list->len + 1);
    g_string_free(uri_list, true);
}

static void
on_folder_view_drag_begin(GtkWidget* widget, GdkDragContext* drag_context,
                          PtkFileBrowser* file_browser)
{
    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name(widget, "drag-begin");
    gtk_drag_set_icon_default(drag_context);
    file_browser->is_drag = true;
}

static GtkTreePath*
folder_view_get_tree_path_at_pos(PtkFileBrowser* file_browser, int x, int y)
{
    GtkTreePath* tree_path;

    switch (file_browser->view_mode)
    {
        case PTK_FB_ICON_VIEW:
        case PTK_FB_COMPACT_VIEW:
            tree_path =
                exo_icon_view_get_path_at_pos(EXO_ICON_VIEW(file_browser->folder_view), x, y);
            break;
        case PTK_FB_LIST_VIEW:
            gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(file_browser->folder_view),
                                          x,
                                          y,
                                          &tree_path,
                                          nullptr,
                                          nullptr,
                                          nullptr);
            break;
        default:
            break;
    }

    return tree_path;
}

static bool
on_folder_view_auto_scroll(GtkScrolledWindow* scroll)
{
    GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(scroll);
    double vpos = gtk_adjustment_get_value(vadj);

    if (folder_view_auto_scroll_direction == GTK_DIR_UP)
    {
        vpos -= gtk_adjustment_get_step_increment(vadj);
        if (vpos > gtk_adjustment_get_lower(vadj))
            gtk_adjustment_set_value(vadj, vpos);
        else
            gtk_adjustment_set_value(vadj, gtk_adjustment_get_lower(vadj));
    }
    else
    {
        vpos += gtk_adjustment_get_step_increment(vadj);
        if ((vpos + gtk_adjustment_get_page_size(vadj)) < gtk_adjustment_get_upper(vadj))
            gtk_adjustment_set_value(vadj, vpos);
        else
            gtk_adjustment_set_value(
                vadj,
                (gtk_adjustment_get_upper(vadj) - gtk_adjustment_get_page_size(vadj)));
    }
    return true;
}

static bool
on_folder_view_drag_motion(GtkWidget* widget, GdkDragContext* drag_context, int x, int y,
                           unsigned int time, PtkFileBrowser* file_browser)
{
    GtkAllocation allocation;

    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name(widget, "drag-motion");

    GtkScrolledWindow* scroll = GTK_SCROLLED_WINDOW(gtk_widget_get_parent(widget));

    GtkAdjustment* vadj = gtk_scrolled_window_get_vadjustment(scroll);
    double vpos = gtk_adjustment_get_value(vadj);
    gtk_widget_get_allocation(widget, &allocation);

    if (y < 32)
    {
        /* Auto scroll up */
        if (!folder_view_auto_scroll_timer)
        {
            folder_view_auto_scroll_direction = GTK_DIR_UP;
            folder_view_auto_scroll_timer =
                g_timeout_add(150, (GSourceFunc)on_folder_view_auto_scroll, scroll);
        }
    }
    else if (y > (allocation.height - 32))
    {
        if (!folder_view_auto_scroll_timer)
        {
            folder_view_auto_scroll_direction = GTK_DIR_DOWN;
            folder_view_auto_scroll_timer =
                g_timeout_add(150, (GSourceFunc)on_folder_view_auto_scroll, scroll);
        }
    }
    else if (folder_view_auto_scroll_timer)
    {
        g_source_remove(folder_view_auto_scroll_timer);
        folder_view_auto_scroll_timer = 0;
    }

    GtkTreeViewColumn* col;
    GtkTreeModel* model = nullptr;
    GtkTreePath* tree_path = nullptr;

    switch (file_browser->view_mode)
    {
        case PTK_FB_ICON_VIEW:
        case PTK_FB_COMPACT_VIEW:
            // store x and y because exo_icon_view has no get_drag_dest_row
            file_browser->drag_x = x;
            file_browser->drag_y = y;
            exo_icon_view_widget_to_icon_coords(EXO_ICON_VIEW(widget), x, y, &x, &y);
            tree_path = exo_icon_view_get_path_at_pos(EXO_ICON_VIEW(widget), x, y);
            model = exo_icon_view_get_model(EXO_ICON_VIEW(widget));
            break;
        case PTK_FB_LIST_VIEW:
            // store x and y because == 0 for update drag status when is last row
            file_browser->drag_x = x;
            file_browser->drag_y = y;
            if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget),
                                              x,
                                              y,
                                              nullptr,
                                              &col,
                                              nullptr,
                                              nullptr))
            {
                if (gtk_tree_view_get_column(GTK_TREE_VIEW(widget), 0) == col)
                {
                    gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget),
                                                      x,
                                                      y,
                                                      &tree_path,
                                                      nullptr);
                    model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
                }
            }
            break;
        default:
            break;
    }

    if (tree_path)
    {
        GtkTreeIter it;
        if (gtk_tree_model_get_iter(model, &it, tree_path))
        {
            VFSFileInfo* file;
            gtk_tree_model_get(model, &it, COL_FILE_INFO, &file, -1);
            if (!file || !vfs_file_info_is_dir(file))
            {
                gtk_tree_path_free(tree_path);
                tree_path = nullptr;
            }
            vfs_file_info_unref(file);
        }
    }

    switch (file_browser->view_mode)
    {
        case PTK_FB_ICON_VIEW:
        case PTK_FB_COMPACT_VIEW:
            exo_icon_view_set_drag_dest_item(EXO_ICON_VIEW(widget),
                                             tree_path,
                                             EXO_ICON_VIEW_DROP_INTO);
            break;
        case PTK_FB_LIST_VIEW:
            gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(widget),
                                            tree_path,
                                            GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
            break;
        default:
            break;
    }

    if (tree_path)
        gtk_tree_path_free(tree_path);

    /* FIXME: Creating a new target list everytime is very inefficient,
         but currently gtk_drag_dest_get_target_list always returns nullptr
         due to some strange reason, and cannot be used currently.  */
    GtkTargetList* target_list = gtk_target_list_new(drag_targets, G_N_ELEMENTS(drag_targets));
    GdkAtom target = gtk_drag_dest_find_target(widget, drag_context, target_list);
    gtk_target_list_unref(target_list);

    if (target == GDK_NONE)
        gdk_drag_status(drag_context, (GdkDragAction)0, time);
    else
    {
        GdkDragAction suggested_action;
        /* Only 'move' is available. The user force move action by pressing Shift key */
        if ((gdk_drag_context_get_actions(drag_context) & GDK_ACTION_ALL) == GDK_ACTION_MOVE)
            suggested_action = GDK_ACTION_MOVE;
        /* Only 'copy' is available. The user force copy action by pressing Ctrl key */
        else if ((gdk_drag_context_get_actions(drag_context) & GDK_ACTION_ALL) == GDK_ACTION_COPY)
            suggested_action = GDK_ACTION_COPY;
        /* Only 'link' is available. The user force link action by pressing Shift+Ctrl key */
        else if ((gdk_drag_context_get_actions(drag_context) & GDK_ACTION_ALL) == GDK_ACTION_LINK)
            suggested_action = GDK_ACTION_LINK;
        /* Several different actions are available. We have to figure out a good default action. */
        else
        {
            int drag_action = xset_get_int("drag_action", "x");

            switch (drag_action)
            {
                case 1:
                    suggested_action = GDK_ACTION_COPY;
                    break;
                case 2:
                    suggested_action = GDK_ACTION_MOVE;
                    break;
                case 3:
                    suggested_action = GDK_ACTION_LINK;
                    break;
                default:
                    // automatic
                    file_browser->pending_drag_status = 1;
                    gtk_drag_get_data(widget, drag_context, target, time);
                    suggested_action = gdk_drag_context_get_selected_action(drag_context);
                    break;
            }
        }
        gdk_drag_status(drag_context, suggested_action, time);
    }
    return true;
}

static bool
on_folder_view_drag_leave(GtkWidget* widget, GdkDragContext* drag_context, unsigned int time,
                          PtkFileBrowser* file_browser)
{
    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name(widget, "drag-leave");
    file_browser->drag_source_dev = 0;
    file_browser->drag_source_inode = 0;

    if (folder_view_auto_scroll_timer)
    {
        g_source_remove(folder_view_auto_scroll_timer);
        folder_view_auto_scroll_timer = 0;
    }
    return true;
}

static bool
on_folder_view_drag_drop(GtkWidget* widget, GdkDragContext* drag_context, int x, int y,
                         unsigned int time, PtkFileBrowser* file_browser)
{
    GdkAtom target = gdk_atom_intern("text/uri-list", false);
    /*  Don't call the default handler  */
    g_signal_stop_emission_by_name(widget, "drag-drop");

    gtk_drag_get_data(widget, drag_context, target, time);
    return true;
}

static void
on_folder_view_drag_end(GtkWidget* widget, GdkDragContext* drag_context,
                        PtkFileBrowser* file_browser)
{
    if (folder_view_auto_scroll_timer)
    {
        g_source_remove(folder_view_auto_scroll_timer);
        folder_view_auto_scroll_timer = 0;
    }

    switch (file_browser->view_mode)
    {
        case PTK_FB_ICON_VIEW:
        case PTK_FB_COMPACT_VIEW:
            exo_icon_view_set_drag_dest_item(EXO_ICON_VIEW(widget),
                                             nullptr,
                                             (ExoIconViewDropPosition)0);
            break;
        case PTK_FB_LIST_VIEW:
            gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(widget),
                                            nullptr,
                                            (GtkTreeViewDropPosition)0);
            break;
        default:
            break;
    }
    file_browser->is_drag = false;
}

void
ptk_file_browser_rename_selected_files(PtkFileBrowser* file_browser, GList* files, char* cwd)
{
    if (!file_browser)
        return;
    gtk_widget_grab_focus(file_browser->folder_view);
    gtk_widget_get_toplevel(GTK_WIDGET(file_browser));

    if (!files)
        return;

    GList* l;
    for (l = files; l; l = l->next)
    {
        VFSFileInfo* file = (VFSFileInfo*)l->data;
        if (!ptk_rename_file(file_browser, cwd, file, nullptr, false, PTK_RENAME, nullptr))
            break;
    }
}

void
ptk_file_browser_paste_link(PtkFileBrowser* file_browser) // MOD added
{
    ptk_clipboard_paste_links(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(file_browser))),
                              ptk_file_browser_get_cwd(file_browser),
                              GTK_TREE_VIEW(file_browser->task_view),
                              nullptr,
                              nullptr);
}

void
ptk_file_browser_paste_target(PtkFileBrowser* file_browser) // MOD added
{
    ptk_clipboard_paste_targets(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(file_browser))),
                                ptk_file_browser_get_cwd(file_browser),
                                GTK_TREE_VIEW(file_browser->task_view),
                                nullptr,
                                nullptr);
}

GList*
ptk_file_browser_get_selected_files(PtkFileBrowser* file_browser)
{
    GtkTreeModel* model;
    GList* sel_files = folder_view_get_selected_items(file_browser, &model);
    if (!sel_files)
        return nullptr;

    GList* file_list = nullptr;
    GList* sel;
    for (sel = sel_files; sel; sel = g_list_next(sel))
    {
        GtkTreeIter it;
        VFSFileInfo* file;
        gtk_tree_model_get_iter(model, &it, (GtkTreePath*)sel->data);
        gtk_tree_model_get(model, &it, COL_FILE_INFO, &file, -1);
        file_list = g_list_append(file_list, file);
    }
    g_list_foreach(sel_files, (GFunc)gtk_tree_path_free, nullptr);
    g_list_free(sel_files);
    return file_list;
}

static void
ptk_file_browser_open_selected_files_with_app(PtkFileBrowser* file_browser, char* app_desktop)
{
    GList* sel_files = ptk_file_browser_get_selected_files(file_browser);

    ptk_open_files_with_app(ptk_file_browser_get_cwd(file_browser),
                            sel_files,
                            app_desktop,
                            file_browser,
                            false,
                            false);

    vfs_file_info_list_free(sel_files);
}

void
ptk_file_browser_open_selected_files(PtkFileBrowser* file_browser)
{
    if (xset_opener(file_browser, 1))
        return;
    ptk_file_browser_open_selected_files_with_app(file_browser, nullptr);
}

void
ptk_file_browser_copycmd(PtkFileBrowser* file_browser, GList* sel_files, char* cwd, char* setname)
{
    if (!setname || !file_browser || !sel_files)
        return;
    XSet* set2;
    char* copy_dest = nullptr;
    char* move_dest = nullptr;

    if (!strcmp(setname, "copy_tab_prev"))
        copy_dest = main_window_get_tab_cwd(file_browser, -1);
    else if (!strcmp(setname, "copy_tab_next"))
        copy_dest = main_window_get_tab_cwd(file_browser, -2);
    else if (!strncmp(setname, "copy_tab_", 9))
        copy_dest = main_window_get_tab_cwd(file_browser, strtol(setname + 9, nullptr, 10));
    else if (!strcmp(setname, "copy_panel_prev"))
        copy_dest = main_window_get_panel_cwd(file_browser, -1);
    else if (!strcmp(setname, "copy_panel_next"))
        copy_dest = main_window_get_panel_cwd(file_browser, -2);
    else if (!strncmp(setname, "copy_panel_", 11))
        copy_dest = main_window_get_panel_cwd(file_browser, strtol(setname + 11, nullptr, 10));
    else if (!strcmp(setname, "copy_loc_last"))
    {
        set2 = xset_get("copy_loc_last");
        copy_dest = g_strdup(set2->desc);
    }
    else if (!strcmp(setname, "move_tab_prev"))
        move_dest = main_window_get_tab_cwd(file_browser, -1);
    else if (!strcmp(setname, "move_tab_next"))
        move_dest = main_window_get_tab_cwd(file_browser, -2);
    else if (!strncmp(setname, "move_tab_", 9))
        move_dest = main_window_get_tab_cwd(file_browser, strtol(setname + 9, nullptr, 10));
    else if (!strcmp(setname, "move_panel_prev"))
        move_dest = main_window_get_panel_cwd(file_browser, -1);
    else if (!strcmp(setname, "move_panel_next"))
        move_dest = main_window_get_panel_cwd(file_browser, -2);
    else if (!strncmp(setname, "move_panel_", 11))
        move_dest = main_window_get_panel_cwd(file_browser, strtol(setname + 11, nullptr, 10));
    else if (!strcmp(setname, "move_loc_last"))
    {
        set2 = xset_get("copy_loc_last");
        move_dest = g_strdup(set2->desc);
    }

    if ((g_str_has_prefix(setname, "copy_loc") || g_str_has_prefix(setname, "move_loc")) &&
        !copy_dest && !move_dest)
    {
        char* folder;
        set2 = xset_get("copy_loc_last");
        if (set2->desc)
            folder = set2->desc;
        else
            folder = cwd;
        char* path = xset_file_dialog(GTK_WIDGET(file_browser),
                                      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                      "Choose Location",
                                      folder,
                                      nullptr);
        if (path && g_file_test(path, G_FILE_TEST_IS_DIR))
        {
            if (g_str_has_prefix(setname, "copy_loc"))
                copy_dest = path;
            else
                move_dest = path;
            set2 = xset_get("copy_loc_last");
            xset_set_set(set2, XSET_SET_SET_DESC, path);
        }
        else
            return;
    }

    if (copy_dest || move_dest)
    {
        int file_action;
        char* dest_dir;

        if (copy_dest)
        {
            file_action = VFS_FILE_TASK_COPY;
            dest_dir = copy_dest;
        }
        else
        {
            file_action = VFS_FILE_TASK_MOVE;
            dest_dir = move_dest;
        }

        if (!strcmp(dest_dir, cwd))
        {
            xset_msg_dialog(GTK_WIDGET(file_browser),
                            GTK_MESSAGE_ERROR,
                            "Invalid Destination",
                            0,
                            "Destination same as source",
                            nullptr,
                            nullptr);
            g_free(dest_dir);
            return;
        }

        // rebuild sel_files with full paths
        GList* file_list = nullptr;
        GList* sel;
        char* file_path;
        VFSFileInfo* file;
        for (sel = sel_files; sel; sel = sel->next)
        {
            file = (VFSFileInfo*)sel->data;
            file_path = g_build_filename(cwd, vfs_file_info_get_name(file), nullptr);
            file_list = g_list_prepend(file_list, file_path);
        }

        // task
        PtkFileTask* task =
            ptk_file_task_new((VFSFileTaskType)file_action,
                              file_list,
                              dest_dir,
                              GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(file_browser))),
                              file_browser->task_view);
        ptk_file_task_run(task);
        g_free(dest_dir);
    }
    else
    {
        xset_msg_dialog(GTK_WIDGET(file_browser),
                        GTK_MESSAGE_ERROR,
                        "Invalid Destination",
                        0,
                        "Invalid destination",
                        nullptr,
                        nullptr);
    }
}

void
ptk_file_browser_hide_selected(PtkFileBrowser* file_browser, GList* files, char* cwd)
{
    if (xset_msg_dialog(
            GTK_WIDGET(file_browser),
            0,
            "Hide File",
            GTK_BUTTONS_OK_CANCEL,
            "The names of the selected files will be added to the '.hidden' file located in this "
            "directory, which will hide them from view in SpaceFM.  You may need to refresh the "
            "view or restart SpaceFM for the files to disappear.\n\nTo unhide a file, open the "
            ".hidden file in your text editor, remove the name of the file, and refresh.",
            nullptr,
            nullptr) != GTK_RESPONSE_OK)
        return;

    VFSFileInfo* file;
    GList* l;

    if (files)
    {
        for (l = files; l; l = l->next)
        {
            file = (VFSFileInfo*)l->data;
            if (!vfs_dir_add_hidden(cwd, vfs_file_info_get_name(file)))
                ptk_show_error(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(file_browser))),
                               "Error",
                               "Error hiding files");
        }
        // refresh from here causes a segfault occasionally
        // ptk_file_browser_refresh( nullptr, file_browser );
    }
    else
        ptk_show_error(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(file_browser))),
                       "Error",
                       "No files are selected");
}

void
ptk_file_browser_file_properties(PtkFileBrowser* file_browser, int page)
{
    if (!file_browser)
        return;

    char* dir_name = nullptr;
    GList* sel_files = ptk_file_browser_get_selected_files(file_browser);
    const char* cwd = ptk_file_browser_get_cwd(file_browser);
    if (!sel_files)
    {
        VFSFileInfo* file = vfs_file_info_new();
        vfs_file_info_get(file, ptk_file_browser_get_cwd(file_browser), nullptr);
        sel_files = g_list_prepend(nullptr, file);
        dir_name = g_path_get_dirname(cwd);
    }
    GtkWidget* parent = gtk_widget_get_toplevel(GTK_WIDGET(file_browser));
    ptk_show_file_properties(GTK_WINDOW(parent), dir_name ? dir_name : cwd, sel_files, page);
    vfs_file_info_list_free(sel_files);
    g_free(dir_name);
}

void
on_popup_file_properties_activate(GtkMenuItem* menuitem, void* user_data)
{
    GObject* popup = G_OBJECT(user_data);
    PtkFileBrowser* file_browser = (PtkFileBrowser*)g_object_get_data(popup, "PtkFileBrowser");
    ptk_file_browser_file_properties(file_browser, 0);
}

void
ptk_file_browser_show_hidden_files(PtkFileBrowser* file_browser, bool show)
{
    if (!!file_browser->show_hidden_files == show)
        return;
    file_browser->show_hidden_files = show;

    if (file_browser->file_list)
    {
        ptk_file_browser_update_model(file_browser);
        g_signal_emit(file_browser, signals[SEL_CHANGE_SIGNAL], 0);
    }

    if (file_browser->side_dir)
    {
        ptk_dir_tree_view_show_hidden_files(GTK_TREE_VIEW(file_browser->side_dir),
                                            file_browser->show_hidden_files);
    }

    ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSET_TOOL_SHOW_HIDDEN);
}

static bool
on_dir_tree_button_press(GtkWidget* view, GdkEventButton* evt, PtkFileBrowser* file_browser)
{
    ptk_file_browser_focus_me(file_browser);

    if ((event_handler.win_click->s || event_handler.win_click->ob2_data) &&
        main_window_event(file_browser->main_window,
                          event_handler.win_click,
                          "evt_win_click",
                          0,
                          0,
                          "dirtree",
                          0,
                          evt->button,
                          evt->state,
                          true))
        return false;

    if (evt->type == GDK_BUTTON_PRESS && evt->button == 2) /* middle click */
    {
        /* left and right click handled in ptk-dir-tree-view.c
         * on_dir_tree_view_button_press() */
        GtkTreeModel* model;
        GtkTreePath* tree_path;
        GtkTreeIter it;

        model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
        if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view),
                                          evt->x,
                                          evt->y,
                                          &tree_path,
                                          nullptr,
                                          nullptr,
                                          nullptr))
        {
            if (gtk_tree_model_get_iter(model, &it, tree_path))
            {
                VFSFileInfo* file;
                gtk_tree_model_get(model, &it, COL_DIR_TREE_INFO, &file, -1);
                if (file)
                {
                    char* file_path;
                    file_path = ptk_dir_view_get_dir_path(model, &it);
                    g_signal_emit(file_browser,
                                  signals[OPEN_ITEM_SIGNAL],
                                  0,
                                  file_path,
                                  PTK_OPEN_NEW_TAB);
                    g_free(file_path);
                    vfs_file_info_unref(file);
                }
            }
            gtk_tree_path_free(tree_path);
        }
        return true;
    }
    return false;
}

static GtkWidget*
ptk_file_browser_create_dir_tree(PtkFileBrowser* file_browser)
{
    GtkWidget* dir_tree = ptk_dir_tree_view_new(file_browser, file_browser->show_hidden_files);
    GtkTreeSelection* dir_tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(dir_tree));
    g_signal_connect(dir_tree,
                     "row-activated",
                     G_CALLBACK(on_dir_tree_row_activated),
                     file_browser);
    g_signal_connect(dir_tree,
                     "button-press-event",
                     G_CALLBACK(on_dir_tree_button_press),
                     file_browser);

    return dir_tree;
}

static int
file_list_order_from_sort_order(PtkFBSortOrder order)
{
    int col;

    switch (order)
    {
        case PTK_FB_SORT_BY_NAME:
            col = COL_FILE_NAME;
            break;
        case PTK_FB_SORT_BY_SIZE:
            col = COL_FILE_SIZE;
            break;
        case PTK_FB_SORT_BY_MTIME:
            col = COL_FILE_MTIME;
            break;
        case PTK_FB_SORT_BY_TYPE:
            col = COL_FILE_DESC;
            break;
        case PTK_FB_SORT_BY_PERM:
            col = COL_FILE_PERM;
            break;
        case PTK_FB_SORT_BY_OWNER:
            col = COL_FILE_OWNER;
            break;
        default:
            col = COL_FILE_NAME;
    }
    return col;
}

void
ptk_file_browser_read_sort_extra(PtkFileBrowser* file_browser)
{
    PtkFileList* list = PTK_FILE_LIST(file_browser->file_list);
    if (!list)
        return;

    list->sort_natural = xset_get_b_panel(file_browser->mypanel, "sort_extra");
    list->sort_case = xset_get_int_panel(file_browser->mypanel, "sort_extra", "x") == XSET_B_TRUE;
    list->sort_dir = xset_get_int_panel(file_browser->mypanel, "sort_extra", "y");
    list->sort_hidden_first =
        xset_get_int_panel(file_browser->mypanel, "sort_extra", "z") == XSET_B_TRUE;
}

void
ptk_file_browser_set_sort_extra(PtkFileBrowser* file_browser, const char* setname)
{
    if (!file_browser || !setname)
        return;

    XSet* set = xset_get(setname);

    if (!g_str_has_prefix(set->name, "sortx_"))
        return;

    const char* name = set->name + 6;
    PtkFileList* list = PTK_FILE_LIST(file_browser->file_list);
    if (!list)
        return;
    int panel = file_browser->mypanel;
    char* val = nullptr;

    if (!strcmp(name, "natural"))
    {
        list->sort_natural = set->b == XSET_B_TRUE;
        xset_set_b_panel(panel, "sort_extra", list->sort_natural);
    }
    else if (!strcmp(name, "case"))
    {
        list->sort_case = set->b == XSET_B_TRUE;
        val = g_strdup_printf("%d", set->b);
        xset_set_panel(panel, "sort_extra", "x", val);
    }
    else if (!strcmp(name, "directories"))
    {
        list->sort_dir = PTK_LIST_SORT_DIR_FIRST;
        val = g_strdup_printf("%d", PTK_LIST_SORT_DIR_FIRST);
        xset_set_panel(panel, "sort_extra", "y", val);
    }
    else if (!strcmp(name, "files"))
    {
        list->sort_dir = PTK_LIST_SORT_DIR_LAST;
        val = g_strdup_printf("%d", PTK_LIST_SORT_DIR_LAST);
        xset_set_panel(panel, "sort_extra", "y", val);
    }
    else if (!strcmp(name, "mix"))
    {
        list->sort_dir = PTK_LIST_SORT_DIR_MIXED;
        val = g_strdup_printf("%d", PTK_LIST_SORT_DIR_MIXED);
        xset_set_panel(panel, "sort_extra", "y", val);
    }
    else if (!strcmp(name, "hidfirst"))
    {
        list->sort_hidden_first = set->b == XSET_B_TRUE;
        val = g_strdup_printf("%d", set->b);
        xset_set_panel(panel, "sort_extra", "z", val);
    }
    else if (!strcmp(name, "hidlast"))
    {
        list->sort_hidden_first = set->b != XSET_B_TRUE;
        val = g_strdup_printf("%d", set->b == XSET_B_TRUE ? XSET_B_FALSE : XSET_B_TRUE);
        xset_set_panel(panel, "sort_extra", "z", val);
    }
    g_free(val);
    ptk_file_list_sort(list);
}

void
ptk_file_browser_set_sort_order(PtkFileBrowser* file_browser, PtkFBSortOrder order)
{
    if (order == file_browser->sort_order)
        return;

    file_browser->sort_order = order;
    int col = file_list_order_from_sort_order(order);

    if (file_browser->file_list)
    {
        gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(file_browser->file_list),
                                             col,
                                             file_browser->sort_type);
    }
}

void
ptk_file_browser_set_sort_type(PtkFileBrowser* file_browser, GtkSortType order)
{
    if (order != file_browser->sort_type)
    {
        file_browser->sort_type = order;
        if (file_browser->file_list)
        {
            int col;
            GtkSortType old_order;
            gtk_tree_sortable_get_sort_column_id(GTK_TREE_SORTABLE(file_browser->file_list),
                                                 &col,
                                                 &old_order);
            gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(file_browser->file_list),
                                                 col,
                                                 order);
        }
    }
}

/* FIXME: Don't recreate the view if previous view is compact view */
void
ptk_file_browser_view_as_icons(PtkFileBrowser* file_browser)
{
    if (file_browser->view_mode == PTK_FB_ICON_VIEW && file_browser->folder_view)
        return;

    show_thumbnails(file_browser,
                    PTK_FILE_LIST(file_browser->file_list),
                    true,
                    file_browser->max_thumbnail);

    file_browser->view_mode = PTK_FB_ICON_VIEW;
    if (file_browser->folder_view)
        gtk_widget_destroy(file_browser->folder_view);
    file_browser->folder_view = create_folder_view(file_browser, PTK_FB_ICON_VIEW);
    exo_icon_view_set_model(EXO_ICON_VIEW(file_browser->folder_view), file_browser->file_list);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_browser->folder_view_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_widget_show(file_browser->folder_view);
    gtk_container_add(GTK_CONTAINER(file_browser->folder_view_scroll), file_browser->folder_view);
}

/* FIXME: Don't recreate the view if previous view is icon view */
void
ptk_file_browser_view_as_compact_list(PtkFileBrowser* file_browser)
{
    if (file_browser->view_mode == PTK_FB_COMPACT_VIEW && file_browser->folder_view)
        return;

    show_thumbnails(file_browser,
                    PTK_FILE_LIST(file_browser->file_list),
                    file_browser->large_icons,
                    file_browser->max_thumbnail);

    file_browser->view_mode = PTK_FB_COMPACT_VIEW;
    if (file_browser->folder_view)
        gtk_widget_destroy(file_browser->folder_view);
    file_browser->folder_view = create_folder_view(file_browser, PTK_FB_COMPACT_VIEW);
    exo_icon_view_set_model(EXO_ICON_VIEW(file_browser->folder_view), file_browser->file_list);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_browser->folder_view_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_widget_show(file_browser->folder_view);
    gtk_container_add(GTK_CONTAINER(file_browser->folder_view_scroll), file_browser->folder_view);
}

void
ptk_file_browser_view_as_list(PtkFileBrowser* file_browser)
{
    if (file_browser->view_mode == PTK_FB_LIST_VIEW && file_browser->folder_view)
        return;

    show_thumbnails(file_browser,
                    PTK_FILE_LIST(file_browser->file_list),
                    file_browser->large_icons,
                    file_browser->max_thumbnail);

    file_browser->view_mode = PTK_FB_LIST_VIEW;
    if (file_browser->folder_view)
        gtk_widget_destroy(file_browser->folder_view);
    file_browser->folder_view = create_folder_view(file_browser, PTK_FB_LIST_VIEW);
    gtk_tree_view_set_model(GTK_TREE_VIEW(file_browser->folder_view), file_browser->file_list);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_browser->folder_view_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_ALWAYS);
    gtk_widget_show(file_browser->folder_view);
    gtk_container_add(GTK_CONTAINER(file_browser->folder_view_scroll), file_browser->folder_view);
}

unsigned int
ptk_file_browser_get_n_sel(PtkFileBrowser* file_browser, uint64_t* sel_size)
{
    if (sel_size)
        *sel_size = file_browser->sel_size;
    return file_browser->n_sel_files;
}

static void
ptk_file_browser_before_chdir(PtkFileBrowser* file_browser, const char* path, bool* cancel)
{
}

static void
ptk_file_browser_after_chdir(PtkFileBrowser* file_browser)
{
}

static void
ptk_file_browser_content_change(PtkFileBrowser* file_browser)
{
}

static void
ptk_file_browser_sel_change(PtkFileBrowser* file_browser)
{
}

static void
ptk_file_browser_pane_mode_change(PtkFileBrowser* file_browser)
{
}

static void
ptk_file_browser_open_item(PtkFileBrowser* file_browser, const char* path, int action)
{
}

static void
show_thumbnails(PtkFileBrowser* file_browser, PtkFileList* list, bool is_big, int max_file_size)
{
    /* This function collects all calls to ptk_file_list_show_thumbnails()
     * and disables them if change detection is blacklisted on current device */
    if (!(file_browser && file_browser->dir))
        max_file_size = 0;
    else if (file_browser->dir->avoid_changes)
        max_file_size = 0;
    ptk_file_list_show_thumbnails(list, is_big, max_file_size);
    ptk_file_browser_update_toolbar_widgets(file_browser, nullptr, XSET_TOOL_SHOW_THUMB);
}

void
ptk_file_browser_show_thumbnails(PtkFileBrowser* file_browser, int max_file_size)
{
    file_browser->max_thumbnail = max_file_size;
    if (file_browser->file_list)
    {
        show_thumbnails(file_browser,
                        PTK_FILE_LIST(file_browser->file_list),
                        file_browser->large_icons,
                        max_file_size);
    }
}

void
ptk_file_browser_emit_open(PtkFileBrowser* file_browser, const char* path, PtkOpenAction action)
{
    g_signal_emit(file_browser, signals[OPEN_ITEM_SIGNAL], 0, path, action);
}

void
ptk_file_browser_set_single_click(PtkFileBrowser* file_browser, bool single_click)
{
    if (single_click == file_browser->single_click)
        return;

    switch (file_browser->view_mode)
    {
        case PTK_FB_ICON_VIEW:
        case PTK_FB_COMPACT_VIEW:
            exo_icon_view_set_single_click((ExoIconView*)file_browser->folder_view, single_click);
            break;
        case PTK_FB_LIST_VIEW:
            // gtk_tree_view_set_single_click((GtkTreeView*)file_browser->folder_view,
            // single_click);
            break;
        default:
            break;
    }

    file_browser->single_click = single_click;
}

void
ptk_file_browser_set_single_click_timeout(PtkFileBrowser* file_browser, unsigned int timeout)
{
    if (timeout == file_browser->single_click_timeout)
        return;

    switch (file_browser->view_mode)
    {
        case PTK_FB_ICON_VIEW:
        case PTK_FB_COMPACT_VIEW:
            exo_icon_view_set_single_click_timeout((ExoIconView*)file_browser->folder_view,
                                                   timeout);
            break;
        case PTK_FB_LIST_VIEW:
            // gtk_tree_view_set_single_click_timeout((GtkTreeView*)file_browser->folder_view,
            //                                       timeout);
            break;
        default:
            break;
    }

    file_browser->single_click_timeout = timeout;
}

////////////////////////////////////////////////////////////////////////////

int
ptk_file_browser_no_access(const char* cwd, const char* smode)
{
    int mode;
    if (!smode)
        mode = W_OK;
    else if (!strcmp(smode, "R_OK"))
        mode = R_OK;
    else
        mode = W_OK;

    return faccessat(0, cwd, mode, AT_EACCESS);
}

void
ptk_file_browser_focus(GtkMenuItem* item, PtkFileBrowser* file_browser, int job2)
{
    GtkWidget* widget;
    int job;
    if (item)
        job = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "job"));
    else
        job = job2;

    FMMainWindow* main_window = (FMMainWindow*)file_browser->main_window;
    int p = file_browser->mypanel;
    char mode = main_window->panel_context[p - 1];
    switch (job)
    {
        case 0:
            // path bar
            if (!xset_get_b_panel_mode(p, "show_toolbox", mode))
            {
                xset_set_b_panel_mode(p, "show_toolbox", mode, true);
                update_views_all_windows(nullptr, file_browser);
            }
            widget = file_browser->path_bar;
            break;
        case 1:
            if (!xset_get_b_panel_mode(p, "show_dirtree", mode))
            {
                xset_set_b_panel_mode(p, "show_dirtree", mode, true);
                update_views_all_windows(nullptr, file_browser);
            }
            widget = file_browser->side_dir;
            break;
        case 2:
            if (!xset_get_b_panel_mode(p, "show_book", mode))
            {
                xset_set_b_panel_mode(p, "show_book", mode, true);
                update_views_all_windows(nullptr, file_browser);
            }
            widget = file_browser->side_book;
            break;
        case 3:
            if (!xset_get_b_panel_mode(p, "show_devmon", mode))
            {
                xset_set_b_panel_mode(p, "show_devmon", mode, true);
                update_views_all_windows(nullptr, file_browser);
            }
            widget = file_browser->side_dev;
            break;
        case 4:
            widget = file_browser->folder_view;
            break;
        default:
            return;
    }
    if (gtk_widget_get_visible(widget))
        gtk_widget_grab_focus(GTK_WIDGET(widget));
}

static void
focus_folder_view(PtkFileBrowser* file_browser)
{
    gtk_widget_grab_focus(GTK_WIDGET(file_browser->folder_view));
    g_signal_emit(file_browser, signals[PANE_MODE_CHANGE_SIGNAL], 0);
}

void
ptk_file_browser_focus_me(PtkFileBrowser* file_browser)
{
    g_signal_emit(file_browser, signals[PANE_MODE_CHANGE_SIGNAL], 0);
}

void
ptk_file_browser_go_tab(GtkMenuItem* item, PtkFileBrowser* file_browser, int t)
{
    // printf( "ptk_file_browser_go_tab fb=%#x\n", file_browser );
    GtkWidget* notebook = file_browser->mynotebook;
    int tab_num;
    if (item)
        tab_num = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "tab_num"));
    else
        tab_num = t;

    switch (tab_num)
    {
        case -1:
            // prev
            if (gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)) == 0)
                gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook),
                                              gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook)) - 1);
            else
                gtk_notebook_prev_page(GTK_NOTEBOOK(notebook));
            break;
        case -2:
            // next
            if (gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)) + 1 ==
                gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook)))
                gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 0);
            else
                gtk_notebook_next_page(GTK_NOTEBOOK(notebook));
            break;
        case -3:
            // close
            on_close_notebook_page(nullptr, file_browser);
            break;
        default:
            if (tab_num <= gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook)) && tab_num > 0)
                gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), tab_num - 1);
            break;
    }
}

void
ptk_file_browser_open_in_tab(PtkFileBrowser* file_browser, int tab_num, char* file_path)
{
    int page_x;
    GtkWidget* notebook = file_browser->mynotebook;
    int cur_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
    int pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));

    switch (tab_num)
    {
        case -1:
            // prev
            page_x = cur_page - 1;
            break;
        case -2:
            // next
            page_x = cur_page + 1;
            break;
        default:
            page_x = tab_num - 1;
            break;
    }

    if (page_x > -1 && page_x < pages && page_x != cur_page)
    {
        PtkFileBrowser* a_browser =
            (PtkFileBrowser*)gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), page_x);

        ptk_file_browser_chdir(a_browser, file_path, PTK_FB_CHDIR_ADD_HISTORY);
    }
}

void
ptk_file_browser_on_permission(GtkMenuItem* item, PtkFileBrowser* file_browser, GList* sel_files,
                               char* cwd)
{
    char* name;
    const char* prog;
    bool as_root = false;
    const char* user1 = "1000";
    const char* user2 = "1001";
    char* myuser = g_strdup_printf("%d", geteuid());

    if (!sel_files)
        return;

    XSet* set = (XSet*)g_object_get_data(G_OBJECT(item), "set");
    if (!set || !file_browser)
        return;

    if (!strncmp(set->name, "perm_", 5))
    {
        name = set->name + 5;
        if (!strncmp(name, "go", 2) || !strncmp(name, "ugo", 3))
            prog = "chmod -R";
        else
            prog = "chmod";
    }
    else if (!strncmp(set->name, "rperm_", 6))
    {
        name = set->name + 6;
        if (!strncmp(name, "go", 2) || !strncmp(name, "ugo", 3))
            prog = "chmod -R";
        else
            prog = "chmod";
        as_root = true;
    }
    else if (!strncmp(set->name, "own_", 4))
    {
        name = set->name + 4;
        prog = "chown";
        as_root = true;
    }
    else if (!strncmp(set->name, "rown_", 5))
    {
        name = set->name + 5;
        prog = "chown -R";
        as_root = true;
    }
    else
        return;

    char* cmd;
    if (!strcmp(name, "r"))
        cmd = g_strdup_printf("u+r-wx,go-rwx");
    else if (!strcmp(name, "rw"))
        cmd = g_strdup_printf("u+rw-x,go-rwx");
    else if (!strcmp(name, "rwx"))
        cmd = g_strdup_printf("u+rwx,go-rwx");
    else if (!strcmp(name, "r_r"))
        cmd = g_strdup_printf("u+r-wx,g+r-wx,o-rwx");
    else if (!strcmp(name, "rw_r"))
        cmd = g_strdup_printf("u+rw-x,g+r-wx,o-rwx");
    else if (!strcmp(name, "rw_rw"))
        cmd = g_strdup_printf("u+rw-x,g+rw-x,o-rwx");
    else if (!strcmp(name, "rwxr_x"))
        cmd = g_strdup_printf("u+rwx,g+rx-w,o-rwx");
    else if (!strcmp(name, "rwxrwx"))
        cmd = g_strdup_printf("u+rwx,g+rwx,o-rwx");
    else if (!strcmp(name, "r_r_r"))
        cmd = g_strdup_printf("ugo+r,ugo-wx");
    else if (!strcmp(name, "rw_r_r"))
        cmd = g_strdup_printf("u+rw-x,go+r-wx");
    else if (!strcmp(name, "rw_rw_rw"))
        cmd = g_strdup_printf("ugo+rw-x");
    else if (!strcmp(name, "rwxr_r"))
        cmd = g_strdup_printf("u+rwx,go+r-wx");
    else if (!strcmp(name, "rwxr_xr_x"))
        cmd = g_strdup_printf("u+rwx,go+rx-w");
    else if (!strcmp(name, "rwxrwxrwx"))
        cmd = g_strdup_printf("ugo+rwx,-t");
    else if (!strcmp(name, "rwxrwxrwt"))
        cmd = g_strdup_printf("ugo+rwx,+t");
    else if (!strcmp(name, "unstick"))
        cmd = g_strdup_printf("-t");
    else if (!strcmp(name, "stick"))
        cmd = g_strdup_printf("+t");
    else if (!strcmp(name, "go_w"))
        cmd = g_strdup_printf("go-w");
    else if (!strcmp(name, "go_rwx"))
        cmd = g_strdup_printf("go-rwx");
    else if (!strcmp(name, "ugo_w"))
        cmd = g_strdup_printf("ugo+w");
    else if (!strcmp(name, "ugo_rx"))
        cmd = g_strdup_printf("ugo+rX");
    else if (!strcmp(name, "ugo_rwx"))
        cmd = g_strdup_printf("ugo+rwX");
    else if (!strcmp(name, "myuser"))
        cmd = g_strdup_printf("%s:%s", myuser, myuser);
    else if (!strcmp(name, "myuser_users"))
        cmd = g_strdup_printf("%s:users", myuser);
    else if (!strcmp(name, "user1"))
        cmd = g_strdup_printf("%s:%s", user1, user1);
    else if (!strcmp(name, "user1_users"))
        cmd = g_strdup_printf("%s:users", user1);
    else if (!strcmp(name, "user2"))
        cmd = g_strdup_printf("%s:%s", user2, user2);
    else if (!strcmp(name, "user2_users"))
        cmd = g_strdup_printf("%s:users", user2);
    else if (!strcmp(name, "root"))
        cmd = g_strdup_printf("root:root");
    else if (!strcmp(name, "root_users"))
        cmd = g_strdup_printf("root:users");
    else if (!strcmp(name, "root_myuser"))
        cmd = g_strdup_printf("root:%s", myuser);
    else if (!strcmp(name, "root_user1"))
        cmd = g_strdup_printf("root:%s", user1);
    else if (!strcmp(name, "root_user2"))
        cmd = g_strdup_printf("root:%s", user2);
    else
        return;

    char* file_paths = g_strdup("");
    GList* sel;
    char* file_path;
    char* str;
    for (sel = sel_files; sel; sel = sel->next)
    {
        file_path = bash_quote(vfs_file_info_get_name((VFSFileInfo*)sel->data));
        str = file_paths;
        file_paths = g_strdup_printf("%s %s", file_paths, file_path);
        g_free(str);
        g_free(file_path);
    }

    // task
    PtkFileTask* task =
        ptk_file_exec_new(set->menu_label, cwd, GTK_WIDGET(file_browser), file_browser->task_view);
    task->task->exec_command = g_strdup_printf("%s %s %s", prog, cmd, file_paths);
    g_free(cmd);
    g_free(file_paths);
    task->task->exec_browser = file_browser;
    task->task->exec_sync = true;
    task->task->exec_show_error = true;
    task->task->exec_show_output = false;
    task->task->exec_export = false;
    if (as_root)
        task->task->exec_as_user = g_strdup_printf("root");
    ptk_file_task_run(task);
}

void
ptk_file_browser_on_action(PtkFileBrowser* browser, char* setname)
{
    char* xname;
    int i = 0;
    XSet* set = xset_get(setname);
    FMMainWindow* main_window = (FMMainWindow*)browser->main_window;
    char mode = main_window->panel_context[browser->mypanel - 1];

    // printf("ptk_file_browser_on_action %s\n", set->name );

    if (g_str_has_prefix(set->name, "book_"))
    {
        xname = set->name + 5;
        if (!strcmp(xname, "icon") || !strcmp(xname, "menu_icon"))
            ptk_bookmark_view_update_icons(nullptr, browser);
        else if (!strcmp(xname, "add"))
        {
            const char* text = browser->path_bar && gtk_widget_has_focus(browser->path_bar)
                                   ? gtk_entry_get_text(GTK_ENTRY(browser->path_bar))
                                   : nullptr;
            if (text && (g_file_test(text, G_FILE_TEST_EXISTS) || strstr(text, ":/") ||
                         g_str_has_prefix(text, "//")))
                ptk_bookmark_view_add_bookmark(nullptr, browser, text);
            else
                ptk_bookmark_view_add_bookmark(nullptr, browser, nullptr);
        }
        else if (!strcmp(xname, "open") && browser->side_book)
            ptk_bookmark_view_on_open_reverse(nullptr, browser);
    }
    else if (g_str_has_prefix(set->name, "go_"))
    {
        xname = set->name + 3;
        if (!strcmp(xname, "back"))
            ptk_file_browser_go_back(nullptr, browser);
        else if (!strcmp(xname, "forward"))
            ptk_file_browser_go_forward(nullptr, browser);
        else if (!strcmp(xname, "up"))
            ptk_file_browser_go_up(nullptr, browser);
        else if (!strcmp(xname, "home"))
            ptk_file_browser_go_home(nullptr, browser);
        else if (!strcmp(xname, "default"))
            ptk_file_browser_go_default(nullptr, browser);
        else if (!strcmp(xname, "set_default"))
            ptk_file_browser_set_default_folder(nullptr, browser);
    }
    else if (g_str_has_prefix(set->name, "tab_"))
    {
        xname = set->name + 4;
        if (!strcmp(xname, "new"))
            ptk_file_browser_new_tab(nullptr, browser);
        else if (!strcmp(xname, "new_here"))
            ptk_file_browser_new_tab_here(nullptr, browser);
        else
        {
            if (!strcmp(xname, "prev"))
                i = -1;
            else if (!strcmp(xname, "next"))
                i = -2;
            else if (!strcmp(xname, "close"))
                i = -3;
            else
                i = strtol(xname, nullptr, 10);
            ptk_file_browser_go_tab(nullptr, browser, i);
        }
    }
    else if (g_str_has_prefix(set->name, "focus_"))
    {
        xname = set->name + 6;
        if (!strcmp(xname, "path_bar"))
            i = 0;
        else if (!strcmp(xname, "filelist"))
            i = 4;
        else if (!strcmp(xname, "dirtree"))
            i = 1;
        else if (!strcmp(xname, "book"))
            i = 2;
        else if (!strcmp(xname, "device"))
            i = 3;
        ptk_file_browser_focus(nullptr, browser, i);
    }
    else if (!strcmp(set->name, "view_reorder_col"))
        on_reorder(nullptr, GTK_WIDGET(browser));
    else if (!strcmp(set->name, "view_refresh"))
        ptk_file_browser_refresh(nullptr, browser);
    else if (!strcmp(set->name, "view_thumb"))
        main_window_toggle_thumbnails_all_windows();
    else if (g_str_has_prefix(set->name, "sortby_"))
    {
        xname = set->name + 7;
        i = -3;
        if (!strcmp(xname, "name"))
            i = PTK_FB_SORT_BY_NAME;
        else if (!strcmp(xname, "size"))
            i = PTK_FB_SORT_BY_SIZE;
        else if (!strcmp(xname, "type"))
            i = PTK_FB_SORT_BY_TYPE;
        else if (!strcmp(xname, "perm"))
            i = PTK_FB_SORT_BY_PERM;
        else if (!strcmp(xname, "owner"))
            i = PTK_FB_SORT_BY_OWNER;
        else if (!strcmp(xname, "date"))
            i = PTK_FB_SORT_BY_MTIME;
        else if (!strcmp(xname, "ascend"))
        {
            i = -1;
            set->b = browser->sort_type == GTK_SORT_ASCENDING ? XSET_B_TRUE : XSET_B_FALSE;
        }
        else if (!strcmp(xname, "descend"))
        {
            i = -2;
            set->b = browser->sort_type == GTK_SORT_DESCENDING ? XSET_B_TRUE : XSET_B_FALSE;
        }
        if (i > 0)
            set->b = browser->sort_order == i ? XSET_B_TRUE : XSET_B_FALSE;
        on_popup_sortby(nullptr, browser, i);
    }
    else if (g_str_has_prefix(set->name, "sortx_"))
        ptk_file_browser_set_sort_extra(browser, set->name);
    else if (!strcmp(set->name, "path_help"))
        ptk_path_entry_help(nullptr, GTK_WIDGET(browser));
    else if (g_str_has_prefix(set->name, "panel"))
    {
        i = 0;
        if (strlen(set->name) > 6)
        {
            xname = g_strdup(set->name + 5);
            xname[1] = '\0';
            i = strtol(xname, nullptr, 10);
            xname[1] = '_';
            g_free(xname);
        }
        // printf( "ACTION panelN=%d  %c\n", i, set->name[5] );
        if (i > 0 && i < 5)
        {
            XSet* set2;
            xname = set->name + 7;
            if (!strcmp(xname, "show_hidden")) // shared key
            {
                ptk_file_browser_show_hidden_files(
                    browser,
                    xset_get_b_panel(browser->mypanel, "show_hidden"));
            }
            else if (!strcmp(xname, "show")) // main View|Panel N
                show_panels_all_windows(nullptr, (FMMainWindow*)browser->main_window);
            else if (g_str_has_prefix(xname, "show_")) // shared key
            {
                set2 = xset_get_panel_mode(browser->mypanel, xname, mode);
                set2->b = set2->b == XSET_B_TRUE ? XSET_B_UNSET : XSET_B_TRUE;
                update_views_all_windows(nullptr, browser);
                if (!strcmp(xname, "show_book") && browser->side_book)
                {
                    ptk_bookmark_view_chdir(GTK_TREE_VIEW(browser->side_book), browser, true);
                    gtk_widget_grab_focus(GTK_WIDGET(browser->side_book));
                }
            }
            else if (!strcmp(xname, "list_detailed")) // shared key
                on_popup_list_detailed(nullptr, browser);
            else if (!strcmp(xname, "list_icons")) // shared key
                on_popup_list_icons(nullptr, browser);
            else if (!strcmp(xname, "list_compact")) // shared key
                on_popup_list_compact(nullptr, browser);
            else if (!strcmp(xname, "list_large")) // shared key
            {
                if (browser->view_mode != PTK_FB_ICON_VIEW)
                {
                    xset_set_b_panel(browser->mypanel, "list_large", !browser->large_icons);
                    on_popup_list_large(nullptr, browser);
                }
            }
            else if (g_str_has_prefix(xname, "detcol_") // shared key
                     && browser->view_mode == PTK_FB_LIST_VIEW)
            {
                set2 = xset_get_panel_mode(browser->mypanel, xname, mode);
                set2->b = set2->b == XSET_B_TRUE ? XSET_B_UNSET : XSET_B_TRUE;
                update_views_all_windows(nullptr, browser);
            }
        }
    }
    else if (g_str_has_prefix(set->name, "status_"))
    {
        xname = set->name + 7;
        if (!strcmp(xname, "border") || !strcmp(xname, "text"))
            on_status_effect_change(nullptr, browser);
        else if (!strcmp(xname, "name") || !strcmp(xname, "path") || !strcmp(xname, "info") ||
                 !strcmp(xname, "hide"))
            on_status_middle_click_config(nullptr, set);
    }
    else if (g_str_has_prefix(set->name, "paste_"))
    {
        xname = set->name + 6;
        if (!strcmp(xname, "link"))
            ptk_file_browser_paste_link(browser);
        else if (!strcmp(xname, "target"))
            ptk_file_browser_paste_target(browser);
        else if (!strcmp(xname, "as"))
            ptk_file_misc_paste_as(browser, ptk_file_browser_get_cwd(browser), nullptr);
    }
    else if (g_str_has_prefix(set->name, "select_"))
    {
        xname = set->name + 7;
        if (!strcmp(xname, "all"))
            ptk_file_browser_select_all(nullptr, browser);
        else if (!strcmp(xname, "un"))
            ptk_file_browser_unselect_all(nullptr, browser);
        else if (!strcmp(xname, "invert"))
            ptk_file_browser_invert_selection(nullptr, browser);
        else if (!strcmp(xname, "patt"))
            ptk_file_browser_select_pattern(nullptr, browser, nullptr);
    }
    else // all the rest require ptkfilemenu data
        ptk_file_menu_action(browser, set->name);
}
