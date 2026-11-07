#include <gtk/gtk.h>

#define COMPLETION_HEIGHT 300
#define ROW_HEIGHT 39
#define SURFACE_RADIUS 24.0
#define SURFACE_OPACITY 0.94
#define SURFACE_PADDING 8

static int main_width = 0;
static int restoring_style = 0;
static GdkScreen *opening_screen = NULL;
static int opening_monitor = 0;
static int completion_marker;
static int glass_marker;
static int widget_marker;
static gboolean composited = FALSE;
static GdkRectangle main_bounds;

static void rounded_path(cairo_t *cr, gdouble x, gdouble y,
                         gdouble width, gdouble height, gdouble radius)
{
    radius = MIN(radius, MIN(width, height) / 2.0);
    const gdouble pi = G_PI;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + width - radius, y + radius, radius, -pi / 2, 0);
    cairo_arc(cr, x + width - radius, y + height - radius, radius, 0, pi / 2);
    cairo_arc(cr, x + radius, y + height - radius, radius, pi / 2, pi);
    cairo_arc(cr, x + radius, y + radius, radius, pi, 3 * pi / 2);
    cairo_close_path(cr);
}

static gboolean draw_surface(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    (void)data;
    if (event->window != gtk_widget_get_window(widget)) return FALSE;

    gint width, height;
    gdk_drawable_get_size(event->window, &width, &height);
    GdkColor base = gtk_widget_get_style(widget)->base[GTK_STATE_NORMAL];
    cairo_t *cr = gdk_cairo_create(event->window);
    gdk_cairo_region(cr, event->region);
    cairo_clip(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    gdouble radius = composited ? SURFACE_RADIUS : 0;
    rounded_path(cr, 0.5, 0.5, width - 1, height - 1, radius);
    cairo_pattern_t *fill = cairo_pattern_create_linear(0, 0, 0, height);
    gdouble red = base.red / 65535.0;
    gdouble green = base.green / 65535.0;
    gdouble blue = base.blue / 65535.0;
    cairo_pattern_add_color_stop_rgb(fill, 0,
        MIN(red + 0.035, 1), MIN(green + 0.035, 1), MIN(blue + 0.045, 1));
    cairo_pattern_add_color_stop_rgb(fill, 1, red, green, blue);
    cairo_set_source(cr, fill);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(fill);

    cairo_pattern_t *rim = cairo_pattern_create_linear(0, 0, 0, height);
    cairo_pattern_add_color_stop_rgba(rim, 0, 1, 1, 1, 0.65);
    cairo_pattern_add_color_stop_rgba(rim, 1, 1, 1, 1, 0.25);
    cairo_set_source(cr, rim);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
    cairo_pattern_destroy(rim);
    if (gtk_window_get_window_type(GTK_WINDOW(widget)) == GTK_WINDOW_TOPLEVEL) {
        PangoLayout *prompt = gtk_widget_create_pango_layout(widget, ">");
        gint prompt_height;
        pango_layout_get_pixel_size(prompt, NULL, &prompt_height);
        cairo_set_source_rgba(cr, 0.741, 0.576, 0.976, 0.95);
        cairo_move_to(cr, SURFACE_PADDING + 10, (height - prompt_height) / 2.0);
        pango_cairo_show_layout(cr, prompt);
        g_object_unref(prompt);
    }
    cairo_destroy(cr);
    return FALSE;
}

static void prepare_surface(GtkWidget *widget)
{
    if (g_object_get_data(G_OBJECT(widget), "gmrun-glass-surface")) return;
    g_object_set_data(G_OBJECT(widget), "gmrun-glass-surface", &glass_marker);
    if (gtk_window_get_window_type(GTK_WINDOW(widget)) == GTK_WINDOW_TOPLEVEL) {
        GtkWidget *content = gtk_bin_get_child(GTK_BIN(widget));
        if (content) {
            GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(widget));
            gint cursor = 0, start = 0, end = 0;
            if (focus && GTK_IS_EDITABLE(focus)) {
                g_object_ref(focus);
                cursor = start = end = gtk_editable_get_position(GTK_EDITABLE(focus));
                gtk_editable_get_selection_bounds(GTK_EDITABLE(focus), &start, &end);
            } else {
                focus = NULL;
            }
            GtkWidget *inset = gtk_alignment_new(0, 0.5, 1, 1);
            gtk_alignment_set_padding(GTK_ALIGNMENT(inset), 0, 0, 36, 0);
            g_object_ref(content);
            gtk_container_remove(GTK_CONTAINER(widget), content);
            gtk_container_add(GTK_CONTAINER(inset), content);
            gtk_container_add(GTK_CONTAINER(widget), inset);
            g_object_unref(content);
            gtk_widget_show(inset);
            if (focus) {
                gtk_widget_grab_focus(focus);
                gtk_editable_select_region(GTK_EDITABLE(focus),
                    cursor == start ? end : start, cursor == start ? start : end);
                g_object_unref(focus);
            } else {
                gtk_widget_child_focus(content, GTK_DIR_TAB_FORWARD);
            }
        }
    }
    gtk_widget_set_app_paintable(widget, TRUE);
    gtk_container_set_border_width(GTK_CONTAINER(widget), SURFACE_PADDING);
    if (composited) gtk_window_set_opacity(GTK_WINDOW(widget), SURFACE_OPACITY);
    g_signal_connect(widget, "expose-event", G_CALLBACK(draw_surface), NULL);
    gtk_widget_queue_draw(widget);
}

