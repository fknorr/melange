#include "mainwindow.h"
#include "presets.h"
#include "util.h"

#include <stdlib.h>


struct MelangeMainWindow {
    GtkApplicationWindow parent_instance;

    MelangeApp *app;

    // The main GtkStack switching between web, add, account editing and settings view
    GtkWidget *view_stack;
    GtkWidget *add_view;
    GtkWidget *settings_view;
    GtkWidget *account_details_view;

    // Which view was active before the current one? (For navigation with back/escape)
    GtkWidget *last_web_view;

    GtkWidget *sidebar_revealer;

    // Vertical dots, visible when auto-hide-sidebar is on
    GtkWidget *sidebar_handle;

    // Container of the sidebar top switcher buttons (web views & add view)
    GtkWidget *switcher_box;

    // Outer container of switcher_box and the settings button (csd-off mode)
    GtkWidget *menu_box;

    // Button grid in add view
    GtkWidget *service_grid;

    // Matches number of notifications in titles like "(1) WhatsApp"
    GRegex *new_message_regex;

    guint sidebar_timeout;
    guint notification_timeout;

    const char *initial_csd_setting;
};


typedef GtkApplicationWindowClass MelangeMainWindowClass;


enum {
    MELANGE_MAIN_WINDOW_PROP_APP = 1,
    MELANGE_MAIN_WINDOW_N_PROPS
};


G_DEFINE_TYPE(MelangeMainWindow, melange_main_window, GTK_TYPE_APPLICATION_WINDOW)


static void
melange_main_window_set_property(GObject *object, guint property_id, const GValue *value,
        GParamSpec *pspec) {
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

    // Suppress context menu
    return TRUE;
}


// If difference == 0, resets the notification count for web_view to 0.
// If difference > 0, adds `difference` notifications to web_view.
// Configures the red notification labels and updates the global notification count.
static void
melange_main_window_update_unread_messages(MelangeMainWindow *win, WebKitWebView *web_view,
        int difference) {
    int global;
    g_object_get(win->app, "unread-messages", &global, NULL);
    int local = (int) (intptr_t) g_object_get_data(G_OBJECT(web_view), "unread-messages");

    if (difference == 0) {
        global = local = 0;
    } else {
        local = MAX(0, local) + difference; // local starts out at -1
        global += difference;
    }

    g_object_set(win->app, "unread-messages", global, NULL);
    g_object_set_data(G_OBJECT(web_view), "unread-messages", (gpointer) (intptr_t) local);

    GtkWidget *notify_label = g_object_get_data(G_OBJECT(web_view), "notify-label");
    if (local > 0) {
        char text[10] = { 0 };
        snprintf(text, 10, "%d", local);
        gtk_label_set_text(GTK_LABEL(notify_label), text);
    }
    gtk_widget_set_visible(notify_label, local > 0);
}


static void
melange_main_window_web_view_notify_title(WebKitWebView *web_view, GParamSpec *pspec,
        MelangeMainWindow *win) {
    (void) pspec;
    (void) win;

    // Only use the title notification count if we have not seen any data yet. Later, incoming
    // notification pop-ups are counted towards the notification count instead
    if (g_object_get_data(G_OBJECT(web_view), "unread-messages") != (gpointer) -1) {
        return;
    }

    int unread = 0;

    // Parse title like "(1) WhatsApp"
    GMatchInfo *match;
    g_regex_match(win->new_message_regex, webkit_web_view_get_title(web_view), 0, &match);
    if (g_match_info_matches(match)) {
        unread = (int) strtol(g_match_info_fetch(match, 2), NULL, 10);
        melange_main_window_update_unread_messages(win, web_view, unread);
    }
}


static void
melange_main_window_web_view_load_changed(WebKitWebView *web_view, WebKitLoadEvent load_event,
        MelangeMainWindow *win) {
    MelangeAccount *account = g_object_get_data(G_OBJECT(web_view), "account");
    if (account->preset && load_event == WEBKIT_LOAD_FINISHED) {
        // Inject JS code for modifying style etc.
        char *file_name = g_strdup_printf("js/%s.js", account->preset->id);
        char *override_js = melange_app_load_text_resource(win->app, file_name, TRUE);
        if (override_js) {
            webkit_web_view_run_javascript(web_view, override_js, NULL, NULL, NULL);
            g_free(override_js);
        }
        g_free(file_name);
    }
}


