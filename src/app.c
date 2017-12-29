#include "app.h"
#include "mainwindow.h"


struct MelangeApp {
    GtkApplication parent_instance;

    GtkStatusIcon *status_icon;
    GtkWidget *status_menu;
    GtkWidget *main_window;
};

typedef GtkApplicationClass MelangeAppClass;

G_DEFINE_TYPE(MelangeApp, melange_app, GTK_TYPE_APPLICATION)


static void
melange_app_status_icon_activate(GtkStatusIcon *status_icon, MelangeApp *app) {
    (void) status_icon;

    if (gtk_widget_get_visible(app->main_window)) {
        gtk_widget_hide(app->main_window);
    } else {
        gtk_window_present(GTK_WINDOW(app->main_window));
    }
}


gboolean
melange_app_status_icon_button_press_event(GtkStatusIcon *status_icon, GdkEvent *event,
        MelangeApp *app)
{
    (void) status_icon;

    if (event->type == GDK_BUTTON_PRESS) {
        GdkEventButton *button_event = (GdkEventButton*) event;
        if (button_event->button == GDK_BUTTON_SECONDARY) {
            gtk_menu_popup_at_pointer(GTK_MENU(app->status_menu), event);
            return TRUE;
        }
    }
    return FALSE;
}


static gboolean
melange_app_main_window_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    (void) event;
    (void) user_data;

    gtk_widget_hide(widget);

    return TRUE; // inhibit destruction of window
}


static void
melange_app_startup(GApplication *g_app) {
    G_APPLICATION_CLASS(melange_app_parent_class)->startup(g_app);

    MelangeApp *app = MELANGE_APP(g_app);

    GdkPixbuf *icon = gdk_pixbuf_new_from_file("res/icons/melange.svg", NULL);
    app->status_icon = gtk_status_icon_new_from_pixbuf(icon);
    g_signal_connect(app->status_icon, "activate", G_CALLBACK(melange_app_status_icon_activate),
            app);
    g_signal_connect(app->status_icon, "button-press-event",
            G_CALLBACK(melange_app_status_icon_button_press_event), app);

    WebKitWebsiteDataManager *data_manager = webkit_website_data_manager_new(
            "base-data-directory", "/tmp/melange/data",
            "base-cache-directory", "/tmp/melange/cache",
            NULL);

    WebKitWebContext *web_context = webkit_web_context_new_with_website_data_manager(data_manager);
    webkit_web_context_set_cache_model(web_context, WEBKIT_CACHE_MODEL_WEB_BROWSER);

    WebKitSecurityOrigin *origin = webkit_security_origin_new_for_uri("https://web.whatsapp.com");
    GList *allowed_origins = g_list_append(NULL, origin);
    webkit_web_context_initialize_notification_permissions(web_context, allowed_origins, NULL);

    app->main_window = melange_main_window_new(icon, web_context);
    g_signal_connect_swapped(app->main_window, "destroy", G_CALLBACK(g_application_quit), app);
    g_signal_connect(app->main_window, "delete-event",
            G_CALLBACK(melange_app_main_window_delete_event), NULL);
    gtk_widget_show_all(app->main_window);
    gtk_application_add_window(GTK_APPLICATION(app), GTK_WINDOW(app->main_window));

    app->status_menu = gtk_menu_new();
    GtkWidget *menu_item = gtk_menu_item_new_with_label("Quit");
    g_signal_connect_swapped(menu_item, "activate", G_CALLBACK(gtk_widget_destroy),
            app->main_window);
    gtk_menu_shell_append(GTK_MENU_SHELL(app->status_menu), menu_item);
    gtk_widget_show_all(app->status_menu);
}


static void
melange_app_shutdown(GApplication *app) {
    G_APPLICATION_CLASS(melange_app_parent_class)->shutdown(app);
}


static void
melange_app_activate(GApplication *app) {
    G_APPLICATION_CLASS(melange_app_parent_class)->activate(app);
}


static void
melange_app_finalize(GObject *app) {
    G_OBJECT_CLASS(melange_app_parent_class)->finalize(app);
}


static void
melange_app_init(MelangeApp *app) {
    (void) app;
}


static void
melange_app_class_init(MelangeAppClass *class) {
    GApplicationClass *application_class = G_APPLICATION_CLASS(class);
    application_class->startup = melange_app_startup;
    application_class->shutdown = melange_app_shutdown;
    application_class->activate = melange_app_activate;

    GObjectClass *object_class = G_OBJECT_CLASS(class);
    object_class->finalize = melange_app_finalize;
}


GApplication *
melange_app_new(void) {
    return g_object_new(MELANGE_TYPE_APP,
            "application-id", "de.inforge.melange",
            "register-session", TRUE,
            NULL);
}
