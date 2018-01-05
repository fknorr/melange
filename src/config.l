%option prefix="melange_config_parser_"

%{
#define MELANGE_CONFIG_PARSER_STYPE void *
#define yylval melange_config_parser_lval

#include "config.tab.h"
#include <glib-2.0/glib.h>

#pragma GCC diagnostic ignored "-Wunused-function"
%}

%%

\"[^\"\n]*\"    yylval = g_strdup(yytext + 1); ((char *) yylval)[strlen(yylval) - 1] = 0; return T_STRING;
[a-z-]+         yylval = g_strdup(yytext); return T_IDENTIFIER;
\{              return T_LEFT_BRACE;
\}              return T_RIGHT_BRACE;
\n              return T_LINE_FEED;
[ \t]+          /* ignore other whitespace */

%%