static gboolean
melange_main_window_web_view_show_notification(WebKitWebView *web_view,
        WebKitNotification *notification, MelangeMainWindow *win) {
    MelangeAccount *account = g_object_get_data(G_OBJECT(web_view), "account");
    g_assert(account);

    // If view in background, count towards notification label
    if (!gtk_window_is_active(GTK_WINDOW(win))
            || gtk_stack_get_visible_child(GTK_STACK(win->view_stack)) != GTK_WIDGET(web_view)) {
        melange_main_window_update_unread_messages(win, web_view, 1);
    }

    const char *title = webkit_notification_get_title(notification);
    const char *body = webkit_notification_get_body(notification);
    const char *icon = account->preset ? account->preset->id : NULL;

    melange_app_show_message_notification(win->app, title, body, icon);
    return TRUE;
}

gboolean
melange_main_window_web_view_decide_policy(WebKitWebView *web_view, WebKitPolicyDecision *decision,
        WebKitPolicyDecisionType decision_type, MelangeMainWindow *win) {
    (void) web_view;
    (void) win;

    if (decision_type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION) {
        WebKitNavigationAction *action = webkit_navigation_policy_decision_get_navigation_action(
                        WEBKIT_NAVIGATION_POLICY_DECISION(decision));
        WebKitURIRequest *request = webkit_navigation_action_get_request(action);
        if (g_str_equal(webkit_uri_request_get_http_method(request), "GET")) {
            GError *error;
            if (!g_app_info_launch_default_for_uri(webkit_uri_request_get_uri(request), NULL,
                    &error)) {
                g_warning("Unable to open link in external application: %s", error->message);
                g_error_free(error);
            }
        }
        webkit_policy_decision_ignore(decision);
        return TRUE;
    }
    return FALSE;
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
        MelangeMainWindow *win) {
    (void) widget;
    (void) event;

    melange_main_window_cancel_sidebar_timeout(win);
    melange_main_window_set_sidebar_visible(win, TRUE);
    return TRUE;
}


GLADE_EVENT_HANDLER gboolean
melange_main_window_sidebar_leave_notify_event(GtkWidget *widget, GdkEvent *event,
        MelangeMainWindow *win) {
    (void) widget;
    (void) event;

    melange_main_window_hide_sidebar_after_timeout(win, 1000);
    return TRUE;
}


GLADE_EVENT_HANDLER gboolean
melange_main_window_dark_theme_setting_state_set(GtkSwitch *widget, gboolean state,
        MelangeMainWindow *win) {
    (void) widget;
    g_object_set(win->app, "dark-theme", state, NULL);
    return FALSE;
}


GLADE_EVENT_HANDLER gboolean
melange_main_window_auto_hide_sidebar_setting_state_set(GtkSwitch *widget, gboolean state,
        MelangeMainWindow *win) {
    (void) widget;
    g_object_set(win->app, "auto-hide-sidebar", state, NULL);
    return FALSE;
}


GLADE_EVENT_HANDLER void
melange_main_window_client_side_decorations_setting_changed(GtkComboBox *combo_box,
        MelangeMainWindow *win) {
    g_object_set(win->app, "client-side-decorations", gtk_combo_box_get_active_id(combo_box), NULL);
}


static gboolean
melange_main_window_clear_active_view_notification(MelangeMainWindow *win) {
    GtkWidget *active_view = gtk_stack_get_visible_child(GTK_STACK(win->view_stack));
    melange_main_window_update_unread_messages(win, WEBKIT_WEB_VIEW(active_view), 0);
    win->notification_timeout = 0;
    return false;
}


static void
melange_main_window_cancel_notification_timeout(MelangeMainWindow *win) {
    if (win->notification_timeout) {
        g_source_remove(win->notification_timeout);
        win->notification_timeout = 0;
    }
}


