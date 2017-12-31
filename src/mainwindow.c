#include "mainwindow.h"
#include "app.h"
#include "util.h"


struct MelangeMainWindow
{
    GtkApplicationWindow parent_instance;

    MelangeApp *app;

    GtkWidget *web_view;
    GtkWidget *add_view;
    GtkWidget *settings_view;
    GtkWidget *sidebar_revealer;
    GtkWidget *sidebar_handle;
    GtkWidget *view_stack;
    GtkWidget *switcher_box;
    GtkWidget *menu_box;

    guint sidebar_timeout;
};

typedef GtkApplicationWindowClass MelangeMainWindowClass;

enum {
    MELANGE_MAIN_WINDOW_PROP_APP = 1,
    MELANGE_MAIN_WINDOW_N_PROPS
};


G_DEFINE_TYPE(MelangeMainWindow, melange_main_window, GTK_TYPE_APPLICATION_WINDOW)


static void
melange_main_window_set_property(GObject *object, guint property_id, const GValue *value,
        GParamSpec *pspec)
{
    MelangeMainWindow *win = MELANGE_MAIN_WINDOW(object);
    switch (property_id) {
        case MELANGE_MAIN_WINDOW_PROP_APP:
            win->app = MELANGE_APP(g_value_get_pointer(value));
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
melange_main_window_set_sidebar_visible(MelangeMainWindow *win, gboolean visible) {
    if (gtk_revealer_get_child_revealed(GTK_REVEALER(win->sidebar_revealer)) != visible) {
        gtk_revealer_set_reveal_child(GTK_REVEALER(win->sidebar_revealer), visible);
    }
}


static gboolean
melange_main_window_hide_sidebar_callback(MelangeMainWindow *win) {
    if (WEBKIT_IS_WEB_VIEW(gtk_stack_get_visible_child(GTK_STACK(win->view_stack)))) {
        melange_main_window_set_sidebar_visible(win, FALSE);
    }
    win->sidebar_timeout = 0;
    return FALSE;
}


static void
melange_main_window_cancel_sidebar_timeout(MelangeMainWindow *win) {
    if (win->sidebar_timeout) {
        g_source_remove(win->sidebar_timeout);
        win->sidebar_timeout = 0;
    }
}


static void
melange_main_window_hide_sidebar_after_timeout(MelangeMainWindow *win, guint timeout) {
    gboolean auto_hide_sidebar;
    g_object_get(win->app, "auto-hide-sidebar", &auto_hide_sidebar, NULL);
    if (!auto_hide_sidebar) return;

    melange_main_window_cancel_sidebar_timeout(win);
    if (gtk_revealer_get_child_revealed(GTK_REVEALER(win->sidebar_revealer))) {
        win->sidebar_timeout = g_timeout_add(timeout,
                (GSourceFunc) melange_main_window_hide_sidebar_callback, win);
    }
}


GLADE_EVENT_HANDLER gboolean
melange_main_window_sidebar_enter_notify_event(GtkWidget *widget, GdkEvent *event,
        MelangeMainWindow *win)
{
    (void) widget;
    (void) event;

    melange_main_window_cancel_sidebar_timeout(win);
    melange_main_window_set_sidebar_visible(win, TRUE);
    return TRUE;
}


GLADE_EVENT_HANDLER gboolean
melange_main_window_sidebar_leave_notify_event(GtkWidget *widget, GdkEvent *event,
        MelangeMainWindow *win)
{
    (void) widget;
    (void) event;

    melange_main_window_hide_sidebar_after_timeout(win, 1000);
    return TRUE;
}


GLADE_EVENT_HANDLER gboolean
melange_main_window_dark_theme_setting_state_set(GtkSwitch *widget, gboolean state,
        MelangeMainWindow *win)
{
    (void) widget;
    g_object_set(win->app, "dark-theme", state, NULL);
    return FALSE;
}


GLADE_EVENT_HANDLER gboolean
melange_main_window_auto_hide_sidebar_setting_state_set(GtkSwitch *widget, gboolean state,
        MelangeMainWindow *win)
{
    (void) widget;
    g_object_set(win->app, "auto-hide-sidebar", state, NULL);
    return FALSE;
}


GLADE_EVENT_HANDLER void
melange_main_window_client_side_decorations_setting_changed(GtkComboBox *combo_box,
        MelangeMainWindow *win)
{
    g_object_set(win->app, "client-side-decorations", gtk_combo_box_get_active_id(combo_box), NULL);
}


static void
melange_main_window_realize(GtkWidget *widget) {
    GTK_WIDGET_CLASS(melange_main_window_parent_class)->realize(widget);

    MelangeMainWindow *win = MELANGE_MAIN_WINDOW(widget);
    melange_main_window_hide_sidebar_after_timeout(win, 3000);
    gtk_stack_set_visible_child(GTK_STACK(win->view_stack), win->web_view);
}


static void
melange_main_window_init(MelangeMainWindow *win) {
    win->sidebar_timeout = 0;

    GdkGeometry hints = { .min_width = 800, .min_height = 600 };
    gtk_window_set_geometry_hints(GTK_WINDOW(win), NULL, &hints, GDK_HINT_MIN_SIZE);
}


static void
melange_main_window_switch_to_view(GtkButton *button, GtkWidget *switch_to) {
    GtkStack *stack = GTK_STACK(gtk_widget_get_parent(switch_to));
    gtk_stack_set_visible_child(stack, switch_to);
}


static GtkWidget *
melange_main_window_create_switcher_button(const char *icon, GtkWidget *switch_to) {
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size(icon, 32, 32, NULL);
    GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
    GtkWidget *switcher = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(switcher), GTK_RELIEF_NONE);
    gtk_button_set_image(GTK_BUTTON(switcher), image);
    g_signal_connect(switcher, "clicked", G_CALLBACK(melange_main_window_switch_to_view),
            switch_to);
    return switcher;
}


static void
melange_main_window_app_notify_auto_hide_sidebar(GObject *app, GParamSpec *pspec,
        MelangeMainWindow *win) {
    (void) pspec;

    gboolean auto_hide_sidebar;
    g_object_get(app, "auto-hide-sidebar", &auto_hide_sidebar, NULL);
    gtk_widget_set_visible(win->sidebar_handle, auto_hide_sidebar);
}


static void
melange_main_window_app_notify_client_side_decorations(GObject *app, GParamSpec *pspec,
        MelangeMainWindow *win) {
    (void) pspec;

    gboolean client_side_decorations;
    g_object_get(app, "client-side-decorations", &client_side_decorations, NULL);
}


static void
melange_main_window_constructed(GObject *obj) {
    MelangeMainWindow *win = MELANGE_MAIN_WINDOW(obj);
    g_return_if_fail(win->app);

    GtkBuilder *builder = gtk_builder_new_from_file("res/ui/mainwindow.glade");
    gtk_builder_connect_signals(builder, win);

    GtkWidget *layout = GTK_WIDGET(gtk_builder_get_object(builder, "layout"));
    gtk_container_add(GTK_CONTAINER(win), layout);

    win->sidebar_revealer = GTK_WIDGET(gtk_builder_get_object(builder, "sidebar-revealer"));
    win->sidebar_handle = GTK_WIDGET(gtk_builder_get_object(builder, "sidebar-handle"));
    win->view_stack = GTK_WIDGET(gtk_builder_get_object(builder, "view-stack"));
    win->menu_box = GTK_WIDGET(gtk_builder_get_object(builder, "menu-box"));
    win->switcher_box = GTK_WIDGET(gtk_builder_get_object(builder, "switcher-box"));

    GtkImage *sidebar_handle = GTK_IMAGE(gtk_builder_get_object(builder, "sidebar-handle"));
    gtk_image_set_from_pixbuf(sidebar_handle,
            gdk_pixbuf_new_from_file_at_size("res/icons/vdots.svg", 4, -1, NULL));

    win->add_view = GTK_WIDGET(gtk_builder_get_object(builder, "add-view"));
    gtk_container_add(GTK_CONTAINER(win->view_stack), win->add_view);

    win->settings_view = GTK_WIDGET(gtk_builder_get_object(builder, "settings-view"));
    gtk_container_add(GTK_CONTAINER(win->view_stack), win->settings_view);

    GtkSwitch *dark_theme_setting = GTK_SWITCH(gtk_builder_get_object(builder,
            "dark-theme-setting"));
    GtkSwitch *auto_hide_sidebar_setting = GTK_SWITCH(gtk_builder_get_object(builder,
            "auto-hide-sidebar-setting"));
    GtkComboBox *client_side_decorations_setting = GTK_COMBO_BOX(gtk_builder_get_object(builder,
            "client-side-decorations-setting"));

    g_object_unref(builder);

    WebKitWebContext *web_context = melange_app_get_web_context(win->app);
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
    gtk_container_add(GTK_CONTAINER(win->view_stack), win->web_view);

    gtk_container_add(GTK_CONTAINER(win->switcher_box),
            melange_main_window_create_switcher_button("res/icons/melange.svg", win->web_view));
    gtk_container_add(GTK_CONTAINER(win->switcher_box),
            melange_main_window_create_switcher_button("res/icons/add.svg", win->add_view));
    gtk_container_add(GTK_CONTAINER(win->menu_box), melange_main_window_create_switcher_button(
            "res/icons/settings.svg", win->settings_view));

    gboolean dark_theme;
    g_object_get(win->app, "dark-theme", &dark_theme, NULL);
    gtk_switch_set_state(dark_theme_setting, dark_theme);

    gboolean auto_hide_sidebar;
    g_object_get(win->app, "auto-hide-sidebar", &auto_hide_sidebar, NULL);
    gtk_switch_set_state(dark_theme_setting, auto_hide_sidebar);
    gtk_widget_set_visible(win->sidebar_handle, auto_hide_sidebar);
    g_signal_connect(win->app, "notify::auto-hide-sidebar",
            G_CALLBACK(melange_main_window_app_notify_auto_hide_sidebar), win);

    const char *client_side_decorations;
    g_object_get(win->app, "client-side-decorations", &client_side_decorations, NULL);
    gtk_combo_box_set_active_id(client_side_decorations_setting, client_side_decorations);
    g_signal_connect(win->app, "notify::client-side-decorations",
            G_CALLBACK(melange_main_window_app_notify_client_side_decorations), win);

    gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(win), FALSE);
    if (gtk_application_prefers_app_menu(GTK_APPLICATION(win->app))) {
        GtkWidget *header_bar = gtk_header_bar_new();
        gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);
        gtk_window_set_titlebar(GTK_WINDOW(win), header_bar);
    }
}


static void
melange_main_window_finalize(GObject *obj) {
    MelangeMainWindow *win = MELANGE_MAIN_WINDOW(obj);
    g_signal_handlers_disconnect_by_data(win->app, win);
}


static void
melange_main_window_class_init(MelangeMainWindowClass *cls) {
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(cls);
    widget_class->realize = melange_main_window_realize;

    GObjectClass *object_class = G_OBJECT_CLASS(cls);
    object_class->set_property = melange_main_window_set_property;
    object_class->constructed = melange_main_window_constructed;
    object_class->finalize = melange_main_window_finalize;

    GParamSpec *app_property_spec = g_param_spec_pointer("app", "app-context", "app",
            G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK
                    | G_PARAM_STATIC_BLURB);
    g_object_class_install_property(G_OBJECT_CLASS(cls), MELANGE_MAIN_WINDOW_PROP_APP,
            app_property_spec);
}


GtkWidget *
melange_main_window_new(MelangeApp *app) {
    g_return_val_if_fail(MELANGE_IS_APP(app), NULL);

    return g_object_new(melange_main_window_get_type(),
            "application", app,
            "app", app,
            NULL);
}
