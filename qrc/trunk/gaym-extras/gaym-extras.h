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

#include "../gaym/src/gaym.h"
struct fetch_thumbnail_data {
    const char *who;
    char *pic_data;
    gint pic_data_len;
    gboolean from_file;
};

struct paint_data {
    char *tooltiptext;
    const char *name;
    GaimAccount* account;
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
    SORT_ALPHA,
    SORT_ENTRY,
    SORT_CATEGORY,
} GaymSortOrder;

typedef enum {
    TOOLTIP_CHAT,
    TOOLTIP_IM,
} GaymTooltipType;

struct timeout_cb_data {
    GaymTooltipType type;
    GtkWidget *tv;
    struct gaym_conn *gaym;
};


void get_icon_scale_size(GdkPixbuf* icon, GaimBuddyIconSpec * spec,int *width, int *height);
void clean_popup_stuff(GaimConversation * c);
void add_chat_icon_stuff(GaimConversation *c);
void add_chat_popup_stuff(GaimConversation *c);
void add_chat_sort_functions(GaimConversation *c);
void add_im_popup_stuff(GaimConversation* c);
void init_chat_icons();
void init_popups();
