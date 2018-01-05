#include "presets.h"


#define DEFAULT_USER_AGENT "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 " \
            "(KHTML, like Gecko) Chrome/63.0.3239.108 Safari/537.36"


const MelangeAccount melange_account_presets[] = {
    {
        .preset = "whatsapp",
        .service_name = "WhatsApp",
        .service_url = "https://web.whatsapp.com",
        .icon_url = "https://web.whatsapp.com/favicon.ico",
        .user_agent = DEFAULT_USER_AGENT,
    },
    {
        .preset = "telegram",
        .service_name = "Telegram",
        .service_url = "https://web.telegram.org",
        .icon_url = "https://web.telegram.org/favicon.ico",
        .user_agent = DEFAULT_USER_AGENT,
    },
    {
        .preset = "skype",
        .service_name = "Skype",
        .service_url = "https://web.skype.com",
        .icon_url = "https://upload.wikimedia.org/wikipedia/commons/e/ec/Skype-icon-new.png",
        .user_agent = DEFAULT_USER_AGENT,
    },
    {
        .preset = "facebook",
        .service_name = "Facebook",
        .service_url = "https://www.messenger.com",
        .icon_url = "https://static.xx.fbcdn.net/rsrc.php/yl/r/H3nktOa7ZMg.ico",
        .user_agent = DEFAULT_USER_AGENT,
    },
    {
        .preset = "icq",
        .service_name = "ICQ",
        .service_url = "https://web.icq.com",
        .icon_url = "https://web.icq.com/images/icq_logo_124x130.png",
        .user_agent = DEFAULT_USER_AGENT,
    },
};


size_t melange_n_account_presets = sizeof melange_account_presets
                                   / sizeof *melange_account_presets;
