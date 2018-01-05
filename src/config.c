#include "config.h"
#include <stdio.h>
#include <errno.h>

extern FILE *melange_config_parser_in;
extern MelangeConfig *melange_config_parser_result;
int melange_config_parser_parse(void);


void
melange_clear_account(MelangeAccount *account) {
    g_free(account->id);
    g_free(account->preset);
    g_free(account->service_name);
    g_free(account->service_url);
    g_free(account->icon_url);
    g_free(account->user_agent);
}


MelangeConfig *
melange_config_new(void) {
    MelangeConfig template = {
        .dark_theme = FALSE,
        .client_side_decorations = MELANGE_CSD_AUTO,
        .auto_hide_sidebar = FALSE,
        .accounts = g_array_new(FALSE, FALSE, sizeof(MelangeAccount)),
    };
    g_array_set_clear_func(template.accounts, (GDestroyNotify) melange_clear_account);
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
    g_array_free(config->accounts, TRUE);
    g_free(config);
}


void
melange_config_add_preset_account(MelangeConfig *config, char *id, char *preset) {
    MelangeAccount account = {
        .id = id,
        .preset = preset,
    };
    g_array_append_val(config->accounts, account);
}


void
melange_config_add_custom_account(MelangeConfig *config, char *id, char *service_name,
                                  char *service_url, char *icon_url, char *user_agent)
{
    MelangeAccount account = {
        .id = id,
        .service_name = service_name,
        .service_url = service_url,
        .icon_url = icon_url,
        .user_agent = user_agent,
    };
    g_array_append_val(config->accounts, account);
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

    for (size_t i = 0; i < config->accounts->len; ++i) {
        MelangeAccount *account = &g_array_index(config->accounts, MelangeAccount, i);
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

    fclose(file);
}
