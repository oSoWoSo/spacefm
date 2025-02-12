/*
 *  C Implementation: vfs-execute
 *
 * Description:
 *
 *
 * Author: Hong Jen Yee (PCMan) <pcman.tw (AT) gmail.com>, (C) 2006
 *
 * Copyright: See COPYING file that comes with this distribution
 *
 */

#ifdef HAVE_SN

#include <ctime>

/* FIXME: Startup notification may cause problems */
#define SN_API_NOT_YET_FROZEN
#include <libsn/sn-launcher.h>
#include <X11/Xatom.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <time.h>
#endif

#include "vfs/vfs-execute.hxx"

#include "logger.hxx"

#ifdef HAVE_SN
static bool
sn_timeout(void* user_data)
{
    SnLauncherContext* ctx = (SnLauncherContext*)user_data;
    /* FIXME: startup notification, is this correct? */
    sn_launcher_context_complete(ctx);
    sn_launcher_context_unref(ctx);
    return false;
}

/* This function is taken from the code of thunar, written by Benedikt Meurer <benny@xfce.org> */
static int
tvsn_get_active_workspace_number(GdkScreen* screen)
{
    GdkWindow* root;
    unsigned long bytes_after_ret = 0;
    unsigned long nitems_ret = 0;
    unsigned int* prop_ret = nullptr;
    Atom _NET_CURRENT_DESKTOP;
    Atom _WIN_WORKSPACE;
    Atom type_ret = None;
    int format_ret;
    int ws_num = 0;

    gdk_error_trap_push();

    root = gdk_screen_get_root_window(screen);

    /* determine the X atom values */
    _NET_CURRENT_DESKTOP = XInternAtom(GDK_WINDOW_XDISPLAY(root), "_NET_CURRENT_DESKTOP", False);
    _WIN_WORKSPACE = XInternAtom(GDK_WINDOW_XDISPLAY(root), "_WIN_WORKSPACE", False);

    if (XGetWindowProperty(GDK_WINDOW_XDISPLAY(root),
                           GDK_WINDOW_XID(root),
                           _NET_CURRENT_DESKTOP,
                           0,
                           32,
                           False,
                           XA_CARDINAL,
                           &type_ret,
                           &format_ret,
                           &nitems_ret,
                           &bytes_after_ret,
                           (void*)&prop_ret) != Success)
    {
        if (XGetWindowProperty(GDK_WINDOW_XDISPLAY(root),
                               GDK_WINDOW_XID(root),
                               _WIN_WORKSPACE,
                               0,
                               32,
                               False,
                               XA_CARDINAL,
                               &type_ret,
                               &format_ret,
                               &nitems_ret,
                               &bytes_after_ret,
                               (void*)&prop_ret) != Success)
        {
            if (G_UNLIKELY(prop_ret != nullptr))
            {
                XFree(prop_ret);
                prop_ret = nullptr;
            }
        }
    }

    if (G_LIKELY(prop_ret != nullptr))
    {
        if (G_LIKELY(type_ret != None && format_ret != 0))
            ws_num = *prop_ret;
        XFree(prop_ret);
    }

    int err = gdk_error_trap_pop();

    return ws_num;
}
#endif

bool
vfs_exec_on_screen(GdkScreen* screen, const char* work_dir, char** argv, char** envp,
                   const char* disp_name, GSpawnFlags flags, bool use_startup_notify, GError** err)
{
#ifdef HAVE_SN
    SnLauncherContext* ctx = nullptr;
    SnDisplay* display;
    int startup_id_index = -1;
#else
    (void)disp_name;
    (void)use_startup_notify;
#endif
    extern char** environ;
    int display_index = -1;

    if (!envp)
        envp = environ;

    int n_env = g_strv_length(envp);

    char** new_env = g_new0(char*, n_env + 4);
    int i;
    for (i = 0; i < n_env; ++i)
    {
        // LOG_DEBUG("old envp[{}] = '{}'" , i, envp[i]);
        if (!strncmp(envp[i], "DISPLAY=", 8))
            display_index = i;
        else
        {
#ifdef HAVE_SN
            if (!strncmp(envp[i], "DESKTOP_STARTUP_ID=", 19))
                startup_id_index = i;
#endif
            new_env[i] = g_strdup(envp[i]);
        }
    }

#ifdef HAVE_SN
    if (use_startup_notify)
        display = sn_display_new(GDK_SCREEN_XDISPLAY(screen),
                                 (SnDisplayErrorTrapPush)gdk_error_trap_push,
                                 (SnDisplayErrorTrapPush)gdk_error_trap_pop);
    else
        display = nullptr;

    if (G_LIKELY(display))
    {
        if (!disp_name)
            disp_name = argv[0];

        ctx = sn_launcher_context_new(display, gdk_screen_get_number(screen));

        sn_launcher_context_set_description(ctx, disp_name);
        sn_launcher_context_set_name(ctx, g_get_prgname());
        sn_launcher_context_set_binary_name(ctx, argv[0]);

        sn_launcher_context_set_workspace(ctx, tvsn_get_active_workspace_number(screen));

        /* FIXME: I don't think this is correct, other people seem to use CurrentTime here.
                  However, using CurrentTime causes problems, so I so it like this.
                  Maybe this is incorrect, but it works, so, who cares?
        */
        /* std::time( &cur_time ); */
        sn_launcher_context_initiate(ctx,
                                     g_get_prgname(),
                                     argv[0],
                                     gtk_get_current_event_time() /*cur_time*/);

        GSpawnChildSetupFunc setup_func = nullptr;

        setup_func = (GSpawnChildSetupFunc)sn_launcher_context_setup_child_process;
        if (startup_id_index >= 0)
            g_free(new_env[i]);
        else
            startup_id_index = i++;
        new_env[startup_id_index] =
            g_strconcat("DESKTOP_STARTUP_ID=", sn_launcher_context_get_startup_id(ctx), nullptr);
    }
#endif

    /* This is taken from gdk_spawn_on_screen */
    char* display_name = gdk_screen_make_display_name(screen);
    if (display_index >= 0)
        new_env[display_index] = g_strconcat("DISPLAY=", display_name, nullptr);
    else
        new_env[i++] = g_strconcat("DISPLAY=", display_name, nullptr);

    g_free(display_name);
    new_env[i] = nullptr;

    bool ret = g_spawn_async(work_dir, argv, new_env, flags, nullptr, nullptr, nullptr, err);

    /* for debugging */
#if 0
    LOG_DEBUG("debug vfs_execute_on_screen(): flags: {}, display_index={}", flags, display_index );
    for( i = 0; argv[i]; ++i ) {
        LOG_DEBUG("argv[{}] = '{}'" , i, argv[i]);
    }
    for( i = 0; i < n_env /*new_env[i]*/; ++i ) {
        LOG_DEBUG("new_env[{}] = '{}'" , i, new_env[i]);
    }
    if( ret )
        LOG_DEBUG("the program was executed without error");
    else
        LOG_DEBUG("launch failed: '{}'", (*err)->message);
#endif

    g_strfreev(new_env);

#ifdef HAVE_SN
    if (G_LIKELY(ctx))
    {
        if (G_LIKELY(ret))
            g_timeout_add(20 * 1000, sn_timeout, ctx);
        else
        {
            sn_launcher_context_complete(ctx);
            sn_launcher_context_unref(ctx);
        }
    }

    if (G_LIKELY(display))
        sn_display_unref(display);
#endif

    return ret;
}
