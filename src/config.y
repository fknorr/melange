%define api.prefix {melange_config_parser_}

%{
#include <glib-2.0/glib.h>
#include <stdio.h>

#include <src/config.h>

#define MELANGE_CONFIG_PARSER_STYPE void *

#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"


typedef struct KeyValuePair {
    char *key;
    char *value;
} KeyValuePair;


typedef struct Block {
    char *type;
    GArray *items;
} Block;


static KeyValuePair *
key_value_pair_new(char *key, char *value) {
    KeyValuePair template = {
        .key = key,
        .value = value,
    };
    return g_memdup(&template, sizeof template);
}

static void
key_value_pair_destroy(KeyValuePair **pair) {
    g_free((*pair)->key);
    g_free((*pair)->value);
    g_free(*pair);
}


static Block *
block_new(char *type, GArray *items) {
    Block template = {
        .type = type,
        .items = items,
    };
    return g_memdup(&template, sizeof template);
}

static void
block_free(Block *block) {
    g_free(block->type);
    g_array_free(block->items, TRUE);
    g_free(block);
}


static void
read_boolean(const char *str, gboolean *out) {
    if (g_str_equal(str, "0") || g_str_equal(str, "false") || g_str_equal(str, "no")
        || g_str_equal(str, "off")) {
        *out = FALSE;
    } else if (g_str_equal(str, "1") || g_str_equal(str, "true") || g_str_equal(str, "yes")
               || g_str_equal(str, "on")) {
        *out = TRUE;
    } else {
        g_warning("Invalid boolean value \"%s\", skipping", str);
    }
}


static void
read_csd(const char *str, MelangeCsdMode *out) {
    if (g_str_equal(str, "off")) {
        *out = MELANGE_CSD_OFF;
    } else if (g_str_equal(str, "on")) {
        *out = MELANGE_CSD_ON;
    } else if (g_str_equal(str, "auto")) {
        *out = MELANGE_CSD_AUTO;
    } else {
        g_warning("Invalid client-side-decorations value \"%s\", skipping", str);
    }
}


static void
move_ptr(void *dest, void *src) {
    g_free(*(void**) dest);
    *(void**) dest = *(void**) src;
    *(void**) src = NULL;
}


int melange_config_parser_lex(void);

MelangeConfig *melange_config_parser_result;

void
melange_config_parser_error(const char *str) {
    g_warning("Unable to parse configuration: %s", str);
}

int
melange_config_parser_wrap(void) {
    return 1;
}

%}

%token T_STRING T_IDENTIFIER T_LEFT_BRACE T_RIGHT_BRACE T_LINE_FEED

%%

config_file:
    blank_lines
    {
        melange_config_parser_result = melange_config_new();
    }
    | config_file block blank_lines
    {
        Block *block = (Block *) $2;
        MelangeConfig *config = (MelangeConfig *) melange_config_parser_result;
        if (g_str_equal(block->type, "settings")) {
            for (size_t i = 0; i < block->items->len; ++i) {
                KeyValuePair *kv = g_array_index(block->items, KeyValuePair *, i);
                if (g_str_equal(kv->key, "dark-theme")) {
                    read_boolean(kv->value, &config->dark_theme);
                } else if  (g_str_equal(kv->key, "client-side-decorations")) {
                    read_csd(kv->value, &config->client_side_decorations);
                } else if (g_str_equal(kv->key, "auto-hide-sidebar")) {
                    read_boolean(kv->value, &config->auto_hide_sidebar);
                } else {
                    g_warning("Ignoring unknown setting %s in configuration", kv->key);
                }
            }
        } else if (g_str_equal(block->type, "account")) {
            enum { UNKNOWN, KNOWN_PRESET, KNOWN_CUSTOM } type = UNKNOWN;
            char *id = NULL, *preset = NULL, *service_name = NULL, *service_url = NULL,
                    *icon_url = NULL, *user_agent = NULL;
            gboolean skip = FALSE;
            for (size_t i = 0; i < block->items->len; ++i) {
                KeyValuePair *kv = g_array_index(block->items, KeyValuePair *, i);
                if (g_str_equal(kv->key, "id")) {
                    move_ptr(&id, &kv->value);
                } else if (g_str_equal(kv->key, "preset")) {
                    if (type == KNOWN_CUSTOM) {
                        g_warning("Setting preset in custom account, skipping this account");
                        skip = TRUE;
                        break;
                    }
                    type = KNOWN_PRESET;
                    move_ptr(&preset, &kv->value);
                } else if (g_str_equal(kv->key, "service-name") || g_str_equal(kv->key, "service-url")
                        || g_str_equal(kv->key, "icon-url") || g_str_equal(kv->key, "user-agent")) {
                    if (type == KNOWN_PRESET) {
                        g_warning("Setting custom values in preset account, skipping this account");
                        skip = TRUE;
                        break;
                    }
                    type = KNOWN_CUSTOM;
                    if (g_str_equal(kv->key, "service-name")) {
                        move_ptr(&service_name, &kv->value);
                    } else if (g_str_equal(kv->key, "service-url")) {
                        move_ptr(&service_url, &kv->value);
                    } else if (g_str_equal(kv->key, "icon-url")) {
                        move_ptr(&icon_url, &kv->value);
                    } else if (g_str_equal(kv->key, "user-agent")) {
                        move_ptr(&user_agent, &kv->value);
                    }
                } else {
                    g_warning("Ignoring unknown account detail %s", kv->key);
                }
            }
            if (!skip) {
                switch (type) {
                    case UNKNOWN:
                        g_warning("Ignoring incomplete account in configuration");
                        break;

                    case KNOWN_PRESET:
                        if (id && preset) {
                            melange_config_add_preset_account(config, id, preset);
                            id = preset = NULL;
                        } else {
                            g_warning("Ignoring incomplete account in configuration");
                        }
                        break;

                    case KNOWN_CUSTOM:
                        if (id && service_name && service_url && icon_url && user_agent) {
                            melange_config_add_custom_account(config, id, service_name,
                                    service_url, icon_url, user_agent);
                            id = service_name = service_url = icon_url = user_agent = NULL;
                        } else {
                            g_warning("Ignoring incomplete account in configuration");
                        }
                        break;
                }
            }
            g_free(id);
            g_free(preset);
            g_free(service_name);
            g_free(service_url);
            g_free(icon_url);
            g_free(user_agent);
        } else {
            g_warning("Ignoring unknown configuration block \"%s\"", block->type);
        }
        block_free(block);
    }
    ;

block:
    T_IDENTIFIER T_LEFT_BRACE line_break kv_lines T_RIGHT_BRACE
    {
        $$ = block_new($1, $4);
    }
    ;

line_break:
    T_LINE_FEED
    | line_break T_LINE_FEED
    ;

blank_lines:
    %empty
    | line_break
    ;

kv_lines:
    %empty
    {
        $$ = g_array_new(FALSE, FALSE, sizeof(KeyValuePair *));
        g_array_set_clear_func($$, (GDestroyNotify) key_value_pair_destroy);
    }
    | kv_lines key_value line_break
    {
        g_array_append_val($1, $2);
        $$ = $1;
    }
    ;

key_value : T_IDENTIFIER T_STRING
    {
        $$ = key_value_pair_new($1, $2);
    }
    ;
