#include "app.h"
#include "util.h"
#include "presets.h"
#include "mainwindow.h"


struct MelangeApp {
    GtkApplication parent_instance;

    GtkStatusIcon *status_icon;
    GtkWidget *status_menu;
    GtkWidget *main_window;

    WebKitWebContext *web_context;

    MelangeConfig *config;
    char *config_file_name;

    char *icon_cache_dir;
    GHashTable *icon_table;
};

typedef GtkApplicationClass MelangeAppClass;

enum {
    MELANGE_APP_PROP_DARK_THEME = 1,
    MELANGE_APP_PROP_CLIENT_SIDE_DECORATIONS,
    MELANGE_APP_PROP_AUTO_HIDE_SIDEBAR,
    MELANGE_APP_N_PROPS
};

typedef struct MelangeAppIconDownloadContext {
    MelangeApp *app;
    const MelangeAccount *preset;
    gboolean failed;
} MelangeAppIconDownloadContext;


G_DEFINE_TYPE(MelangeApp, melange_app, GTK_TYPE_APPLICATION)


static void
melange_app_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    MelangeApp *app = MELANGE_APP(object);
    switch (property_id) {
        case MELANGE_APP_PROP_DARK_THEME:
            g_value_set_boolean(value, app->config->dark_theme);
            break;

        case MELANGE_APP_PROP_AUTO_HIDE_SIDEBAR:
            g_value_set_boolean(value, app->config->auto_hide_sidebar);
            break;

        case MELANGE_APP_PROP_CLIENT_SIDE_DECORATIONS:
            switch (app->config->client_side_decorations) {
                case MELANGE_CSD_OFF: g_value_set_string(value, "off"); break;
                case MELANGE_CSD_ON: g_value_set_string(value, "on"); break;
                case MELANGE_CSD_AUTO: g_value_set_string(value, "auto"); break;
            }
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}


static void
melange_app_set_property(GObject *object, guint property_id, const GValue *value,
        GParamSpec *pspec)
{
    MelangeApp *app = MELANGE_APP(object);
    switch (property_id) {
        case MELANGE_APP_PROP_DARK_THEME:
            app->config->dark_theme = g_value_get_boolean(value);
            g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme",
                    app->config->dark_theme, NULL);
            break;

        case MELANGE_APP_PROP_AUTO_HIDE_SIDEBAR:
            app->config->auto_hide_sidebar = g_value_get_boolean(value);
            break;

        case MELANGE_APP_PROP_CLIENT_SIDE_DECORATIONS: {
            const char *str_value = g_value_get_string(value);
            if (g_str_equal(str_value, "off")) {
                app->config->client_side_decorations = MELANGE_CSD_OFF;
            } else if (g_str_equal(str_value, "on")) {
                app->config->client_side_decorations = MELANGE_CSD_ON;
            } else if (g_str_equal(str_value, "auto")) {
                app->config->client_side_decorations = MELANGE_CSD_AUTO;
            } else {
                g_warning("Invalid value for property client-side-decorations: %s", str_value);
            }
            break;
        }

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            return;
    }

    melange_config_write_to_file(app->config, app->config_file_name);
}


GLADE_EVENT_HANDLER void
melange_app_status_icon_activate(GtkStatusIcon *status_icon, MelangeApp *app) {
    (void) status_icon;

    if (gtk_widget_get_visible(app->main_window)) {
        gtk_widget_hide(app->main_window);
    } else {
        gtk_window_present(GTK_WINDOW(app->main_window));
    }
}


GLADE_EVENT_HANDLER void
melange_app_status_menu_quit_item_activate(GtkMenuItem *menu_item, MelangeApp *app) {
    (void) menu_item;

    g_application_quit(G_APPLICATION(app));
}


GLADE_EVENT_HANDLER gboolean
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


static gboolean
melange_app_decide_icon_destination(WebKitDownload *download, gchar *suggested_filename,
                                    MelangeAppIconDownloadContext *context)
{
    (void) suggested_filename;

    char *uri = g_strdup_printf("file://%s/%s.ico", context->app->icon_cache_dir,
                                context->preset->preset);
    webkit_download_set_destination(download, uri);
    g_free(uri);

    return TRUE;
}


static void
melange_app_icon_download_failed(WebKitDownload *download, GError *error, MelangeAppIconDownloadContext *context) {
    (void) download;
    (void) error;

    g_warning("Unable to download favicon from %s: %s", context->preset->icon_url, error->message);
    context->failed = TRUE;
}


static void
melange_app_icon_download_finished(WebKitDownload *download, MelangeAppIconDownloadContext *context) {
    (void) download;

    if (context->failed) goto cleanup;

    char *file_name = g_strdup_printf("%s/%s.ico", context->app->icon_cache_dir,
                                      context->preset->preset);

    GError *error = NULL;
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size(file_name, 32, 32, &error);
    if (pixbuf) {
        g_hash_table_insert(context->app->icon_table, (gpointer) context->preset->preset, pixbuf);
    } else {
        g_warning("Unable to load favicon file %s: %s", file_name, error->message);
        g_error_free(error);
    }

    g_free(file_name);

cleanup:
    g_free(context);
}


