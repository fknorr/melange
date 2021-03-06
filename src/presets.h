#ifndef MELANGE_PRESETS_H
#define MELANGE_PRESETS_H

#include "config.h"


extern const MelangeAccount melange_account_presets[];
extern size_t melange_n_account_presets;


const MelangeAccount *melange_account_presets_lookup(const char *id);


#endif //MELANGE_PRESETS_H
