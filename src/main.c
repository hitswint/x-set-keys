/***************************************************************************
 *
 * Copyright (C) 2017-2018 Tomoyuki KAWAO
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ***************************************************************************/

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <glib-unix.h>
#include <X11/Xlib.h>

#define MAIN
#include "common.h"
#include "x-set-keys.h"
#include "config.h"

typedef struct _Arguments_ {
  gchar *config_filepath;
  gchar *device_filepath;
  gchar **excluded_classes;
  gchar **excluded_fcitx_input_methods;
} _Arguments;

static volatile gboolean _caught_sigint = FALSE;
static volatile gboolean _caught_sigterm = FALSE;
static volatile gboolean _caught_sighup = FALSE;
static volatile gboolean _caught_sigusr1 = FALSE;
static volatile gboolean _error_occurred = FALSE;
static jmp_buf _xio_error_env;

static void _set_debug_flag();
static gboolean _parse_arguments(gint argc,
                                 gchar *argv[],
                                 _Arguments *arguments);
static void _free_arguments( _Arguments *arguments);
static gboolean _handle_signal(gpointer flag_pointer);
static gint _handle_x_error(Display *display, XErrorEvent *event);
static gint _handle_xio_error(Display *display);
static gboolean _run(const _Arguments *arguments);

gint main(gint argc, gchar *argv[])
{
  _Arguments arguments = { 0 };
  gint error_retry_count = 0;

  g_set_prgname(g_path_get_basename(argv[0]));
  _set_debug_flag();

  if (!_parse_arguments(argc, argv, &arguments)) {
    return EXIT_FAILURE;
  }

  g_unix_signal_add(SIGINT, _handle_signal, (gpointer)&_caught_sigint);
  g_unix_signal_add(SIGTERM, _handle_signal, (gpointer)&_caught_sigterm);
  g_unix_signal_add(SIGHUP, _handle_signal, (gpointer)&_caught_sighup);
  g_unix_signal_add(SIGUSR1, _handle_signal, (gpointer)&_caught_sigusr1);

  XSetErrorHandler(_handle_x_error);
  XSetIOErrorHandler(_handle_xio_error);

  while (_run(&arguments)) {
    if (_error_occurred) {
      if (++error_retry_count > 10) {
        g_critical("Maximum error retry count exceeded");
        break;
      }
      g_usleep(G_USEC_PER_SEC);
      _error_occurred = FALSE;
    } else {
      error_retry_count = 0;
    }
    g_message("Restarting");
  }

  g_message("Exiting");
  _free_arguments(&arguments);
  return _error_occurred ? EXIT_FAILURE : EXIT_SUCCESS;
}

void notify_error()
{
  _error_occurred = TRUE;
}

static void _set_debug_flag()
{
  const gchar *domains = g_getenv("G_MESSAGES_DEBUG");

  is_debug = domains != NULL && strcmp(domains, "all") == 0;
}

static gboolean _parse_arguments(gint argc,
                                 gchar *argv[],
                                 _Arguments *arguments)
{
  gboolean result = TRUE;
  GOptionEntry entries[] = {
    {
      "device-file", 'd', 0, G_OPTION_ARG_FILENAME,
      &arguments->device_filepath,
      "Keyboard device file", "<devicefile>"
    }, {
      "exclude-focus-class", 'e', 0, G_OPTION_ARG_STRING_ARRAY,
      &arguments->excluded_classes,
      "Exclude class of input focus window (Can be specified multiple times)",
      "<classname>"
    }, {
      "exclude-fcitx-im", 'f', 0, G_OPTION_ARG_STRING_ARRAY,
      &arguments->excluded_fcitx_input_methods,
      "Exclude input method of fcitx (Can be specified multiple times)",
      "<inputmethod>"
    }, {
      NULL
    }
  };
  GError *error = NULL;
  GOptionContext *context = g_option_context_new("<configuration-file>");

  g_option_context_add_main_entries(context, entries, NULL);
  if (!g_option_context_parse(context, &argc, &argv, &error)) {
    gchar *help;

    g_critical("Option parsing failed: %s", error->message);
    g_error_free(error);

    help = g_option_context_get_help(context, TRUE, NULL);
    g_print("%s\n", help);
    g_free(help);
    result = FALSE;
  } else if (argc != 2) {
    gchar *help;

    if (argc < 2) {
      g_critical("Configuration file must be specified");
    } else {
      g_critical("Too many arguments");
    }

    help = g_option_context_get_help(context, TRUE, NULL);
    g_print("%s\n", help);
    g_free(help);
    result = FALSE;
  } else {
    arguments->config_filepath = argv[1];
  }
  g_option_context_free(context);
  return result;
}

static void _free_arguments( _Arguments *arguments)
{
  if (arguments->device_filepath) {
    g_free(arguments->device_filepath);
  }
  if (arguments->excluded_classes) {
    g_strfreev(arguments->excluded_classes);
  }
  if (arguments->excluded_fcitx_input_methods) {
    g_strfreev(arguments->excluded_fcitx_input_methods);
  }
}

