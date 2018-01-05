const style = document.createElement('style');
style.type = 'text/css';
style.appendChild(document.createTextNode(`
    .tg_head_split, .im_page_wrap {
        max-width: none !important;
    }
    .im_dialogs_col_wrap {
        max-width: 400px !important;
    }
    .im_message_wrap {
        max-width: 800px !important;
    }
`));
document.head.appendChild(style);
