/* Puts last 4k of log in new conversations a la Everybuddy (and then
   stolen by Trillian "Pro") */

#include "internal.h"
#include "gtkgaim.h"

#include "conversation.h"
#include "debug.h"
#include "log.h"
#include "prefs.h"
#include "signals.h"
#include "util.h"
#include "version.h"
#include "prpl.h"

#include "gtkconv.h"
#include "gtkimhtml.h"
#include "gtkplugin.h"
#include "gtkdialogs.h"
#include "gtkutils.h"
#include "gtkblist.h"
#include "gtkimhtmltoolbar.h"
#include <gdk/gdkkeysyms.h>

#define CHATSORT_PLUGIN_ID "gtk-chatsort"
#define CHATSORT_USERS_COLUMNS 4
#define CHATSORT_USERS_ENTRY_COLUMN 3

/**
 * Unused variables:
 *
 * static GList *browsers = NULL;
 */

struct RoomBrowseInfo {

    GaimAccount *account;
    GaimConnection *gc;
};

static GtkWidget *setup_roombrowse_pane(GaimConversation * conv)
{
    GaimGtkConversation *gtkconv;
    GaimGtkChatPane *gtkchat;
    GaimConnection *gc;
    GtkWidget *vpaned, *hpaned;
    GtkWidget *vbox;

        /**
	 * Unused variables:
	 *
	 * GaimPluginProtocolInfo *prpl_info = NULL;
	 * GtkWidget *hbox;
	 * GtkWidget *lbox, *bbox;
	 * GtkWidget *label;
	 * GtkWidget *list;
	 * GtkWidget *button;
	 * GtkWidget *sw;
	 * GtkListStore *ls;
	 * GtkCellRenderer *rend;
	 * GtkTreeViewColumn *col;
	 * GList *focus_chain = NULL;
	 */

    gtkconv = GAIM_GTK_CONVERSATION(conv);
    gtkchat = gtkconv->u.chat;
    gc = gaim_conversation_get_gc(conv);

    /* Setup the outer pane. */
    vpaned = gtk_vpaned_new();
    gtk_widget_show(vpaned);
    /* Setup the top part of the pane. */
    vbox = gtk_vbox_new(FALSE, 6);
    gtk_paned_pack1(GTK_PANED(vpaned), vbox, TRUE, TRUE);
    gtk_widget_show(vbox);

    /* Setup the horizontal pane. */
    hpaned = gtk_hpaned_new();
    gtk_box_pack_start(GTK_BOX(vbox), hpaned, TRUE, TRUE, 0);
    gtk_widget_show(hpaned);

    /* Setup the scrolled window to put gtkimhtml in. */
    gtkconv->sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(gtkconv->sw),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(gtkconv->sw),
                                        GTK_SHADOW_IN);
    gtk_paned_pack1(GTK_PANED(hpaned), gtkconv->sw, TRUE, TRUE);

    gtk_widget_set_size_request(gtkconv->sw,
                                gaim_prefs_get_int
                                ("/gaim/gtk/conversations/chat/default_width"),
                                gaim_prefs_get_int
                                ("/gaim/gtk/conversations/chat/default_height"));

    // g_signal_connect(G_OBJECT(gtkconv->sw), "size-allocate",
    // G_CALLBACK(size_allocate_cb), conv);

    gtk_widget_show(gtkconv->sw);

    return vpaned;
}

static gint close_conv_cb(GtkWidget * w, gpointer d)
{
    GaimConversation *conv = (GaimConversation *) d;

    gaim_conversation_destroy(conv);

    return TRUE;
}

GdkPixbuf *get_tab_icon(GaimConversation * conv, gboolean small_icon)
{
    GaimAccount *account = NULL;
    const char *name = NULL;
    GdkPixbuf *status = NULL;

    g_return_val_if_fail(conv != NULL, NULL);

    account = gaim_conversation_get_account(conv);
    name = gaim_conversation_get_name(conv);

    g_return_val_if_fail(account != NULL, NULL);
    g_return_val_if_fail(name != NULL, NULL);


    if (gaim_conversation_get_type(conv) == GAIM_CONV_IM) {
        GaimBuddy *b = gaim_find_buddy(account, name);
        if (b != NULL) {
            status = gaim_gtk_blist_get_status_icon((GaimBlistNode *) b,
                                                    (small_icon ?
                                                     GAIM_STATUS_ICON_SMALL
                                                     :
                                                     GAIM_STATUS_ICON_LARGE));
        }
    }

    if (!status) {
        GdkPixbuf *pixbuf;
        pixbuf = create_prpl_icon(account);

        if (small_icon && pixbuf != NULL) {
            status = gdk_pixbuf_scale_simple(pixbuf, 15, 15,
                                             GDK_INTERP_BILINEAR);
            g_object_unref(pixbuf);
        } else
            status = pixbuf;
    }
    return status;
}