static void
melange_main_window_clear_active_view_notification_after_timeout(MelangeMainWindow *win) {
    if (!win->notification_timeout) {
        GtkWidget *active_view = gtk_stack_get_visible_child(GTK_STACK(win->view_stack));
        if (WEBKIT_IS_WEB_VIEW(active_view)) {
            win->notification_timeout = g_timeout_add(3000,
                    (GSourceFunc) melange_main_window_clear_active_view_notification, win);
        }
    }
}


static void
melange_main_window_notify_is_active(GObject *obj, GParamSpec *pspec, MelangeMainWindow *win) {
    (void) obj;
    (void) pspec;

    melange_main_window_cancel_notification_timeout(win);

    // If the user has looked at a view for 3 seconds, clear the notification count
    if (gtk_window_is_active(GTK_WINDOW(win))) {
        melange_main_window_clear_active_view_notification_after_timeout(win);
    }
}


static void
melange_main_window_realize(GtkWidget *widget) {
    GTK_WIDGET_CLASS(melange_main_window_parent_class)->realize(widget);

    MelangeMainWindow *win = MELANGE_MAIN_WINDOW(widget);
    melange_main_window_hide_sidebar_after_timeout(win, 3000);
    gtk_stack_set_visible_child(GTK_STACK(win->view_stack),
            win->last_web_view ? win->last_web_view : win->add_view);
}


static void
melange_main_window_init(MelangeMainWindow *win) {
    win->sidebar_timeout = 0;
    win->notification_timeout = 0;
    win->new_message_regex = g_regex_new("(^\\s*|.*\\()(\\d+)\\b", 0, 0, NULL);

    GdkGeometry hints = { .min_width = 800, .min_height = 600 };
    gtk_window_set_geometry_hints(GTK_WINDOW(win), NULL, &hints, GDK_HINT_MIN_SIZE);
}


static void
melange_main_window_switch_to_view(GtkWidget *view) {
    GtkStack *stack = GTK_STACK(gtk_widget_get_parent(view));
    gtk_stack_set_visible_child(stack, view);
}


static void
melange_main_window_switcher_button_clicked(GtkButton *button, MelangeMainWindow *win) {
    (void) button;

    melange_main_window_cancel_notification_timeout(win);

    GtkWidget *switch_to = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "switch-to"));
    melange_main_window_switch_to_view(switch_to);

    // If the user has looked at a view for 3 seconds, clear the notification count
    if (WEBKIT_IS_WEB_VIEW(switch_to)) {
        melange_main_window_clear_active_view_notification_after_timeout(win);
    }
}


static GtkWidget *
melange_main_window_create_switcher_button(MelangeMainWindow *win, GdkPixbuf *pixbuf,
        int padding, GtkWidget *switch_to, gboolean notifications) {
    int padded_size = 32 - 2 * padding;

    GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
    gtk_image_set_pixel_size(GTK_IMAGE(image), padded_size);
    gtk_widget_set_margin_top(image, padding ? padding : 3);
    gtk_widget_set_margin_bottom(image, padding ? padding : 3);
    gtk_widget_set_margin_start(image, padding);
    gtk_widget_set_margin_end(image, padding);

    GtkWidget *switcher = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(switcher), GTK_RELIEF_NONE);
    gtk_widget_set_can_focus(switcher, FALSE);

    if (notifications) {
        GtkWidget *overlay = gtk_overlay_new();
        gtk_container_add(GTK_CONTAINER(overlay), image);

        GtkWidget *label = gtk_label_new(NULL);
        gtk_widget_set_halign(label, GTK_ALIGN_END);
        gtk_widget_set_valign(label, GTK_ALIGN_END);
        gtk_widget_set_name(label, "notify-label");
        gtk_widget_set_no_show_all(label, TRUE);
        gtk_overlay_add_overlay(GTK_OVERLAY(overlay), label);

        // Do not handle mouse events in the label, pass through to button
        gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(overlay), label, TRUE);

        gtk_container_add(GTK_CONTAINER(switcher), overlay);
        g_object_set_data(G_OBJECT(switcher), "notify-label", label);
        g_object_set_data(G_OBJECT(switch_to), "notify-label", label);
    } else {
        gtk_button_set_image(GTK_BUTTON(switcher), image);
    }

    g_object_set_data(G_OBJECT(switcher), "switch-to", switch_to);
    g_signal_connect(switcher, "clicked", G_CALLBACK(melange_main_window_switcher_button_clicked),
            win);
    return switcher;
}


