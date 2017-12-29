#ifndef MELANGE_MAINWINDOW_H
#define MELANGE_MAINWINDOW_H

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

typedef struct MelangeMainWindow MelangeMainWindow;


GtkWidget *melange_main_window_new(GdkPixbuf *icon, WebKitWebContext *web_context);

GType melange_app_get_type(void);


#endif // MELANGE_MAINWINDOW_H