/**
 * Unused function
 */
#if 0
static void update_tab_icon(GaimConversation * conv)
{
    GaimGtkConversation *gtkconv;
    GaimConvWindow *win = gaim_conversation_get_window(conv);
    GaimAccount *account;
    const char *name;
    GdkPixbuf *status = NULL;

    g_return_if_fail(conv != NULL);

    gtkconv = GAIM_GTK_CONVERSATION(conv);
    name = gaim_conversation_get_name(conv);
    account = gaim_conversation_get_account(conv);

    status = get_tab_icon(conv, TRUE);

    g_return_if_fail(status != NULL);

    gtk_image_set_from_pixbuf(GTK_IMAGE(gtkconv->icon), status);
    gtk_image_set_from_pixbuf(GTK_IMAGE(gtkconv->menu_icon), status);

    if (status != NULL)
        g_object_unref(status);

    if (gaim_conv_window_get_active_conversation(win) == conv &&
        gtkconv->u.im->anim == NULL) {
        status = get_tab_icon(conv, FALSE);

        gtk_window_set_icon(GTK_WINDOW(GAIM_GTK_WINDOW(win)->window),
                            status);

        if (status != NULL)
            g_object_unref(status);
    }
}
#endif

/* Courtesy of Galeon! */
static void
tab_close_button_state_changed_cb(GtkWidget * widget,
                                  GtkStateType prev_state)
{
    if (GTK_WIDGET_STATE(widget) == GTK_STATE_ACTIVE)
        gtk_widget_set_state(widget, GTK_STATE_NORMAL);
}

