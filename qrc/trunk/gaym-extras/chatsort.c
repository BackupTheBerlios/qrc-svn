#include "gaym-extras.h"

static gint
sort_chat_users_by_entry(GtkTreeModel * model, GtkTreeIter * a,
                         GtkTreeIter * b, gpointer userdata)
{
    GaimConvChatBuddyFlags f1 = 0, f2 = 0;
    char *user1 = NULL, *user2 = NULL;
    gint ret = 0;

    gtk_tree_model_get(model, a, CHAT_USERS_NAME_COLUMN, &user1,
                       CHAT_USERS_FLAGS_COLUMN, &f1, -1);
    gtk_tree_model_get(model, b, CHAT_USERS_NAME_COLUMN, &user2,
                       CHAT_USERS_FLAGS_COLUMN, &f2, -1);

    if (user1 == NULL || user2 == NULL) {
        if (!(user1 == NULL && user2 == NULL))
            ret = (user1 == NULL) ? -1 : 1;
    } else if (f1 != f2) {
        /* sort more important users first */
        ret = (f1 > f2) ? -1 : 1;
    } else {
        ret = g_utf8_collate(user1, user2);
    }

    g_free(user1);
    g_free(user2);
    return ret;
}

static gint
sort_chat_users_by_alpha(GtkTreeModel * model, GtkTreeIter * a,
                         GtkTreeIter * b, gpointer userdata)
{
    char *user1 = NULL, *user2 = NULL;
    gint ret = 0;

    gtk_tree_model_get(model, a, CHAT_USERS_NAME_COLUMN, &user1, -1);
    gtk_tree_model_get(model, b, CHAT_USERS_NAME_COLUMN, &user2, -1);

    if (user1 == NULL || user2 == NULL) {
        if (!(user1 == NULL && user2 == NULL))
            ret = (user1 == NULL) ? -1 : 1;
    } else {
        ret = g_utf8_collate(user1, user2);
    }

    g_free(user1);
    g_free(user2);
    return ret;
}


static gint
sort_chat_users_by_category(GtkTreeModel * model, GtkTreeIter * a,
                            GtkTreeIter * b, gpointer userdata)
{
    GaimConvChatBuddyFlags f1 = 0, f2 = 0;
    gint flag_mask = 0x000F;
    char *user1 = NULL, *user2 = NULL;
    gint ret = 0;

    gtk_tree_model_get(model, a, CHAT_USERS_NAME_COLUMN, &user1,
                       CHAT_USERS_FLAGS_COLUMN, &f1, -1);
    gtk_tree_model_get(model, b, CHAT_USERS_NAME_COLUMN, &user2,
                       CHAT_USERS_FLAGS_COLUMN, &f2, -1);

    f1 = f1 & flag_mask;
    f2 = f2 & flag_mask;

    if (user1 == NULL || user2 == NULL) {
        if (!(user1 == NULL && user2 == NULL))
            ret = (user1 == NULL) ? -1 : 1;
    } else if (f1 != f2) {
        /* sort more important users first */
        ret = (f1 > f2) ? -1 : 1;
    } else {
        ret = g_utf8_collate(user1, user2);
    }

    g_free(user1);
    g_free(user2);
    return ret;
}



void change_sort_order(GtkWidget * button, void *data)
{

    static GaymSortOrder order = SORT_ENTRY;


    GtkTreeView *list = (GtkTreeView *) data;
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(list));
    gaim_debug_misc("chatsort", "list: %x, data: %x, model: %x\n", list,
                    data, model);
    if (order == SORT_ALPHA) {
        order = SORT_CATEGORY;
        gaim_debug_misc("chatsort", "Change to entry order");
        gtk_button_set_label(GTK_BUTTON(button), "E");
        gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(model),
                                        CHAT_USERS_NAME_COLUMN,
                                        sort_chat_users_by_category, NULL,
                                        NULL);
    } else if (order == SORT_CATEGORY) {
        order = SORT_ENTRY;
        gaim_debug_misc("chatsort", "Change to category order");
        gtk_button_set_label(GTK_BUTTON(button), "P");
        gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(model),
                                        CHAT_USERS_NAME_COLUMN,
                                        sort_chat_users_by_entry, NULL,
                                        NULL);
    } else {
        order = SORT_ALPHA;
        gaim_debug_misc("chatsort", "Change to alpha order");
        gtk_button_set_label(GTK_BUTTON(button), "A");
        gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(model),
                                        CHAT_USERS_NAME_COLUMN,
                                        sort_chat_users_by_alpha, NULL,
                                        NULL);
    }

}
void add_chat_sort_functions(GaimConversation * c)
{

    GaimGtkConversation *gtkconv = GAIM_GTK_CONVERSATION(c);
    GaimGtkChatPane *gtkchat = gtkconv->u.chat;

    GtkBox *iconbox = (GtkBox *) gtkconv->info->parent;
    GtkWidget *button = gtk_button_new_with_label("E");
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_box_pack_end(iconbox, button, FALSE, FALSE, 0);
    gtk_widget_show(button);
    g_signal_connect(G_OBJECT(button), "clicked",
                     G_CALLBACK(change_sort_order), gtkchat->list);
    gaim_debug_misc("chatsort", "Connected signal with data %x\n",
                    gtkchat->list);


}
