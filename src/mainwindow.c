#include "mainwindow.h"
#include "app.h"
#include "presets.h"
#include "util.h"
#include <math.h>


struct MelangeMainWindow
{
    GtkApplicationWindow parent_instance;

    MelangeApp *app;

    GtkWidget *last_web_view;
    GtkWidget *add_view;
    GtkWidget *settings_view;
    GtkWidget *account_details_view;
    GtkWidget *sidebar_revealer;
    GtkWidget *sidebar_handle;
    GtkWidget *view_stack;
    GtkWidget *switcher_box;
    GtkWidget *menu_box;

    GArray *account_views;

    guint sidebar_timeout;
    const char* initial_csd_setting;
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
    (void) win;
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
    gtk_stack_set_visible_child(GTK_STACK(win->view_stack), win->last_web_view ? win->last_web_view : win->add_view);

    gboolean auto_hide_sidebar;
    g_object_get(win->app, "auto-hide-sidebar", &auto_hide_sidebar, NULL);
    gtk_widget_set_visible(win->sidebar_handle, auto_hide_sidebar);
}


static void
melange_main_window_init(MelangeMainWindow *win) {
    win->sidebar_timeout = 0;

    GdkGeometry hints = { .min_width = 800, .min_height = 600 };
    gtk_window_set_geometry_hints(GTK_WINDOW(win), NULL, &hints, GDK_HINT_MIN_SIZE);
}


static void
melange_main_window_switch_to_view(GtkButton *button, GtkWidget *switch_to) {
    (void) button;

    GtkStack *stack = GTK_STACK(gtk_widget_get_parent(switch_to));
    gtk_stack_set_visible_child(stack, switch_to);
}


static GtkWidget *
melange_main_window_create_switcher_button(GdkPixbuf *pixbuf, int padding, GtkWidget *switch_to) {
    int padded_size = 32 - 2 * padding;

    GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
    gtk_image_set_pixel_size(GTK_IMAGE(image), padded_size);
    gtk_widget_set_margin_top(image, padding);
    gtk_widget_set_margin_bottom(image, padding);
    gtk_widget_set_margin_start(image, padding);
    gtk_widget_set_margin_end(image, padding);

    GtkWidget *switcher = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(switcher), GTK_RELIEF_NONE);
    gtk_button_set_image(GTK_BUTTON(switcher), image);
    g_signal_connect(switcher, "clicked", G_CALLBACK(melange_main_window_switch_to_view),
            switch_to);
    return switcher;
}


static GtkWidget *
melange_main_window_create_utility_switcher_button(const char* icon, GtkWidget *switch_to) {
    int padded_size = 16;

    char *file_name = g_strdup_printf("res/icons/light/%s.svg", icon);
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size(file_name, padded_size, padded_size, NULL);
    g_free(file_name);

    return melange_main_window_create_switcher_button(pixbuf, 8, switch_to);
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
    (void) app;
    (void) pspec;
    (void) win;
}


static gboolean
melange_main_window_navigate_back(MelangeMainWindow *win) {
    GtkWidget *view = gtk_stack_get_visible_child(GTK_STACK(win->view_stack));
    if (view == win->settings_view) {
        gtk_stack_set_visible_child(GTK_STACK(win->view_stack),
                win->last_web_view ? win->last_web_view : win->add_view);
    } else if (view == win->add_view && win->last_web_view) {
        gtk_stack_set_visible_child(GTK_STACK(win->view_stack), win->last_web_view);
    } else if (view == win->account_details_view) {
        gtk_stack_set_visible_child(GTK_STACK(win->view_stack), win->add_view);
    } else {
        return FALSE;
    }
    return TRUE;
}


GLADE_EVENT_HANDLER gboolean
melange_main_window_button_press_event(GtkWidget *widget, GdkEvent *event, MelangeMainWindow *win) {
    (void) widget;

    switch (event->button.button){
        case 8: // Mouse "back" button
            return melange_main_window_navigate_back(win);

        default:
            return FALSE;
    }
}


static gboolean
melange_main_window_key_press_event(GtkWidget *widget, GdkEventKey *event, MelangeMainWindow *win) {
    (void) widget;

    switch (event->keyval) {
        case GDK_KEY_Back:
        case GDK_KEY_Escape:
            return melange_main_window_navigate_back(win);
    }
    return FALSE;
}


