#include <pthread.h>
#include <cairo.h>
#include <gtk/gtk.h>

#include "vm/tl.h"
#include "graphics.h"
#include "image.h"
#include "window.h"
#include "app.h"

static void configure_window(GtkWindow* window, GdkEvent* event, gpointer data) {
    int x = event->configure.x;
    int y = event->configure.y;
    int width = event->configure.width;
    int height = event->configure.height;
    windowResizeEvent(WindowAs(g_object_get_data(G_OBJECT(window), "tl")), x, y, width, height);
}

static gboolean draw_window(GtkWidget* w, GdkEventExpose* e, void* data) {
    cairo_t *cr = gdk_cairo_create(gtk_widget_get_window(w));
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);

    renderWindow(WindowAs(g_object_get_data(G_OBJECT(w), "tl")), cr);

    cairo_destroy(cr);
    return TRUE;
}
static gboolean destroy_window(GtkWindow* w) {
    trace("destroy window: %p", w);
    closeWindow(WindowAs(g_object_get_data(G_OBJECT(w), "tl")));
    return FALSE;
}
static gboolean key_release_window(GtkWidget* widget, GdkEventKey* event, gpointer data) {
    return FALSE;
}
static gboolean key_press_window(GtkWidget* widget, GdkEventKey* event, gpointer data) {
    trace("key_press window: %p", widget);
    int key = event->keyval;
    const char* input = event->string;
    trace("HAVE A KEY PRESS: %d %s", key, input);
    switch (key) {
        case GDK_KEY_Escape: key = 27; break;
        case GDK_KEY_BackSpace: key = 8; break;
        case GDK_KEY_Return: key = 13; break;
        case GDK_KEY_Left: key = 37; break;
        case GDK_KEY_Up: key = 38; break;
        case GDK_KEY_Right: key = 39; break;
        case GDK_KEY_Down: key = 40; break;
    }
    if (key >= 'a' && key <= 'z') key = key & ~0x20;

    if (key < 0 || key > 4000) return FALSE;
    windowKeyEvent(WindowAs(g_object_get_data(G_OBJECT(widget), "tl")), key, tlStringFromCopy(input, 0));
    return FALSE;
}

NativeWindow* nativeWindowNew(Window* window) {
    GtkWindow* w = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));

    g_signal_connect(w, "draw", G_CALLBACK(draw_window), NULL);
    g_signal_connect(w, "destroy", G_CALLBACK(destroy_window), NULL);
    g_signal_connect(w, "key_press_event", G_CALLBACK(key_press_window), NULL);
    g_signal_connect(w, "key_release_event", G_CALLBACK(key_release_window), NULL);
    g_signal_connect(w, "configure-event", G_CALLBACK(configure_window), NULL);
    g_object_set_data(G_OBJECT(w), "tl", window);

    gtk_window_resize(GTK_WINDOW(w), window->width, window->height);
    gtk_window_set_position(GTK_WINDOW(w), GTK_WIN_POS_CENTER);
    gtk_window_present(GTK_WINDOW(w));
    gtk_widget_hide(GTK_WIDGET(w));

    return w;
}

void nativeWindowClose(NativeWindow* w) {
    gtk_window_close(GTK_WINDOW(w));
}

void nativeWindowFocus(NativeWindow* w) {
    gtk_widget_show(GTK_WIDGET(w));
    gtk_window_activate_focus(GTK_WINDOW(w));
}

void nativeWindowSetVisible(NativeWindow* w, bool visible) {
    if (visible) {
        gtk_widget_show(GTK_WIDGET(w));
    } else {
        gtk_widget_hide(GTK_WIDGET(w));
    }
}
int nativeWindowVisible(NativeWindow* w) {
    return gtk_widget_is_visible(GTK_WIDGET(w));
}

bool nativeWindowFullScreen(NativeWindow* w) {
    GdkWindowState state = gdk_window_get_state(gtk_widget_get_window(GTK_WIDGET(w)));
    return (state & GDK_WINDOW_STATE_FULLSCREEN) == GDK_WINDOW_STATE_FULLSCREEN;
}
void nativeWindowSetFullScreen(NativeWindow* w, bool full) {
    if (full) {
        gtk_window_fullscreen(GTK_WINDOW(w));
    } else {
        gtk_window_unfullscreen(GTK_WINDOW(w));
    }
}

static gboolean redraw(gpointer w) {
    gdk_window_invalidate_rect(gtk_widget_get_window(GTK_WIDGET(w)), NULL, true);
    return FALSE;
}
void nativeWindowRedraw(NativeWindow* w) {
    gdk_threads_add_idle(redraw, w);
}

void nativeWindowFrame(NativeWindow* w, int* x, int* y, int* width, int* height) {
    gtk_window_get_size(GTK_WINDOW(w), width, height);
    gtk_window_get_position(GTK_WINDOW(w), x, y);
}

void nativeWindowSetPos(NativeWindow* w, int x, int y) {
    gtk_window_move(GTK_WINDOW(w), x, y);
}

void nativeWindowSetSize(NativeWindow* w, int width, int height) {
    gtk_window_resize(GTK_WINDOW(w), width, height);
}

tlString* nativeWindowTitle(NativeWindow* w) {
    return tlStringFromCopy(gtk_window_get_title(GTK_WINDOW(w)), 0);
}

void nativeWindowSetTitle(NativeWindow* w, tlString* title) {
    gtk_window_set_title(GTK_WINDOW(w), tlStringData(title));
}

NativeTextBox* nativeTextBoxNew(NativeWindow* window, int x, int y, int width, int height) {
    return null;
}
void nativeTextBoxPosition(NativeTextBox* textbox, int x, int y, int with, int height) {
}
tlString* nativeTextBoxGetText(NativeTextBox* textbox) {
    return tlStringEmpty();
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
    gdk_threads_add_idle(run_started, null);
    gtk_main();
}

