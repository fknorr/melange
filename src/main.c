#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);

    WebKitWebContext *cxt = webkit_web_context_get_default();
    webkit_web_context_set_process_model(cxt, WEBKIT_PROCESS_MODEL_SHARED_SECONDARY_PROCESS);

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    GtkWidget *title = gtk_header_bar_new();
    gtk_header_bar_set_title(GTK_HEADER_BAR(title), "Web Messenger");
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(title), "Telegram");
    gtk_window_set_titlebar(GTK_WINDOW(win), title);

    GtkWidget *notebook = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_LEFT);
    gtk_container_add(GTK_CONTAINER(win), notebook);

    GtkWidget *web = webkit_web_view_new();
    WebKitSettings *sett = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(web));
    webkit_settings_set_user_agent(sett, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/56.0.2924.87 Safari/537.36");
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(web), "https://web.telegram.org");

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), web, gtk_label_new("Telegram"));
    gtk_widget_show_all(win);

    GtkWidget *web2 = webkit_web_view_new();
    WebKitSettings *sett2 = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(web2));
    webkit_settings_set_user_agent(sett2, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/56.0.2924.87 Safari/537.36");
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(web2), "https://web.whatsapp.com");

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), web2, gtk_label_new("WhatsApp"));
    gtk_widget_show_all(win);

    gtk_main();
}