static void
melange_app_start_updating_icons(MelangeApp *app) {
    g_mkdir_with_parents(app->icon_cache_dir, 0777);

    for (size_t i = 0; i < melange_n_account_presets; ++i) {
        const MelangeAccount *preset = &melange_account_presets[i];

        char *file_name = g_strdup_printf("%s/%s.ico", app->icon_cache_dir, preset->preset);
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size(file_name, 32, 32, NULL);
        g_free(file_name);

        if (pixbuf) {
            g_hash_table_insert(app->icon_table, (gpointer) preset->preset, pixbuf);
            continue;
        }

        WebKitDownload *download = webkit_web_context_download_uri(app->web_context,
                                                                   preset->icon_url);

        MelangeAppIconDownloadContext context_template = {
            .app = app,
            .preset = preset,
            .failed = FALSE,
        };

        MelangeAppIconDownloadContext *context = g_memdup(&context_template, sizeof context_template);

        g_signal_connect(download, "decide-destination", G_CALLBACK(melange_app_decide_icon_destination), context);
        g_signal_connect(download, "failed", G_CALLBACK(melange_app_icon_download_failed), context);
        g_signal_connect(download, "finished", G_CALLBACK(melange_app_icon_download_finished), context);
    }
}


GdkPixbuf *
melange_app_request_icon(MelangeApp *app, const char *hostname) {
    GdkPixbuf *lookup = g_hash_table_lookup(app->icon_table, hostname);
    return lookup;
}


gboolean
melange_app_add_account(MelangeApp *app, MelangeAccount *account) {
    if (melange_config_add_account(app->config, account)) {
        melange_config_write_to_file(app->config, app->config_file_name);
        return TRUE;
    } else {
        return FALSE;
    }
}


void
melange_app_iterate_accounts(MelangeApp *app, MelangeAccountConstFunc func, gpointer user_data) {
    melange_config_for_each_account(app->config, (MelangeAccountFunc) func, user_data);
}


static void
melange_app_startup(GApplication *g_app) {
    G_APPLICATION_CLASS(melange_app_parent_class)->startup(g_app);

    MelangeApp *app = MELANGE_APP(g_app);

    app->config = melange_config_new_from_file(app->config_file_name);
    if (!app->config) {
        app->config = melange_config_new();
    }

    GtkBuilder *builder = gtk_builder_new_from_file("res/ui/app.glade");
    gtk_builder_connect_signals(builder, app);

    GMenu *app_menu = g_menu_new();
    g_menu_append(app_menu, "About", NULL);
    gtk_application_set_app_menu(GTK_APPLICATION(app), G_MENU_MODEL(app_menu));

    app->status_icon = GTK_STATUS_ICON(gtk_builder_get_object(builder, "status-icon"));
    g_object_ref(app->status_icon);

    app->status_menu = GTK_WIDGET(gtk_builder_get_object(builder, "status-menu"));

    g_object_unref(builder);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    GdkPixbuf *icon = gdk_pixbuf_new_from_file("res/icons/melange.svg", NULL);
    gtk_status_icon_set_from_pixbuf(app->status_icon, icon);
    gtk_status_icon_set_visible(app->status_icon, TRUE);

#pragma GCC diagnostic pop

    app->web_context = webkit_web_context_new_ephemeral();
    melange_app_start_updating_icons(app);

    app->main_window = melange_main_window_new(app);
    gtk_window_set_icon(GTK_WINDOW(app->main_window), icon);
    gtk_window_set_title(GTK_WINDOW(app->main_window), "Melange");
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
melange_app_finalize(GObject *g_app) {
    MelangeApp *app = MELANGE_APP(g_app);
    g_free(app->icon_cache_dir);
    g_hash_table_destroy(app->icon_table);
    g_free(app->config_file_name);

    G_OBJECT_CLASS(melange_app_parent_class)->finalize(g_app);
}


static void
melange_app_init(MelangeApp *app) {
    app->icon_cache_dir = g_strdup_printf("%s/melange/icons", g_get_user_cache_dir());
    app->icon_table = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_object_unref);
    app->config_file_name = g_strdup_printf("%s/melange/config", g_get_user_config_dir());
}


static void
melange_app_class_init(MelangeAppClass *cls) {
    GApplicationClass *application_class = G_APPLICATION_CLASS(cls);
    application_class->startup = melange_app_startup;
    application_class->shutdown = melange_app_shutdown;
    application_class->activate = melange_app_activate;

    GObjectClass *object_class = G_OBJECT_CLASS(cls);
    object_class->finalize = melange_app_finalize;
    object_class->get_property = melange_app_get_property;
    object_class->set_property = melange_app_set_property;

    GParamSpec *property_specs[MELANGE_APP_N_PROPS] = { NULL };
    GParamFlags property_flags =  G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK
                    | G_PARAM_STATIC_BLURB;

    property_specs[MELANGE_APP_PROP_DARK_THEME] = g_param_spec_boolean("dark-theme", "dark-theme",
            "dark-theme", FALSE, property_flags);
    property_specs[MELANGE_APP_PROP_AUTO_HIDE_SIDEBAR] = g_param_spec_boolean("auto-hide-sidebar",
            "auto-hide-sidebar", "auto-hide-sidebar", FALSE, property_flags);
    property_specs[MELANGE_APP_PROP_CLIENT_SIDE_DECORATIONS] = g_param_spec_string(
            "client-side-decorations", "client-side-decorations", "client-side-decorations",
            "auto", property_flags);

    g_object_class_install_properties(G_OBJECT_CLASS(cls), MELANGE_APP_N_PROPS, property_specs);
}


GApplication *
melange_app_new(void) {
    return g_object_new(MELANGE_TYPE_APP,
            "application-id", "de.inforge.melange",
            "register-session", TRUE,
            NULL);
}


WebKitWebContext *
melange_app_get_web_context(MelangeApp *app) {
    return app->web_context;
}
