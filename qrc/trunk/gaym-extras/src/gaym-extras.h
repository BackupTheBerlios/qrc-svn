#include "internal.h"
#include "gtkgaim.h"

#include "conversation.h"
#include "debug.h"
#include "prefs.h"
#include "signals.h"
#include "util.h"
#include "version.h"
#include "buddyicon.h"
#include "prpl.h"

#include "gtkconv.h"
#include "gtkplugin.h"

#ifndef GAYM_EXTRAS_H
#define GAYM_EXTRAS_H

#define GAYM_EXTRAS_PLUGIN_ID "gtk-gaym-extras"

#define GAYM_STOCK_ALPHA "alpha"
#define GAYM_STOCK_ENTRY "entry"
#define GAYM_STOCK_PIC "pic"

// Messy.
#ifdef _WIN32
#include "win32/win32dep.h"
#else
#define DATADIR GAIM_DATADIR
#endif

struct fetch_thumbnail_data {
    const char *who;
    char *pic_data;
    gint pic_data_len;
    gboolean from_file;
};

struct paint_data {
    char *tooltiptext;
    GdkPixbuf *pixbuf;
};

// Additional UI info for a conversation.
// We may be able to clean this up, some.
typedef struct _GaymChatIcon {

    GaimConversation *conv;
    GtkWidget *icon_container_parent;
    GtkWidget *icon_container;
    GtkWidget *frame;
    GtkWidget *icon;
    GtkWidget *event;
    gboolean show_icon;

} GaymChatIcon;


typedef enum {
    TOOLTIP_CHAT,
    TOOLTIP_IM,
} GaymTooltipType;

struct timeout_cb_data {
    GaymTooltipType type;
    GtkWidget *tv;
    GaimAccount *account;
};


GdkPixbuf *lookup_cached_thumbnail(GaimAccount * account,
                                   const char *fullname);
void get_icon_scale_size(GdkPixbuf * icon, GaimBuddyIconSpec * spec,
                         int *width, int *height);
void clean_popup_stuff(GaimConversation * c);
void add_chat_icon_stuff(GaimConversation * c);
void add_chat_popup_stuff(GaimConversation * c);
void add_chat_sort_functions(GaimConversation * c);
void add_im_popup_stuff(GaimConversation * c);
void init_chat_icons(GaimPlugin * plugin);
void init_popups();
void init_roombrowse(GaimPlugin * plugin);


static struct StockIcon {
    const char *name;
    const char *dir;
    const char *filename;

} const stock_icons[] = {
    {GAYM_STOCK_ALPHA, "gaym", "alpha.png"},
    {GAYM_STOCK_ENTRY, "gaym", "entry.png"},
    {GAYM_STOCK_PIC, "gaym", "pic.png"}
};

#endif                          // GAYM_EXTRAS_H
