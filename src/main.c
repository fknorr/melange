#include "app.h"


int
main(int argc, char **argv) {
    g_set_application_name("Melange");

    char *executable_file;
    if (g_path_is_absolute(argv[0])) {
        executable_file = g_strdup(argv[0]);
    } else {
        executable_file = g_build_path(G_DIR_SEPARATOR_S, g_get_current_dir(),
                                               argv[0], NULL);
    }

    GApplication *app = melange_app_new(executable_file);
    int status = g_application_run(app, argc, argv);
    g_object_unref(app);

    g_free(executable_file);

    return status;
}
