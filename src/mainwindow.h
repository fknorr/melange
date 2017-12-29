#ifndef MELANGE_MAINWINDOW_H
#define MELANGE_MAINWINDOW_H

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>


typedef struct MelangeMainWindow MelangeMainWindow;

#define MELANGE_TYPE_MAIN_WINDOW (melange_main_window_get_type())
#define MELANGE_MAIN_WINDOW(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST((obj), MELANGE_TYPE_MAIN_WINDOW, MelangeMainWindow))


GtkWidget *melange_main_window_new(WebKitWebContext *web_context);

GType melange_app_get_type(void);


#endif // MELANGE_MAINWINDOW_H
