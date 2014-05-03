#include <pthread.h>
#include <cairo.h>
#include <gtk/gtk.h>

#include "vm/tl.h"
#include "graphics.h"
#include "image.h"
#include "window.h"
#include "app.h"

static void draw(cairo_t *cr) {
    tl_init();
    tlVm* vm = tlVmNew();
    graphics_init(vm);
    image_init(vm);
    tlVmInitDefaultEnv(vm);
    tlVmGlobalSet(vm, tlSYM("g"), GraphicsNew(cr));
    tlArgs* args = tlArgsNew(tlListEmpty(), null);
    tlVmEvalCode(vm, tlSTR("g.color(255,0,0);g.arc(50,50,50);g.fill(true);g.color(0,0,0);g.stroke"), tlSTR("hello"), args);
    tlVmDelete(vm);
}

static gboolean draw_window(GtkWidget* w, GdkEventExpose* e, void* data) {
    cairo_t *cr = gdk_cairo_create(gtk_widget_get_window(w));
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);

    renderWindow(WindowAs(g_object_get_data(G_OBJECT(w), "tl")), cr);

    cairo_destroy(cr);
    return TRUE;
}

NativeWindow* nativeWindowNew(Window* window) {
    GtkWindow* w = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));

    g_signal_connect(w, "draw", G_CALLBACK(draw_window), NULL);
    g_signal_connect(w, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_object_set_data(G_OBJECT(w), "tl", window);

    return w;
}

void nativeWindowClose(NativeWindow* w) {
    gtk_window_close(GTK_WINDOW(w));
}

void nativeWindowFocus(NativeWindow* w) {
    //gtk_window_focus(GTK_WINDOW(w));
    gtk_window_present(GTK_WINDOW(w));
}

void nativeWindowSetVisible(NativeWindow* w, bool visible) {
    if (visible) {
        gtk_window_present(GTK_WINDOW(w));
    } else {
        //gtk_window_set_visible(GTK_WINDOW(w), visible);
    }
}

int nativeWindowVisible(NativeWindow* w) {
    //return gtk_window_visible(GTK_WINDOW(w));
    return true;
}

void nativeWindowRedraw(NativeWindow* w) {
    //gtk_window_invalidate(GTK_WINDOW(w));
}

void nativeWindowFrame(NativeWindow* w, int* x, int* y, int* width, int* height) {
    gtk_window_get_size(GTK_WINDOW(w), x, y);
    //gtk_window_get_frame(GTK_WINDOW(w), x, y, width, height)
}

void nativeWindowSetPos(NativeWindow* w, int x, int y) {
    //gtk_window_set_pos(GTK_WINDOW(w), x, y);
}

void nativeWindowSetSize(NativeWindow* w, int width, int height) {
    //gtk_window_set_size(GTK_WINDOW(w), width, height);
}

tlString* nativeWindowTitle(NativeWindow* w) {
    return tlStringFromCopy(gtk_window_get_title(GTK_WINDOW(w)), 0);
}

void nativeWindowSetTitle(NativeWindow* w, tlString* title) {
    gtk_window_set_title(GTK_WINDOW(w), tlStringData(title));
}

// platform can call the following for callbacks
void windowPointerEvent(Window* w);
void windowKeyEvent(Window* w, int code, tlString* input);

gboolean run_on_main(gpointer _onmain) {
    tlRunOnMain* onmain = (tlRunOnMain*)_onmain;
    onmain->result = onmain->cb(onmain->args);
    toolkit_schedule_done(onmain);
    return false;
}

void toolkit_schedule(tlRunOnMain* onmain) {
    gdk_threads_add_idle(run_on_main, onmain);
}

void toolkit_init(int argc, char** argv) {
    gtk_init(&argc, &argv);
}

void toolkit_stop() {
    gtk_main_quit();
}

gboolean run_started(gpointer _data) {
    toolkit_started();
    return false;
}

void toolkit_start() {
    print("starting gtk");
    gdk_threads_add_idle(run_started, null);
    gtk_main();
    print("stopping gtk");
}

