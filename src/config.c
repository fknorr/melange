#include "config.h"
#include "presets.h"
#include <stdio.h>
#include <errno.h>


typedef struct AccountIteratorPointers {
    MelangeAccountFunc func;
    gpointer user_data;
} AccountIteratorPointers;


extern FILE *melange_config_parser_in;
extern MelangeConfig *melange_config_parser_result;
int melange_config_parser_parse(void);


MelangeAccount *
melange_account_new_from_preset(char *id, const char *preset_name) {
    const MelangeAccount *preset = NULL;
    for (size_t i = 0; i < melange_n_account_presets; ++i) {
        if (g_str_equal(melange_account_presets[i].preset, preset_name)) {
            preset = &melange_account_presets[i];
            break;
        }
    }

    if (preset) {
        MelangeAccount *account = g_memdup(preset, sizeof *preset);
        account->id = id;
        return account;
    } else {
        g_warning("Unknown account preset %s", preset_name);
        return NULL;
    }
}


MelangeAccount *
melange_account_new(char *id, char *service_name, char *service_url, char *icon_url,
                    char *user_agent)
{
    MelangeAccount account = {
        .id = id,
        .service_name = service_name,
        .service_url = service_url,
        .icon_url = icon_url,
        .user_agent = user_agent,
    };
    return g_memdup(&account, sizeof account);
}


void
melange_account_free(MelangeAccount *account) {
    g_free(account->id);
    if (!account->preset) {
        g_free(account->service_name);
        g_free(account->service_url);
        g_free(account->icon_url);
        g_free(account->user_agent);
    }
    g_free(account);
}


MelangeConfig *
melange_config_new(void) {
    MelangeConfig template = {
        .dark_theme = FALSE,
        .client_side_decorations = MELANGE_CSD_AUTO,
        .auto_hide_sidebar = FALSE,
        .accounts = g_hash_table_new_full(g_str_hash, g_str_equal, NULL,
                                          (GDestroyNotify) melange_account_free),
    };
    return g_memdup(&template, sizeof template);
}


MelangeConfig *
melange_config_new_from_file(const char *file_name) {
    FILE *file = fopen(file_name, "r");
    if (!file) {
        if (errno != ENOENT) {
            g_warning("Unable to open existing config file %s: %s", file_name, g_strerror(errno));
        }
        return NULL;
    }

    melange_config_parser_in = file;
    melange_config_parser_result = NULL;
    int status = melange_config_parser_parse();

    fclose(file);

    if (status == 0) {
        return melange_config_parser_result;
    } else {
        melange_config_free(melange_config_parser_result);
        return NULL;
    }
}


void melange_config_free(MelangeConfig *config) {
    g_hash_table_destroy(config->accounts);
    g_free(config);
}


gboolean
melange_config_add_account(MelangeConfig *config, MelangeAccount *account) {
    return g_hash_table_insert(config->accounts, account->id, account);
}


MelangeAccount *
melange_config_lookup_account(MelangeConfig *config, const char *id) {
    return g_hash_table_lookup(config->accounts, id);
}


static void
melange_config_hash_table_iterator(gpointer key, gpointer value, gpointer user_data) {
    (void) key;

    AccountIteratorPointers *pointers = user_data;
    pointers->func(value, pointers->user_data);
}


void
melange_config_for_each_account(MelangeConfig *config, MelangeAccountFunc func,
                                gpointer user_data)
{
    AccountIteratorPointers pointers = { func, user_data };
    g_hash_table_foreach(config->accounts, melange_config_hash_table_iterator, &pointers);
}


static void
melange_config_write_account(MelangeAccount *account, FILE *file) {
    fprintf(file,
            "\naccount {\n"
                    "    id            \"%s\"\n",
            account->id
    );
    if (account->preset) {
        fprintf(file,
                "    preset        \"%s\"\n"
                        "}\n",
                account->preset
        );
    } else {
        fprintf(file,
                "    service-name  \"%s\"\n"
                        "    service-url   \"%s\"\n"
                        "    icon-url      \"%s\"\n"
                        "    user-agent    \"%s\"\n"
                        "}\n",
                account->service_name,
                account->service_url,
                account->icon_url,
                account->user_agent
        );
    }
}


void
melange_config_write_to_file(MelangeConfig *config, const char *file_name) {
    char *path = g_path_get_dirname(file_name);
    if (g_mkdir_with_parents(path, 0777) != 0) {
        g_warning("Unable to create config directory %s: %s", path, g_strerror(errno));
        g_free(path);
        return;
    }
    g_free(path);

    FILE *file = fopen(file_name, "w");
    if (!file) {
        g_warning("Unable to write config file %s: %s", file_name, g_strerror(errno));
        return;
    }

    static const char *bool_string[] = { "false", "true" };
    static const char *csd_string[] = { "off", "on", "auto" };

    fprintf(file,
            "settings {\n"
            "    dark-theme               \"%s\"\n"
            "    client-side-decorations  \"%s\"\n"
            "    auto-hide-sidebar        \"%s\"\n"
            "}\n",
            bool_string[config->dark_theme],
            csd_string[config->client_side_decorations],
            bool_string[config->auto_hide_sidebar]
    );

    melange_config_for_each_account(config, (MelangeAccountFunc) melange_config_write_account,
                                    file);
    fclose(file);
}
