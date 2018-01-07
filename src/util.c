#include "util.h"


gboolean
melange_util_has_dark_background(GtkWidget *widget) {
    // Hack. This will not work for all themes.
    GValue value = { 0 };
    gtk_style_context_get_property(gtk_widget_get_style_context(widget), "background-color",
            GTK_STATE_NORMAL, &value);
    GdkRGBA *bg = g_value_get_boxed(&value);
    return bg->red + bg->green + bg->blue < 1.5;
}