static GtkWidget *
melange_main_window_create_utility_switcher_button(MelangeMainWindow *win, const char *icon,
        GtkWidget *switch_to) {
    int padded_size = 16;

    char *file_name = g_strdup_printf("icons/light/%s.svg", icon);
    GdkPixbuf *pixbuf = melange_app_load_pixbuf_resource(win->app, file_name, padded_size,
            padded_size, FALSE);
    g_free(file_name);

    return melange_main_window_create_switcher_button(win, pixbuf, 8, switch_to, FALSE);
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

    switch (event->button.button) {
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

    // Each web view has its own data manager and web context to allow multiple accounts of the
    // same messenger

    WebKitWebsiteDataManager *data_manager = webkit_website_data_manager_new(
            "base-data-directory", base_path,
            "base-cache-directory", base_path,
            NULL);

    g_free(base_path);

    WebKitWebContext *web_context = webkit_web_context_new_with_website_data_manager(data_manager);

    WebKitSecurityOrigin *origin = webkit_security_origin_new_for_uri(
            melange_account_get_service_url(account));
    GList *allowed_origins = g_list_append(NULL, origin);
    webkit_web_context_initialize_notification_permissions(web_context, allowed_origins, NULL);

    GtkWidget *web_view = webkit_web_view_new_with_context(web_context);
    g_signal_connect(web_view, "context-menu",
            G_CALLBACK(melange_main_window_web_view_context_menu), win);
    g_signal_connect(web_view, "notify::title",
            G_CALLBACK(melange_main_window_web_view_notify_title), win);
    g_signal_connect(web_view, "load-changed",
            G_CALLBACK(melange_main_window_web_view_load_changed), win);
    g_signal_connect(web_view, "show-notification",
            G_CALLBACK(melange_main_window_web_view_show_notification), win);
    g_signal_connect(web_view, "decide-policy",
            G_CALLBACK(melange_main_window_web_view_decide_policy), win);

    WebKitSettings *sett = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(web_view));
    webkit_settings_set_user_agent(sett, melange_account_get_user_agent(account));
    webkit_settings_set_enable_java(sett, FALSE);
    webkit_settings_set_enable_offline_web_application_cache(sett, TRUE);
    webkit_settings_set_enable_plugins(sett, FALSE);
    webkit_settings_set_enable_developer_extras(sett, FALSE);

    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(web_view), melange_account_get_service_url(account));
    gtk_container_add(GTK_CONTAINER(win->view_stack), web_view);
    gtk_widget_show_all(web_view);

    if (!win->last_web_view) {
        win->last_web_view = web_view;
    }

    GdkPixbuf *pixbuf = NULL;
    if (account->preset) {
        pixbuf = melange_app_request_icon(win->app, account->preset->id);
    }
    if (!pixbuf) {
        pixbuf = melange_app_load_pixbuf_resource(win->app, "icons/light/messenger.svg",
                32, 32, FALSE);
    }

    GtkWidget *switcher_button = melange_main_window_create_switcher_button(
            win, pixbuf, 0, web_view, TRUE);
    gtk_container_add(GTK_CONTAINER(win->switcher_box), switcher_button);
    gtk_widget_show_all(switcher_button);

    gpointer pointer = (gpointer) account;
    g_object_set_data(G_OBJECT(web_view), "account", pointer);
    g_object_set_data(G_OBJECT(switcher_button), "account", pointer);

    g_object_set_data(G_OBJECT(web_view), "unread-messages", (gpointer) -1);

    melange_main_window_switch_to_view(web_view);
}


