#include "mainwindow.h"


struct MelangeMainWindow
{
    GtkWindow parent_instance;

    WebKitWebContext *web_context;
    GtkWidget *web_view;
};

typedef GtkWindowClass MelangeMainWindowClass;

enum {
    MELANGE_MAIN_WINDOW_PROP_WEB_CONTEXT = 1,
    MELANGE_MAIN_WINDOW_N_PROPS
};


G_DEFINE_TYPE(MelangeMainWindow, melange_main_window, GTK_TYPE_WINDOW)


static void
melange_main_window_set_property(GObject *object, guint property_id, const GValue *value,
        GParamSpec *pspec)
{
    MelangeMainWindow *win = MELANGE_MAIN_WINDOW(object);
    switch (property_id) {
        case MELANGE_MAIN_WINDOW_PROP_WEB_CONTEXT:
            win->web_context = g_value_get_pointer(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
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


static void
melange_main_window_init(MelangeMainWindow *win) {
    (void) win;
}


static void
melange_main_window_constructed(GObject *obj) {
    MelangeMainWindow *win = MELANGE_MAIN_WINDOW(obj);
    g_return_if_fail(win->web_context);

    win->web_view = webkit_web_view_new_with_context(win->web_context);
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

    GdkGeometry hints = { .min_width = 800, .min_height = 600, };
    gtk_window_set_geometry_hints(GTK_WINDOW(win), NULL, &hints, GDK_HINT_MIN_SIZE);
    gtk_container_add(GTK_CONTAINER(win), win->web_view);
}


static void
melange_main_window_class_init(MelangeMainWindowClass *cls) {
    GObjectClass *object_class = G_OBJECT_CLASS(cls);
    object_class->set_property = melange_main_window_set_property;
    object_class->constructed = melange_main_window_constructed;

    GParamSpec *web_context_spec = g_param_spec_pointer("web-context", "web-context",
            "WebKitWebContext to use", G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE
                    | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB);
    g_object_class_install_property(G_OBJECT_CLASS(cls), MELANGE_MAIN_WINDOW_PROP_WEB_CONTEXT,
            web_context_spec);
}


GtkWidget *
melange_main_window_new(WebKitWebContext *web_context) {
    return g_object_new(melange_main_window_get_type(),
            "web-context", web_context,
            NULL);
}
