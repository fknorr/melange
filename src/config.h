#ifndef MELANGE_CONFIG_H
#define MELANGE_CONFIG_H

#include <glib-2.0/glib.h>


typedef enum MelangeCsdMode {
    MELANGE_CSD_OFF,
    MELANGE_CSD_ON,
    MELANGE_CSD_AUTO,
} MelangeCsdMode;

typedef struct MelangeAccount {
    char *id;
    char *preset;

    // Only set if preset == NULL
    char *service_name;
    char *service_url;
    char *icon_url;
    char *user_agent;
} MelangeAccount;

typedef struct MelangeConfig {
    gboolean dark_theme;
    MelangeCsdMode client_side_decorations;
    gboolean auto_hide_sidebar;

    GArray *accounts;
} MelangeConfig;


MelangeConfig *melange_config_new(void);

MelangeConfig *melange_config_new_from_file(const char *file_name);

void melange_config_free(MelangeConfig *config);

void melange_config_add_preset_account(MelangeConfig *config, char *id, char *preset);

void melange_config_add_custom_account(MelangeConfig *config, char *id, char *service_name,
                                       char *service_url, char *icon_url, char *user_agent);

void melange_config_write_to_file(MelangeConfig *config, const char *file_name);


#endif // MELANGE_CONFIG_H
