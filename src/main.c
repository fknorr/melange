#include <gtk/gtk.h>
#include <webkit2/webkit2.h>


int main(int argc, char **argv) {
    gtk_init(&argc, &argv);

    WebKitWebsiteDataManager *data_manager = webkit_website_data_manager_new(
            "base-data-directory", "/tmp/melange/data",
            "base-cache-directory", "/tmp/melange/cache",
            NULL);

    WebKitWebContext *cxt = webkit_web_context_new_with_website_data_manager(data_manager);
    webkit_web_context_set_cache_model(cxt, WEBKIT_CACHE_MODEL_WEB_BROWSER);

    WebKitSecurityOrigin *origin = webkit_security_origin_new_for_uri("https://web.whatsapp.com");
	GList *allowed_origins = g_list_append(NULL, origin);
    webkit_web_context_initialize_notification_permissions(cxt, allowed_origins, NULL);

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    GtkWidget *web = webkit_web_view_new_with_context(cxt);
    WebKitSettings *sett = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(web));
    webkit_settings_set_user_agent(sett, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
            "(KHTML, like Gecko) Chrome/63.0.3239.108 Safari/537.36");

	webkit_web_view_load_uri(WEBKIT_WEB_VIEW(web), "https://web.whatsapp.com");

    gtk_container_add(GTK_CONTAINER(win), web);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_show_all(win);

    gtk_main();

}

