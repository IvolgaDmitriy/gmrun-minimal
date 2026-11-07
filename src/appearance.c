#include <gtk/gtk.h>

#define COMPLETION_HEIGHT 300
#define ROW_HEIGHT 39

static int main_width = 0;
static int restoring_style = 0;
static GdkScreen *opening_screen = NULL;
static int opening_monitor = 0;
static int completion_marker;

static void restore_theme(GtkWidget *widget, GtkStyle *previous_style, gpointer data)
{
    (void)previous_style;
    (void)data;
    /* Honor the RC palette when gmrun assigns a copied GtkStyle. */
    if (restoring_style) return;
    restoring_style = 1;
    gtk_widget_set_style(widget, NULL);
    restoring_style = 0;
}

static gboolean hide_header(gpointer widget)
{
    GtkWidget *top = gtk_widget_get_toplevel(widget);
    /* Preserve labels in the entry's Copy/Paste context menu. */
    if (GTK_IS_WINDOW(top) &&
        gtk_window_get_window_type(GTK_WINDOW(top)) == GTK_WINDOW_TOPLEVEL) {
        gtk_widget_set_no_show_all(widget, 1);
        gtk_widget_hide(widget);
    }
    g_object_unref(widget);
    return 0;
}

static gboolean shown(GSignalInvocationHint *hint, guint count, const GValue *values, gpointer data)
{
    (void)hint;
    (void)count;
    (void)data;
    GtkWidget *widget = g_value_get_object(values);
    if (GTK_IS_LABEL(widget))
        g_idle_add(hide_header, g_object_ref(widget));
    return 1;
}

static gboolean layout_window(gpointer widget)
{
    GdkWindow *window = gtk_widget_get_window(widget);
    if (window) {
        int width, height;
        int toplevel = gtk_window_get_window_type(GTK_WINDOW(widget)) == GTK_WINDOW_TOPLEVEL;
        /* The application resets its border after realization. Apply our
         * spacing once layout has completed, including after popup resize. */
        gtk_container_set_border_width(GTK_CONTAINER(widget), 0);
        gdk_drawable_get_size(window, &width, &height);
        if (toplevel && width > 100) {
            /* gmrun writes GDK_DECOR_BORDER after realizing its window.
             * Reset those hints after setup so GNOME adds no frame/titlebar. */
            gdk_window_set_decorations(window, 0);
            main_width = width;
            if (opening_screen) {
                GdkRectangle monitor;
                gdk_screen_get_monitor_geometry(opening_screen, opening_monitor, &monitor);
                gtk_window_move(GTK_WINDOW(widget), monitor.x + (monitor.width - width) / 2,
                    monitor.y + (monitor.height - height) / 2);
            }
        }
        if (!toplevel && main_width > 100 &&
            g_object_get_data(widget, "gmrun-completion-window"))
            gtk_widget_set_size_request(widget, main_width, COMPLETION_HEIGHT);
    }
    g_object_unref(widget);
    return 0;
}

static void resized(GtkWidget *widget, GtkAllocation *allocation, gpointer data)
{
    (void)allocation;
    (void)data;
    g_idle_add(layout_window, g_object_ref(widget));
}

static gboolean realized(GSignalInvocationHint *hint, guint count, const GValue *values, gpointer data)
{
    (void)hint;
    (void)count;
    (void)data;
    GtkWidget *widget = g_value_get_object(values);
    if (GTK_IS_LABEL(widget))
        g_idle_add(hide_header, g_object_ref(widget));
    if (GTK_IS_ENTRY(widget)) {
        gtk_entry_set_has_frame(GTK_ENTRY(widget), 0);
        restore_theme(widget, NULL, NULL);
        g_signal_connect(widget, "style-set", G_CALLBACK(restore_theme), NULL);
    }
    if (GTK_IS_CLIST(widget)) {
        gtk_clist_set_row_height(GTK_CLIST(widget), ROW_HEIGHT);
        g_object_set_data(G_OBJECT(gtk_widget_get_toplevel(widget)),
            "gmrun-completion-window", &completion_marker);
    }
    if (GTK_IS_SCROLLED_WINDOW(widget))
        gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(widget), GTK_SHADOW_NONE);
    if (!GTK_IS_WINDOW(widget)) return 1;
    int toplevel = gtk_window_get_window_type(GTK_WINDOW(widget)) == GTK_WINDOW_TOPLEVEL;
    if (toplevel) {
        gtk_window_set_decorated(GTK_WINDOW(widget), 0);
        gtk_window_set_position(GTK_WINDOW(widget), GTK_WIN_POS_CENTER_ALWAYS);
    }
    gtk_container_set_border_width(GTK_CONTAINER(widget), 0);
    g_signal_connect_after(widget, "size-allocate", G_CALLBACK(resized), NULL);
    g_idle_add(layout_window, g_object_ref(widget));
    return 1;
}

G_MODULE_EXPORT void gtk_module_init(gint *argc, gchar ***argv)
{
    (void)argc;
    (void)argv;
    int pointer_x, pointer_y;
    unsigned int pointer_mask;
    gdk_display_get_pointer(gdk_display_get_default(), &opening_screen,
        &pointer_x, &pointer_y, &pointer_mask);
    if (opening_screen)
        opening_monitor = gdk_screen_get_monitor_at_point(opening_screen, pointer_x, pointer_y);
    gchar *config = g_build_filename(g_get_user_config_dir(), "gmrun-minimal", "gtkrc", NULL);
    gtk_rc_parse(config);
    g_free(config);
    /* Initialize the class before looking up its signals on a plain theme. */
    gpointer widget_class = g_type_class_ref(GTK_TYPE_WIDGET);
    unsigned int signal = g_signal_lookup("realize", gtk_widget_get_type());
    g_signal_add_emission_hook(signal, 0, realized, NULL, NULL);
    signal = g_signal_lookup("show", gtk_widget_get_type());
    g_signal_add_emission_hook(signal, 0, shown, NULL, NULL);
    g_type_class_unref(widget_class);
}