static void
melange_main_window_add_service_button_clicked(GtkButton *button, MelangeMainWindow *win) {
    const MelangeAccount *preset = g_object_get_data(G_OBJECT(button), "preset");
    g_return_if_fail(preset);

    // Count ids "whatsapp1", "whatsapp2", ...

    int serial = 1;
    char *id = g_strdup_printf("%s%d", preset->id, serial);

    MelangeAccount *account = melange_account_new_from_preset(id, preset);
    while (!melange_app_add_account(win->app, account)) {
        g_free(account->id);
        account->id = g_strdup_printf("%s%d", preset->id, ++serial);
    }

    melange_main_window_add_account_view(account, win);
}


// Buttons for the add view grid
static GtkWidget *
melange_main_window_create_service_add_button(MelangeMainWindow *win,
        const MelangeAccount *preset) {
    GdkPixbuf *pixbuf = NULL;
    if (preset) {
        pixbuf = melange_app_request_icon(win->app, preset->id);
    }
    if (!pixbuf) {
        pixbuf = melange_app_load_pixbuf_resource(win->app, "icons/light/messenger.svg",
                32, 32, FALSE);
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
        g_object_set_data(G_OBJECT(button), "preset", (gpointer) preset);
        g_signal_connect(button, "clicked",
                G_CALLBACK(melange_main_window_add_service_button_clicked), win);
    }

    return button;
}


// Callback when the app has finished loading a messenger icon from the web.
static void
melange_main_window_icon_available(MelangeApp *app, const char *preset, GdkPixbuf *pixbuf,
        MelangeMainWindow *win) {
    (void) app;

    // Update "add service" view grid
    for (GList *list = gtk_container_get_children(GTK_CONTAINER(win->service_grid)); list;
         list = list->next) {
        GtkContainer *button = GTK_CONTAINER(list->data);
        const char *button_preset = g_object_get_data(G_OBJECT(button), "preset");
        if (button_preset && g_str_equal(button_preset, preset)) {
            GtkContainer *box = GTK_CONTAINER(gtk_container_get_children(button)->data);
            GtkImage *image = GTK_IMAGE(gtk_container_get_children(box)->data);
            gtk_image_set_from_pixbuf(image, pixbuf);
        }
    }

    // Update sidebar
    for (GList *list = gtk_container_get_children(GTK_CONTAINER(win->switcher_box)); list;
         list = list->next) {
        GtkButton *button = GTK_BUTTON(list->data);
        MelangeAccount *account = g_object_get_data(G_OBJECT(button), "account");
        if (account && account->preset && g_str_equal(account->preset->id, preset)) {
            gtk_image_set_from_pixbuf(GTK_IMAGE(gtk_button_get_image(button)), pixbuf);
        }
    }
}