static void
melange_main_window_add_account_view(MelangeAccount *account, MelangeMainWindow *win) {
    char *base_path = g_strdup_printf("%s/melange/accounts/%s", g_get_user_cache_dir(),
                                      account->id);

    WebKitWebsiteDataManager *data_manager = webkit_website_data_manager_new(
            "base-data-directory", base_path,
            "base-cache-directory", base_path,
            NULL);

    g_free(base_path);

    WebKitWebContext *web_context = webkit_web_context_new_with_website_data_manager(data_manager);

    WebKitSecurityOrigin *origin = webkit_security_origin_new_for_uri(account->service_url);
    GList *allowed_origins = g_list_append(NULL, origin);
    webkit_web_context_initialize_notification_permissions(web_context, allowed_origins, NULL);

    GtkWidget *web_view = webkit_web_view_new_with_context(web_context);
    g_signal_connect(web_view, "context-menu",
                     G_CALLBACK(melange_main_window_web_view_context_menu), win);
    g_signal_connect(web_view, "notify::title",
                     G_CALLBACK(melange_main_window_web_view_notify_title), win);
    g_signal_connect(web_view, "load-changed",
                     G_CALLBACK(melange_main_window_web_view_load_changed), win);

    WebKitSettings *sett = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(web_view));
    webkit_settings_set_user_agent(sett, account->user_agent);

    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(web_view), account->service_url);
    gtk_container_add(GTK_CONTAINER(win->view_stack), web_view);
    gtk_widget_show_all(web_view);

    if (!win->last_web_view) {
        win->last_web_view = web_view;
    }

    GdkPixbuf *pixbuf = NULL;
    if (account->preset) {
        pixbuf = melange_app_request_icon(win->app, account->preset);
    }
    if (!pixbuf) {
        pixbuf = gdk_pixbuf_new_from_file_at_size("res/icons/light/messenger.svg", 32, 32, NULL);
    }
    GtkWidget *switcher_button = melange_main_window_create_switcher_button(pixbuf, 0, web_view);
    gtk_container_add(GTK_CONTAINER(win->switcher_box), switcher_button);
    gtk_widget_show_all(switcher_button);

    melange_main_window_switch_to_view(NULL, web_view);
}


static void
melange_main_window_add_service_button_clicked(GtkButton *button, MelangeMainWindow *win) {
    const char *preset = g_object_get_qdata(
            G_OBJECT(button), g_quark_from_static_string("preset"));
    g_return_if_fail(preset);

    int serial = 1;
    char *id = g_strdup_printf("%s%d", preset, serial);

    MelangeAccount *account = melange_account_new_from_preset(id, preset);
    while (!melange_app_add_account(win->app, account)) {
        g_free(account->id);
        account->id = g_strdup_printf("%s%d", preset, ++serial);
    }

    melange_main_window_add_account_view(account, win);
}


static GtkWidget *
melange_main_window_create_service_add_button(MelangeMainWindow *win, const MelangeAccount *preset) {
    GdkPixbuf *pixbuf = NULL;
    if (preset && preset->preset) {
        pixbuf = melange_app_request_icon(win->app, preset->preset);
    }
    if (!pixbuf) {
        pixbuf = gdk_pixbuf_new_from_file_at_size("res/icons/light/messenger.svg", 32, 32, NULL);
    }

    GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);

    GtkWidget *label = gtk_label_new(preset ? preset->service_name : "Custom");
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_label_set_width_chars(GTK_LABEL(label), 12);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_add(GTK_CONTAINER(box), image);
    gtk_container_add(GTK_CONTAINER(box), label);

    GtkWidget *button = gtk_button_new();
    gtk_widget_set_margin_top(box, 10);
    gtk_widget_set_margin_bottom(box, 10);
    gtk_widget_set_margin_start(box, 20);
    gtk_widget_set_margin_end(box, 20);
    gtk_container_add(GTK_CONTAINER(button), box);

    if (preset) {
        g_object_set_qdata(G_OBJECT(button), g_quark_from_static_string("preset"),
                           (gpointer) preset->preset);
        g_signal_connect(button, "clicked",
                         G_CALLBACK(melange_main_window_add_service_button_clicked), win);
    }

    return button;
}