static gboolean draw_placeholder(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    (void)data;
    GtkEntry *entry = GTK_ENTRY(widget);
    if (gdk_window_get_parent(event->window) != gtk_widget_get_window(widget) ||
        *gtk_entry_get_text(entry) ||
        *pango_layout_get_text(gtk_entry_get_layout(entry)))
        return FALSE;

    gint x, y, area_x, area_y;
    gtk_entry_get_layout_offsets(entry, &x, &y);
    gdk_window_get_position(event->window, &area_x, &area_y);
    GdkColor color = gtk_widget_get_style(widget)->text[GTK_STATE_NORMAL];
    cairo_t *cr = gdk_cairo_create(event->window);
    PangoLayout *placeholder = gtk_widget_create_pango_layout(widget, "Type request...");
    cairo_set_source_rgba(cr, color.red / 65535.0, color.green / 65535.0,
        color.blue / 65535.0, 0.55);
    cairo_move_to(cr, x - area_x, y - area_y);
    pango_cairo_show_layout(cr, placeholder);
    g_object_unref(placeholder);
    cairo_destroy(cr);
    return FALSE;
}

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
        gboolean surface = g_object_get_data(G_OBJECT(widget), "gmrun-glass-surface") != NULL;
        gtk_container_set_border_width(GTK_CONTAINER(widget), surface ? SURFACE_PADDING : 0);
        gdk_drawable_get_size(window, &width, &height);
        if (toplevel && width > 100) {
            prepare_surface(widget);
            /* gmrun writes GDK_DECOR_BORDER after realizing its window.
             * Reset those hints after setup so GNOME adds no frame/titlebar. */
            gdk_window_set_decorations(window, 0);
            main_width = width;
            if (opening_screen) {
                GdkRectangle monitor;
                gdk_screen_get_monitor_geometry(opening_screen, opening_monitor, &monitor);
                gtk_window_move(GTK_WINDOW(widget), monitor.x + (monitor.width - width) / 2,
                    monitor.y + (monitor.height - height) / 2);
                main_bounds.x = monitor.x + (monitor.width - width) / 2;
                main_bounds.y = monitor.y + (monitor.height - height) / 2;
                main_bounds.width = width;
                main_bounds.height = height;
            }
        }
        if (!toplevel && main_width > 100 &&
            g_object_get_data(widget, "gmrun-completion-window")) {
            gtk_widget_set_size_request(widget, main_width, COMPLETION_HEIGHT);
            gint menu_y = main_bounds.y + main_bounds.height + SURFACE_PADDING;
            if (opening_screen) {
                GdkRectangle monitor;
                gdk_screen_get_monitor_geometry(opening_screen, opening_monitor, &monitor);
                if (menu_y + COMPLETION_HEIGHT > monitor.y + monitor.height)
                    menu_y = MAX(monitor.y, main_bounds.y - COMPLETION_HEIGHT - SURFACE_PADDING);
            }
            gtk_window_move(GTK_WINDOW(widget), main_bounds.x, menu_y);
        }
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
    if (g_object_get_data(G_OBJECT(widget), "gmrun-appearance-ready")) return TRUE;
    g_object_set_data(G_OBJECT(widget), "gmrun-appearance-ready", &widget_marker);
    if (GTK_IS_LABEL(widget))
        g_idle_add(hide_header, g_object_ref(widget));
    if (GTK_IS_ENTRY(widget)) {
        gtk_entry_set_has_frame(GTK_ENTRY(widget), 0);
        restore_theme(widget, NULL, NULL);
        g_signal_connect(widget, "style-set", G_CALLBACK(restore_theme), NULL);
        g_signal_connect_after(widget, "expose-event", G_CALLBACK(draw_placeholder), NULL);
        g_signal_connect_swapped(widget, "changed", G_CALLBACK(gtk_widget_queue_draw), widget);
    }
    if (GTK_IS_CLIST(widget)) {
        gtk_clist_set_row_height(GTK_CLIST(widget), ROW_HEIGHT);
        g_object_set_data(G_OBJECT(gtk_widget_get_toplevel(widget)),
            "gmrun-completion-window", &completion_marker);
        prepare_surface(gtk_widget_get_toplevel(widget));
    }
    if (GTK_IS_SCROLLED_WINDOW(widget))
        gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(widget), GTK_SHADOW_NONE);
    if (!GTK_IS_WINDOW(widget)) return 1;
    int toplevel = gtk_window_get_window_type(GTK_WINDOW(widget)) == GTK_WINDOW_TOPLEVEL;
    if (toplevel) {
        gtk_window_set_decorated(GTK_WINDOW(widget), 0);
        gtk_window_set_position(GTK_WINDOW(widget), GTK_WIN_POS_CENTER_ALWAYS);
        if (gtk_bin_get_child(GTK_BIN(widget))) prepare_surface(widget);
    }
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
    GdkScreen *screen = gdk_screen_get_default();
    GdkColormap *rgba = gdk_screen_get_rgba_colormap(screen);
    composited = rgba && gdk_screen_is_composited(screen);
    if (composited) gtk_widget_set_default_colormap(rgba);
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