static void
roombrowse_gtk_add_conversation(GaimConvWindow * win,
                                GaimConversation * conv)
{
    GaimGtkWindow *gtkwin;
    GaimGtkConversation *gtkconv, *focus_gtkconv;
    GaimConversation *focus_conv;
    GtkWidget *pane = NULL;
    GtkWidget *tab_cont;
    GtkWidget *tabby, *menu_tabby;
    GtkWidget *close_image;
    gboolean new_ui;
    GaimConversationType conv_type;
    const char *name;

    name = gaim_conversation_get_name(conv);
    conv_type = gaim_conversation_get_type(conv);
    gtkwin = GAIM_GTK_WINDOW(win);

    if (conv->ui_data != NULL) {
        gtkconv = (GaimGtkConversation *) conv->ui_data;

        tab_cont = gtkconv->tab_cont;

        new_ui = FALSE;
    } else {
        gtkconv = g_malloc0(sizeof(GaimGtkConversation));
        conv->ui_data = gtkconv;

        /* Setup some initial variables. */
        gtkconv->sg = gtk_size_group_new(GTK_SIZE_GROUP_BOTH);
        gtkconv->tooltips = gtk_tooltips_new();

        gaim_debug_misc("roombrowse", "setting up pane\n");
        pane = setup_roombrowse_pane(conv);

        gaim_debug_misc("roombrowse", "set up pane\n");

        if (pane == NULL) {
            g_free(gtkconv);
            conv->ui_data = NULL;

            return;
        }



        /* Setup the container for the tab. */
        gtkconv->tab_cont = tab_cont = gtk_vbox_new(FALSE, 6);
        gtk_container_set_border_width(GTK_CONTAINER(tab_cont), 6);
        gtk_container_add(GTK_CONTAINER(tab_cont), pane);
        gtk_widget_show(pane);

        new_ui = TRUE;

        gtkconv->make_sound = FALSE;
        gtkconv->show_formatting_toolbar = FALSE;
        gtkconv->show_timestamps = FALSE;

        g_signal_connect_swapped(G_OBJECT(pane), "focus",
                                 G_CALLBACK(gtk_widget_grab_focus),
                                 gtkconv->entry);
    }

    gaim_debug_misc("roombrowse", "Setting up tabs\n");
    gtkconv->tabby = tabby = gtk_hbox_new(FALSE, 6);
    gtkconv->menu_tabby = menu_tabby = gtk_hbox_new(FALSE, 6);
    gtkconv->entry = gtk_imhtml_new(NULL, NULL);
    gtkconv->toolbar = gtk_imhtmltoolbar_new();

    gaim_debug_misc("roombrowse", "Setting up close button\n");
    /* Close button. */
    gtkconv->close = gtk_button_new();
    gtk_widget_set_size_request(GTK_WIDGET(gtkconv->close), 16, 16);
    gtk_button_set_relief(GTK_BUTTON(gtkconv->close), GTK_RELIEF_NONE);
    close_image =
        gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
    gtk_widget_show(close_image);
    gtk_container_add(GTK_CONTAINER(gtkconv->close), close_image);
    gtk_tooltips_set_tip(gtkconv->tooltips, gtkconv->close,
                         _("Close conversation"), NULL);

    g_signal_connect(G_OBJECT(gtkconv->close), "clicked",
                     G_CALLBACK(close_conv_cb), conv);

    /* 
     * I love Galeon. They have a fix for that stupid annoying visible
     * border bug. I love you guys! -- ChipX86
     */
    g_signal_connect(G_OBJECT(gtkconv->close), "state_changed",
                     G_CALLBACK(tab_close_button_state_changed_cb), NULL);

    /* Status icon. */
    gtkconv->icon = gtk_image_new();
    gtkconv->menu_icon = gtk_image_new();
    // update_tab_icon(conv);

    /* Tab label. */
    gtkconv->tab_label = gtk_label_new(gaim_conversation_get_title(conv));
    gtkconv->menu_label = gtk_label_new(gaim_conversation_get_title(conv));
#if 0
    gtk_misc_set_alignment(GTK_MISC(gtkconv->tab_label), 0.00, 0.5);
    gtk_misc_set_padding(GTK_MISC(gtkconv->tab_label), 4, 0);
#endif

    gaim_debug_misc("roombrowse", "Packing\n");
    /* Pack it all together. */
    gtk_box_pack_start(GTK_BOX(tabby), gtkconv->icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(menu_tabby), gtkconv->menu_icon,
                       FALSE, FALSE, 0);

    gtk_widget_show_all(gtkconv->icon);
    gtk_widget_show_all(gtkconv->menu_icon);

    gtk_box_pack_start(GTK_BOX(tabby), gtkconv->tab_label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(menu_tabby), gtkconv->menu_label, TRUE,
                       TRUE, 0);
    gtk_widget_show(gtkconv->tab_label);
    gtk_widget_show(gtkconv->menu_label);
    gtk_misc_set_alignment(GTK_MISC(gtkconv->menu_label), 0, 0);

    gtk_box_pack_start(GTK_BOX(tabby), gtkconv->close, FALSE, FALSE, 0);
    if (gaim_prefs_get_bool("/gaim/gtk/conversations/close_on_tabs"))
        gtk_widget_show(gtkconv->close);

    gtk_widget_show(tabby);
    gtk_widget_show(menu_tabby);

    if (gaim_conversation_get_type(conv) == GAIM_CONV_IM)
        gaim_gtkconv_update_buddy_icon(conv);

    gaim_debug_misc("roombrowse", "Adding to notebook\n");
    gaim_debug_misc("roombrowse", "gtkwin->notebook=%x\n",
                    gtkwin->notebook);
    gaim_debug_misc("roombrowse", "gtkwin=%x\n", gtkwin);
    gaim_debug_misc("roombrowse", "tabby=%x\n", tabby);
    gaim_debug_misc("roombrowse", "menu_tabby=%x\n", menu_tabby);
    gaim_debug_misc("roombrowse", "tab_cont=%x\n", tab_cont);

    /* Add this pane to the conversation's notebook. */
    int n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(gtkwin->notebook));
    gaim_debug_misc("roombrowse:", "Notebook has %d pages\n", n);
    gtk_notebook_append_page_menu(GTK_NOTEBOOK(gtkwin->notebook), tab_cont,
                                  tabby, menu_tabby);
    gaim_debug_misc("roombrowse", "Got through append_page_menu\n");
    gtk_widget_show(tab_cont);

    if (gaim_conv_window_get_conversation_count(win) == 1) {
        /* Er, bug in notebooks? Switch to the page manually. */
        gtk_notebook_set_current_page(GTK_NOTEBOOK(gtkwin->notebook), 0);

        gtk_notebook_set_show_tabs(GTK_NOTEBOOK(gtkwin->notebook),
                                   gaim_prefs_get_bool
                                   ("/gaim/gtk/conversations/tabs"));
    } else
        gtk_notebook_set_show_tabs(GTK_NOTEBOOK(gtkwin->notebook), TRUE);
    gaim_debug_misc("roombrowse", "FOcus stuff\n");
    focus_conv = g_list_nth_data(gaim_conv_window_get_conversations(win),
                                 gtk_notebook_get_current_page(GTK_NOTEBOOK
                                                               (gtkwin->
                                                                notebook)));
    focus_gtkconv = GAIM_GTK_CONVERSATION(focus_conv);
    gtk_widget_grab_focus(focus_gtkconv->entry);

    if (!new_ui)
        g_object_unref(gtkconv->tab_cont);
}