static void
melange_main_window_constructed(GObject *obj) {
    G_OBJECT_CLASS(melange_main_window_parent_class)->constructed(obj);

    MelangeMainWindow *win = MELANGE_MAIN_WINDOW(obj);
    g_return_if_fail(win->app);

    // Glade file contains everything but the actual main window, since it's a custom class
    GtkBuilder *builder = melange_app_load_ui_resource(win->app, "ui/mainwindow.glade", FALSE);
    gtk_builder_connect_signals(builder, win);

    GtkWidget *layout = GTK_WIDGET(gtk_builder_get_object(builder, "layout"));
    gtk_container_add(GTK_CONTAINER(win), layout);

    win->sidebar_revealer = GTK_WIDGET(gtk_builder_get_object(builder, "sidebar-revealer"));
    win->sidebar_handle = GTK_WIDGET(gtk_builder_get_object(builder, "sidebar-handle"));
    win->view_stack = GTK_WIDGET(gtk_builder_get_object(builder, "view-stack"));
    win->menu_box = GTK_WIDGET(gtk_builder_get_object(builder, "menu-box"));
    win->switcher_box = GTK_WIDGET(gtk_builder_get_object(builder, "switcher-box"));

    gtk_image_set_from_pixbuf(GTK_IMAGE(win->sidebar_handle),
            melange_app_load_pixbuf_resource(win->app, "icons/light/vdots.svg", 4, -1, FALSE));

    win->add_view = GTK_WIDGET(gtk_builder_get_object(builder, "add-view"));
    gtk_container_add(GTK_CONTAINER(win->view_stack), win->add_view);
    win->account_details_view = GTK_WIDGET(gtk_builder_get_object(builder, "account-details-view"));
    gtk_container_add(GTK_CONTAINER(win->view_stack), win->account_details_view);

    win->service_grid = GTK_WIDGET(gtk_builder_get_object(builder, "service-grid"));
    for (size_t i = 0; i <= melange_n_account_presets; ++i) {
        GtkWidget *button;
        if (i < melange_n_account_presets) {
            button = melange_main_window_create_service_add_button(
                    win, &melange_account_presets[i]);
        } else {
            button = melange_main_window_create_service_add_button(win, NULL);
            g_object_set_data(G_OBJECT(button), "switch-to", win->account_details_view);
            g_signal_connect(button, "clicked",
                    G_CALLBACK(melange_main_window_switcher_button_clicked), win);
        }
        // Add icons left-to-right, then top-to-bottom
        gtk_grid_attach(GTK_GRID(win->service_grid), button, (gint) i % 3, (gint) i / 3, 1, 1);
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

    // Stylesheet
    GtkCssProvider *css_provider = gtk_css_provider_new();
    char *css_path = melange_app_get_resource_path(win->app, "ui/mainwindow.css");
    if (!gtk_css_provider_load_from_path(css_provider, css_path, NULL)) {
        g_warning("Unable to load CSS file");
    }
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
            GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_free(css_path);

    // Settings controls
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

    const char *csd_option;
    g_object_get(win->app, "client-side-decorations", &csd_option, NULL);

    gboolean enable_csd = FALSE;
    if (g_str_equal(csd_option, "on")) {
        enable_csd = TRUE;
    } else if (g_str_equal(csd_option, "auto")) {
        // prefers-app-menu is true for GNOME, false for Xfce and i3, as intended. Maybe replace.
        enable_csd = gtk_application_prefers_app_menu(GTK_APPLICATION(win->app));
    }

    GtkWidget *header_bar = NULL;
    if (enable_csd) {
        header_bar = gtk_header_bar_new();
        gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);
        gtk_window_set_titlebar(GTK_WINDOW(win), header_bar);
        gboolean header_bar_dark = melange_util_has_dark_background(header_bar);

        GtkWidget *image = gtk_image_new_from_pixbuf(melange_app_load_pixbuf_resource(win->app,
                header_bar_dark ? "icons/dark/settings.svg" : "icons/light/settings.svg",
                16, 16, FALSE));

        // Move "settings" button to header bar
        GtkWidget *switcher = GTK_WIDGET(gtk_tool_button_new(image, "Preferences"));
        g_object_set_data(G_OBJECT(switcher), "switch-to", win->settings_view);
        g_signal_connect(switcher, "clicked",
                G_CALLBACK(melange_main_window_switcher_button_clicked), win);
        gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), switcher);
    } else {
        // Settings button in sidebar
        gtk_container_add(GTK_CONTAINER(win->menu_box),
                melange_main_window_create_utility_switcher_button(win,
                        "settings", win->settings_view));
    }

    melange_app_iterate_accounts(win->app,
            (MelangeAccountConstFunc) melange_main_window_add_account_view, win);

    gtk_box_pack_end(GTK_BOX(win->switcher_box),
            melange_main_window_create_utility_switcher_button(win, "add", win->add_view),
            FALSE, FALSE, 0);

    g_signal_connect(win, "notify::is-active", G_CALLBACK(melange_main_window_notify_is_active),
            win);
    g_signal_connect(win, "button-press-event", G_CALLBACK(melange_main_window_button_press_event),
            win);
    g_signal_connect(win, "key-press-event", G_CALLBACK(melange_main_window_key_press_event), win);
    g_signal_connect(win->app, "icon-available", G_CALLBACK(melange_main_window_icon_available),
            win);

    gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(win), FALSE);
}


static void
melange_main_window_finalize(GObject *obj) {
    MelangeMainWindow *win = MELANGE_MAIN_WINDOW(obj);
    g_signal_handlers_disconnect_by_data(win->app, win);

    g_regex_unref(win->new_message_regex);

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