static void
melange_main_window_constructed(GObject *obj) {
    G_OBJECT_CLASS(melange_main_window_parent_class)->constructed(obj);

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
            gdk_pixbuf_new_from_file_at_size("res/icons/light/vdots.svg", 4, -1, NULL));

    win->add_view = GTK_WIDGET(gtk_builder_get_object(builder, "add-view"));
    gtk_container_add(GTK_CONTAINER(win->view_stack), win->add_view);
    win->account_details_view = GTK_WIDGET(gtk_builder_get_object(builder, "account-details-view"));
    gtk_container_add(GTK_CONTAINER(win->view_stack), win->account_details_view);

    GtkWidget *service_grid = GTK_WIDGET(gtk_builder_get_object(builder, "service-grid"));
    for (size_t i = 0; i <= melange_n_account_presets; ++i) {
        GtkWidget *button;
        if (i < melange_n_account_presets) {
            button = melange_main_window_create_service_add_button(
                    win, &melange_account_presets[i]);
        } else {
            button = melange_main_window_create_service_add_button(win, NULL);
            g_signal_connect(button, "clicked", G_CALLBACK(melange_main_window_switch_to_view),
                             win->account_details_view);
        }
        gtk_grid_attach(GTK_GRID(service_grid), button, (gint) i % 3, (gint) i / 3, 1, 1);
    }

    win->settings_view = GTK_WIDGET(gtk_builder_get_object(builder, "settings-view"));
    gtk_container_add(GTK_CONTAINER(win->view_stack), win->settings_view);

    GtkSwitch *dark_theme_setting = GTK_SWITCH(gtk_builder_get_object(builder,
            "dark-theme-setting"));
    GtkSwitch *auto_hide_sidebar_setting = GTK_SWITCH(gtk_builder_get_object(builder,
            "auto-hide-sidebar-setting"));
    GtkComboBox *client_side_decorations_setting = GTK_COMBO_BOX(gtk_builder_get_object(builder,
            "client-side-decorations-setting"));

    g_object_unref(builder);

    GtkCssProvider *css_provider = gtk_css_provider_new();
    if (!gtk_css_provider_load_from_path(css_provider, "res/ui/mainwindow.css", NULL)) {
        g_warning("Unable to load CSS file");
    }
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
            GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    gboolean dark_theme;
    g_object_get(win->app, "dark-theme", &dark_theme, NULL);
    gtk_switch_set_state(dark_theme_setting, dark_theme);

    gboolean auto_hide_sidebar;
    g_object_get(win->app, "auto-hide-sidebar", &auto_hide_sidebar, NULL);
    gtk_widget_set_visible(win->sidebar_handle, auto_hide_sidebar);
    gtk_switch_set_state(auto_hide_sidebar_setting, auto_hide_sidebar);
    g_signal_connect(win->app, "notify::auto-hide-sidebar",
            G_CALLBACK(melange_main_window_app_notify_auto_hide_sidebar), win);

    const char *client_side_decorations;
    g_object_get(win->app, "client-side-decorations", &client_side_decorations, NULL);
    gtk_combo_box_set_active_id(client_side_decorations_setting, client_side_decorations);
    g_signal_connect(win->app, "notify::client-side-decorations",
            G_CALLBACK(melange_main_window_app_notify_client_side_decorations), win);

    const char* csd_option;
    g_object_get(win->app, "client-side-decorations", &csd_option, NULL);

    gboolean enable_csd = FALSE;
    if (g_str_equal(csd_option, "on")) {
        enable_csd = TRUE;
    } else if (g_str_equal(csd_option, "auto")) {
        enable_csd = gtk_application_prefers_app_menu(GTK_APPLICATION(win->app));
    }

    GtkWidget *header_bar = NULL;
    if (enable_csd) {
        header_bar = gtk_header_bar_new();
        gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);
        gtk_window_set_titlebar(GTK_WINDOW(win), header_bar);
        gboolean header_bar_dark = melange_util_has_dark_background(header_bar);
        GtkWidget *image = gtk_image_new_from_pixbuf(gdk_pixbuf_new_from_file_at_size(
                header_bar_dark ? "res/icons/dark/settings.svg" : "res/icons/light/settings.svg", 16, 16, NULL));
        GtkWidget *switcher = GTK_WIDGET(gtk_tool_button_new(image, "Preferences"));
        g_signal_connect(switcher, "clicked", G_CALLBACK(melange_main_window_switch_to_view), win->settings_view);
        gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), switcher);
    } else {
        gtk_container_add(GTK_CONTAINER(win->menu_box),
                          melange_main_window_create_utility_switcher_button("settings", win->settings_view));
    }

    melange_app_iterate_accounts(win->app, (MelangeAccountConstFunc) melange_main_window_add_account_view, win);

    gtk_box_pack_end(GTK_BOX(win->switcher_box),
                     melange_main_window_create_utility_switcher_button("add", win->add_view),
                     FALSE, FALSE, 0);


    g_signal_connect(win, "button-press-event", G_CALLBACK(melange_main_window_button_press_event), win);
    g_signal_connect(win, "key-press-event", G_CALLBACK(melange_main_window_key_press_event), win);

    gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(win), FALSE);
}


static void
melange_main_window_finalize(GObject *obj) {
    MelangeMainWindow *win = MELANGE_MAIN_WINDOW(obj);
    g_signal_handlers_disconnect_by_data(win->app, win);

    G_OBJECT_CLASS(melange_main_window_parent_class)->finalize(obj);
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
