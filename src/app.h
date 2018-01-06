#ifndef MELANGE_APP_H
#define MELANGE_APP_H

#include "config.h"
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>


typedef struct MelangeApp MelangeApp;


#define MELANGE_TYPE_APP (melange_app_get_type())
#define MELANGE_APP(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), MELANGE_TYPE_APP, MelangeApp))
#define MELANGE_IS_APP(inst) (G_TYPE_CHECK_INSTANCE_TYPE ((inst), MELANGE_TYPE_APP))


GApplication *melange_app_new(const char *executable_file);

GType melange_app_get_type(void);

WebKitWebContext *melange_app_get_web_context(MelangeApp *app);

GdkPixbuf *melange_app_request_icon(MelangeApp *app, const char *hostname);

gboolean melange_app_add_account(MelangeApp *app, MelangeAccount *account);

void melange_app_iterate_accounts(MelangeApp *app, MelangeAccountConstFunc func,
                                  gpointer user_data);

char *melange_app_get_resource_path(MelangeApp *app, const char *resource);

GdkPixbuf *melange_app_load_pixbuf_resource(MelangeApp *app, const char *resource,
                                            gint width, gint height, gboolean allow_failure);

GtkBuilder *melange_app_load_ui_resource(MelangeApp *app, const char *resource,
                                         gboolean allow_failure);

char *melange_app_load_text_resource(MelangeApp *app, const char *resource,
                                     gboolean allow_failure);


#endif // MELANGE_APP_H