static void roombrowse_menu_cb(GaimBlistNode * node, gpointer data)
{
    GaimConvWindow *win = gaim_get_first_window_with_type(GAIM_CONV_MISC);
    GaimConversation *conv = g_new0(GaimConversation, 1);

    GaimAccount *account = ((GaimChat *) node)->account;
    if (!win)
        win = gaim_conv_window_new();
    GaimChat *chat = ((GaimChat *) node);
    char *room = g_strdup(g_hash_table_lookup(chat->components, "name"));


    gaim_debug_misc("roombrowser", "In cb with node=%x, account=%x\n",
                    node, account);
    conv = gaim_conversation_new(GAIM_CONV_MISC, account, room);

    gaim_conversation_set_logging(conv, FALSE);

    roombrowse_gtk_add_conversation(conv->window, conv);
    gaim_conv_window_show(conv->window);

    g_free(room);

}
static void roombrowse_menu_create(GaimBlistNode * node, GList ** menu)
{

    char *label, *room;

    struct gaym_conn *gaym;
    GaimChat *chat = (GaimChat *) node;

    gaim_debug_misc("roombrowse", "In callback\n");
    if (node->type != GAIM_BLIST_CHAT_NODE)
        return;

    gaym = chat->account->gc->proto_data;

    room = g_strdup(g_hash_table_lookup(chat->components, "name"));
    gaim_debug_misc("roombrowse", "Room name: %s\n", room);
    if (!room)
        return;


    label = g_strdup_printf("Lurk in %s", room);
    GaimBlistNodeAction *act = gaim_blist_node_action_new(label,
                                                          roombrowse_menu_cb,
                                                          chat->account);

    *menu = g_list_append(*menu, act);
    // g_free(label);
}
static gboolean plugin_load(GaimPlugin * plugin)
{
    gaim_signal_connect(gaim_blist_get_handle(),
                        "blist-node-extended-menu",
                        plugin, GAIM_CALLBACK(roombrowse_menu_create),
                        NULL);


    gaim_debug_misc("roombrowse", "Callback registered!\n");
    return TRUE;
}

static GaimPluginInfo info = {
    GAIM_PLUGIN_MAGIC,
    GAIM_MAJOR_VERSION,
    GAIM_MINOR_VERSION,
    GAIM_PLUGIN_STANDARD,
    GAIM_GTK_PLUGIN_TYPE,
    0,
    NULL,
    GAIM_PRIORITY_DEFAULT,
    CHATSORT_PLUGIN_ID,
    N_("Gay.Com Room Browser"),
    VERSION,
    N_("Browse rooms in gay.com"),
    N_("Adds a right-click item to context menu."),
    "Jason LeBrun <gaim@jasonlebrun.info",
    GAIM_WEBSITE,
    plugin_load,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static void init_plugin(GaimPlugin * plugin)
{
}

GAIM_INIT_PLUGIN(history, init_plugin, info)
