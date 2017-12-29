#include "app.h"


int
main(int argc, char **argv) {
    GApplication *app = melange_app_new();
    int status = g_application_run(app, argc, argv);
    g_object_unref(app);
    return status;
}