static gboolean _handle_signal(gpointer flag_pointer)
{
  *((volatile gboolean *)flag_pointer) = TRUE;
  g_main_context_wakeup(NULL);
  return G_SOURCE_CONTINUE;
}

static gint _handle_x_error(Display *display, XErrorEvent *event)
{
  gchar message[256];

  XGetErrorText(display, event->error_code, message, sizeof (message));
  g_critical("X protocol error : %s on protocol request %d",
             message,
             event->request_code);
  _error_occurred = TRUE;
  return 0;
}

static gint _handle_xio_error(Display *display)
{
  g_critical("Connection lost to X server `%s'", DisplayString(display));
  longjmp(_xio_error_env, 1);
  return 0;
}

/* static gchar *_get_focus_config(XSetKeys *xsk, const _Arguments *arguments) */
/* { */
/*         /\* For only class *\/ */
/*         XClassHint focus_class_hints; */
/*         Window focus_root; */
/*         Window focus_parent; */
/*         Window *focus_children; */
/*         guint focus_nchildren; */

/*         Display *display = xsk_get_display(xsk); */
/*         WindowSystem *ws = xsk_get_window_system(xsk); */
/*         Window focus_window = ws->focus_window; */

/*         gchar *result; */

/*         for (;;) { */
/*                 if (XGetClassHint(display, focus_window, &focus_class_hints)) { */
/*                         break; */
/*                 } */
/*                 if (!XQueryTree(display, focus_window, &focus_root, &focus_parent, &focus_children, &focus_nchildren)) { */
/*                         print_error("XQueryTree failed focus_window=%lx", focus_window); */
/*                         return FALSE; */
/*                 } */
/*                 XFree(focus_children); */
/*                 if (focus_window == focus_root || focus_parent == focus_root) { */
/*                         g_critical("Can not get focus_window class for input focus focus_window"); */
/*                         return FALSE; */
/*                 } */
/*                 focus_window = focus_parent; */
/*         } */
/*         if (!g_strcmp0("Emacs", focus_class_hints.res_name) || !g_strcmp0("Emacs", focus_class_hints.res_class)) { */
/*                 result="/home/swint/bin/x-set-keys_emacs.conf"; */
/*         } */
/*         else { */
/*                 result=arguments->config_filepath; */
/*         } */

/*         XFree(focus_class_hints.res_name); */
/*         XFree(focus_class_hints.res_class); */

/*         return result; */
/* } */

static gboolean _run(const _Arguments *arguments)
{
  gboolean is_restart = FALSE;
  XSetKeys xsk = { 0 };
  /* gchar *old_config_filepath; */
  /* gchar *new_config_filepath = arguments->config_filepath; */

  if (setjmp(_xio_error_env)) {
    _error_occurred = TRUE;
    is_restart = FALSE;
  }

  if (!_error_occurred && !xsk_initialize(&xsk, arguments->excluded_classes)) {
    _error_occurred = TRUE;
    if (xsk_get_display(&xsk)) {
      is_restart = TRUE;
    }
  }
  if (!_error_occurred && !config_load(&xsk, arguments->config_filepath)) {
    _error_occurred = TRUE;
  }
  if (!_error_occurred && !xsk_start(&xsk,
                                     arguments->device_filepath,
                                     arguments->excluded_fcitx_input_methods)) {
    _error_occurred = TRUE;
  }

  if (!_error_occurred) {
    debug_print("Starting main loop");
    _caught_sighup = FALSE;
    while (!_caught_sigint &&
           !_caught_sigterm &&
           !_caught_sighup &&
           !_error_occurred) {
      g_main_context_iteration(NULL, TRUE);

      /* old_config_filepath = new_config_filepath; */
      /* new_config_filepath = _get_focus_config(&xsk, arguments); */

      /* /\* 根据当前窗口改变配置文件 *\/ */
      /* if (!_error_occurred && (old_config_filepath != new_config_filepath)) { */
      /*         xsk_mapping_changed(&xsk); */
      /*         /\* 根据当前窗口改变配置文件 *\/ */
      /*         if (!_error_occurred && */
      /*             !config_load(&xsk, new_config_filepath)) { */
      /*                 _error_occurred = TRUE; */
      /*         } */
      /* } */

      if (_caught_sigusr1 && !_error_occurred) {
        g_message("Keyboard mapping changed");
        xsk_mapping_changed(&xsk);
        if (!_error_occurred &&
            !config_load(&xsk, arguments->config_filepath)) {
          _error_occurred = TRUE;
        }
        _caught_sigusr1 = FALSE;
      }
    }
    is_restart = TRUE;
    if (_caught_sigint) {
      is_restart = FALSE;
      g_message("Caught SIGINT");
    }
    if (_caught_sigterm) {
      is_restart = FALSE;
      g_message("Caught SIGTERM");
    }
    if (_caught_sighup) {
      g_message("Caught SIGHUP");
    }
  }

  g_message(is_restart ? "Initiating restart" : "Initiating shutdown");

  if (setjmp(_xio_error_env)) {
    return FALSE;
  }

  xsk_finalize(&xsk, is_restart);
  return is_restart;
}
