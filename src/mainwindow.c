#include "mainwindow.h"


struct MelangeMainWindow
{
    GtkWindow parent_instance;

    GtkWidget *web_view;
};

typedef GtkWindowClass MelangeMainWindowClass;

G_DEFINE_TYPE(MelangeMainWindow, melange_main_window, GTK_TYPE_WINDOW)


static void
melange_main_window_init(MelangeMainWindow *win) {
    (void) win;

    GdkGeometry hints = { .min_width = 800, .min_height = 600, };
    gtk_window_set_geometry_hints(GTK_WINDOW(win), NULL, &hints, GDK_HINT_MIN_SIZE);
}


static void
melange_main_window_class_init(MelangeMainWindowClass *cls) {
    (void) cls;
}


static gboolean
melange_main_window_web_view_context_menu(WebKitWebView *web_view, WebKitContextMenu *context_menu,
        GdkEvent *event,
        WebKitHitTestResult *hit_test_result, gpointer user_data) {
    (void) web_view;
    (void) context_menu;
    (void) event;
    (void) hit_test_result;
    (void) user_data;

    return TRUE;
}


static void
melange_main_window_web_view_notify_title(GObject *gobject, GParamSpec *pspec,
        MelangeMainWindow *win) {
    (void) gobject;
    (void) pspec;

    gtk_window_set_title(GTK_WINDOW(win),
            webkit_web_view_get_title(WEBKIT_WEB_VIEW(win->web_view)));
}


static void
melange_main_window_web_view_load_changed(WebKitWebView *web_view, WebKitLoadEvent load_event,
        gpointer user_data) {
    (void) user_data;

    if (load_event == WEBKIT_LOAD_FINISHED) {
        char *override_js;
        if (g_file_get_contents("res/js/whatsapp.js", &override_js, NULL, NULL)) {
            webkit_web_view_run_javascript(web_view, override_js, NULL, NULL, NULL);
            g_free(override_js);
        }
    }
}


GtkWidget *
melange_main_window_new(GdkPixbuf *icon, WebKitWebContext *web_context) {
    MelangeMainWindow *win = g_object_new(melange_main_window_get_type(), NULL);
    gtk_window_set_icon(GTK_WINDOW(win), icon);
    gtk_window_set_title(GTK_WINDOW(win), "Melange");

    win->web_view = webkit_web_view_new_with_context(web_context);
    g_signal_connect(win->web_view, "context-menu",
            G_CALLBACK(melange_main_window_web_view_context_menu), NULL);
    g_signal_connect(win->web_view, "notify::title",
            G_CALLBACK(melange_main_window_web_view_notify_title), win);
    g_signal_connect(win->web_view, "load-changed",
            G_CALLBACK(melange_main_window_web_view_load_changed), NULL);

    WebKitSettings *sett = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(win->web_view));
    webkit_settings_set_user_agent(sett, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
            "(KHTML, like Gecko) Chrome/63.0.3239.108 Safari/537.36");

    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(win->web_view), "https://web.whatsapp.com");
    gtk_container_add(GTK_CONTAINER(win), win->web_view);
    return GTK_WIDGET(win);
}
