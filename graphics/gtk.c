#include <math.h>
#include <gtk/gtk.h>
#include <cairo.h>
//#include <tl.h>

#if 0
static tlValue _circle(tlFun* fn, tlMap* args) {
    cairo_t *cr = tl_data(tlvm_global_get(tlvm_from_task(task), tlSYM("graphics")));
    int x = tl_int(tlmaps_get_int(args, 0));
    int y = tl_int(tlmaps_get_int(args, 1));
    int r = tl_int(tlmaps_get_int(args, 2));
    if (r <= 0) return;
    cairo_arc(cr, x, y, r, 0, 2*M_PI);
    cairo_fill_preserve(cr);
    cairo_stroke(cr);
    return tlNull;
}
#endif

static void draw(cairo_t *cr) {
    cairo_arc(cr, 100, 100, 50, 0, 2*M_PI);
    cairo_set_source_rgb(cr, 1, 0, 0);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_stroke(cr);

    /*
    tlVm* vm = tlvm_new();
    tlvm_global_set(vm, tlSYM("graphics"), tlDATA(cr));
    tlvm_global_set(vm, tlSYM("circle"), tlFUN(_circle));
    tlvm_run(vm, tlTEXT("circle(100, 100, 50)"));
    //tlvm_run(vm, tltext_from_file("run.tl"));
    tlvm_delete(vm);
    */
}

static gboolean expose_event(GtkWidget* w, GdkEventExpose* e, void* data) {
    cairo_t *cr = gdk_cairo_create(w->window);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);

    draw(cr);

    cairo_destroy(cr);
    return TRUE;
}

int main(int argc, char** argv) {
    gtk_init(&argc, &argv);

    GtkWindow* window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
    gtk_window_set_default_size(window, 200, 200);
    g_signal_connect(window, "expose-event", G_CALLBACK(expose_event), NULL);
    g_signal_connect(window, "delete-event", G_CALLBACK(gtk_main_quit), NULL);
    gtk_window_present(window);
    gtk_main();
    return 0;
}

