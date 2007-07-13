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
sort_chat_users_by_pic(GtkTreeModel * model, GtkTreeIter * a,
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


static struct gaym_sort_orders {
    const char *icon;
    void *sort_funcion;
    const char *tooltip;
} const order[] = {
    {GAYM_STOCK_ENTRY, sort_chat_users_by_entry,
     _("Current sorting by entry")},
    {GAYM_STOCK_ALPHA, sort_chat_users_by_alpha,
     _("Current sorting by alpha")},
    {GAYM_STOCK_PIC, sort_chat_users_by_pic, _("Current sorting by pic")}
};


void change_sort_order(GtkWidget * button, void *data)
{

    static int current = 0;
    current = (current + 1) % G_N_ELEMENTS(order);
    GaimGtkConversation *gtkconv = (GaimGtkConversation *) data;
    GaimGtkChatPane *gtkchat = gtkconv->u.chat;
    GtkTreeModel *model =
        gtk_tree_view_get_model(GTK_TREE_VIEW(gtkchat->list));

    GtkBox *buttonbox = GTK_BOX(button->parent);
    gtk_widget_destroy(button);
    button = GTK_WIDGET(gaim_gtkconv_button_new(order[current].icon, NULL,      // _("E"), 
                                                                                // 
                                                // 
                                                // 
                                                order[current].tooltip,
                                                gtkconv->tooltips,
                                                change_sort_order,
                                                gtkconv));
    gtk_box_pack_end(buttonbox, button, FALSE, FALSE, 0);
    gtk_widget_show(button);
    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(model),
                                    CHAT_USERS_NAME_COLUMN,
                                    order[current].sort_funcion, NULL,
                                    NULL);



}

void add_chat_sort_functions(GaimConversation * c)
{

    GaimGtkConversation *gtkconv = GAIM_GTK_CONVERSATION(c);

    GtkBox *iconbox = (GtkBox *) gtkconv->info->parent;
    // GtkWidget *button = gtk_button_new_with_label("E");
    GtkWidget *button = gaim_gtkconv_button_new(GAYM_STOCK_ENTRY,
                                                NULL,   // _("E"), 
                                                _
                                                ("Currently sorting by entry"),
                                                gtkconv->tooltips,
                                                change_sort_order,
                                                gtkconv);
    gtk_box_pack_end(iconbox, button, FALSE, FALSE, 0);
    gtk_widget_show(button);


}
