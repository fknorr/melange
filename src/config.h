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
    const char *preset;
    char *service_name;
    char *service_url;
    char *icon_url;
    char *user_agent;
} MelangeAccount;

typedef struct MelangeConfig {
    gboolean dark_theme;
    MelangeCsdMode client_side_decorations;
    gboolean auto_hide_sidebar;

    GHashTable *accounts;
} MelangeConfig;

typedef void (*MelangeAccountFunc)(MelangeAccount *account, gpointer user_data);
typedef void (*MelangeAccountConstFunc)(const MelangeAccount *account, gpointer user_data);


MelangeAccount *melange_account_new_from_preset(char *id, const char *preset);

MelangeAccount *melange_account_new(char *id, char *service_name, char *service_url,
                                    char *icon_url, char *user_agent);

void melange_account_free(MelangeAccount *account);


MelangeConfig *melange_config_new(void);

MelangeConfig *melange_config_new_from_file(const char *file_name);

void melange_config_free(MelangeConfig *config);

gboolean melange_config_add_account(MelangeConfig *config, MelangeAccount *account);

MelangeAccount *melange_config_lookup_account(MelangeConfig *config, const char *id);

void melange_config_for_each_account(MelangeConfig *config, MelangeAccountFunc func,
                                     gpointer user_data);

void melange_config_write_to_file(MelangeConfig *config, const char *file_name);


#endif // MELANGE_CONFIG_H
