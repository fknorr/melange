#ifndef MELANGE_UTIL_H
#define MELANGE_UTIL_H

#include <gtk/gtk.h>


#ifdef __GNUC__
#define GLADE_EVENT_HANDLER __attribute__((used))
#endif


gboolean melange_util_has_dark_background(GtkWidget *widget);


#endif //MELANGE_UTIL_H
