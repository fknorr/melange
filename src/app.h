#ifndef MELANGE_APP_H
#define MELANGE_APP_H

#include <gtk/gtk.h>


typedef struct MelangeApp MelangeApp;


#define MELANGE_TYPE_APP (melange_app_get_type())
#define MELANGE_APP(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), MELANGE_TYPE_APP, MelangeApp))


GApplication *melange_app_new(void);

GType melange_app_get_type(void);


#endif // MELANGE_APP_H
