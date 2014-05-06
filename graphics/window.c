#include "app.h"
#include "window.h"
#include "graphics.h"

#include <math.h>
#include <cairo/cairo.h>


static tlSym _s_key;
static tlSym _s_input;
static tlMap* _keyEventMap;

struct Box {
    tlLock lock;
    Window* window;
    Box* up;
    Box* prev; Box* next;
    Box* boxes;

    bool dirty;
    float x; float y; float width; float height;
    float cx; float cy; // center
    float r;            // rotate
    float sx; float sy; // scale - 1
    float alpha;

    tlHandle ondraw;
    tlHandle onkey;
    tlHandle onpointer;
};

static tlKind _WindowKind = { .name = "Window", .locked = true, };
tlKind* WindowKind = &_WindowKind;
static tlKind _BoxKind = { .name = "Box", .locked = true, };
tlKind* BoxKind = &_BoxKind;

static void renderBox(cairo_t* c, Box* b, Graphics* g) {
    if (!b) return;
    b->dirty = false;

    cairo_save(c);

    // setup transform and clip
    cairo_translate(c, b->x + b->cx, b->y + b->cy);
    if (fabs(b->r) >= 0.001) cairo_rotate(c, b->r);
    if (fabs(b->sx - 1.0) >= 0.0001 || fabs(b->sy - 1.0) >= 0.0001) cairo_scale(c, b->sx, b->sy);
    cairo_translate(c, -b->cx, -b->cy);
    cairo_rectangle(c, 0.5, 0.5, b->width, b->height);
    cairo_clip(c);

    // call out to render task, it runs on the vm thread, blocking ours until done
    if (b->ondraw) {
        tlBlockingTaskEval(b->window->rendertask, tlCallFrom(b->ondraw, g, null));
    }

    // draw the sub boxes
    for (Box* sb = b->boxes; sb; sb = sb->next) renderBox(c, sb, g);

    cairo_reset_clip(c);
    cairo_restore(c);
}

void renderWindow(Window* window, cairo_t* cairo) {
    assert(window);
    assert(window->rendertask);
    assert(cairo);
    Graphics* g = GraphicsNew(cairo);

    window->dirty = false;
    for (Box* sb = window->root; sb; sb = sb->next) renderBox(cairo, sb, g);
}

void window_dirty(Window* window) {
    if (!window || window->dirty) return;
    window->dirty = true;
    if (window->native) nativeWindowRedraw(window->native);
}
void box_dirty(Box* box) {
    if (!box || box->dirty) return;
    box->dirty = true;
    if (box->up) box_dirty(box->up);
    else if (box->window) window_dirty(box->window);
}

#define ATTACH(b, w) \
        if(b->window) TL_THROW("box is still attached");\
        b->window = w;

Window* WindowNew(int width, int height) {
    Window* window = tlAlloc(WindowKind, sizeof(Window));
    window->rendertask = tlBlockingTaskNew(tlVmCurrent());
    window->width = width;
    window->height = height;

    window->native = nativeWindowNew(window);

    // ensure open windows don't quit the runtime
    tlVmIncExternal(tlVmCurrent());

    return window;
}
static tlHandle __Window_new(tlArgs* args) {
    print("new window");
    int width = tl_int_or(tlArgsGet(args, 0), 0);
    int height = tl_int_or(tlArgsGet(args, 1), 0);
    Window* window = WindowNew(width, height);
    print("done");
    return window;
}
static tlHandle _Window_new(tlArgs* args) {
    toolkit_launch();
    print("tl: new window");
    tlHandle res = tl_on_toolkit(__Window_new, args);
    print("tl: done");
    return res;
}
void closeWindow(Window* window) {
    if (!window->native) return;
    // closed windows should not keep runtime waiting
    tlVmDecExternal(tlVmCurrent());
    window->native = null;
}
static tlHandle _window_close(tlArgs* args) {
    Window* window = WindowAs(tlArgsTarget(args));
    if (window->native) nativeWindowClose(window->native);
    closeWindow(window);
    return tlNull;
}

static tlHandle _window_add(tlArgs* args) {
    Window* window = WindowAs(tlArgsTarget(args));
    Box* box = BoxCast(tlArgsGet(args, 0));
    if (!box) TL_THROW("expect a Box to add");
    ATTACH(box, window);
    box_dirty(box);

    if (!window->root) {
        window->root = box;
        return box;
    }
    Box* b = window->root;
    while (b->next) b = b->next;
    b->next = box;
    box->prev = b;
    return box;
}
static tlHandle _window_remove(tlArgs* args) {
    TL_THROW("not implemented");
}
static tlHandle _window_get(tlArgs* args) {
    TL_THROW("not implemented");
}
static tlHandle _window_up(tlArgs* args) {
    return tlNull;
}
static tlHandle _window_x(tlArgs* args) {
    Window* window = WindowAs(tlArgsTarget(args));
    if (tlArgsSize(args) > 0) {
        window->x = tl_int_or(tlArgsGet(args, 0), 0);
        window_dirty(window);
    }
    return tlINT(window->x);
}
static tlHandle _window_y(tlArgs* args) {
    Window* window = WindowAs(tlArgsTarget(args));
    if (tlArgsSize(args) > 0) {
        window->y = tl_int_or(tlArgsGet(args, 0), 0);
        window_dirty(window);
    }
    return tlINT(window->y);
}
static tlHandle _window_width(tlArgs* args) {
    Window* window = WindowAs(tlArgsTarget(args));
    if (tlArgsSize(args) > 0) {
        window->width = tl_int_or(tlArgsGet(args, 0), 0);
        window_dirty(window);
    }
    return tlINT(window->width);
}
static tlHandle _window_height(tlArgs* args) {
    Window* window = WindowAs(tlArgsTarget(args));
    if (tlArgsSize(args) > 0) {
        window->height = tl_int_or(tlArgsGet(args, 0), 0);
        window_dirty(window);
    }
    return tlINT(window->height);
}
static tlHandle __window_title(tlArgs* args) {
    Window* window = WindowAs(tlArgsTarget(args));
    if (tlArgsSize(args) > 0) {
        tlString* str = tlStringCast(tlArgsGet(args, 0));
        if (!str) str = tlStringEmpty();
        nativeWindowSetTitle(window->native, str);
        return str;
    }
    return nativeWindowTitle(window->native);
}
static tlHandle _window_title(tlArgs* args) { return tl_on_toolkit(__window_title, args); }
static tlHandle _window_visible(tlArgs* args) {
    Window* window = WindowAs(tlArgsTarget(args));
    if (tlArgsSize(args) > 0) {
        bool visible = tl_bool(tlArgsGet(args, 0));
        window->dirty = false;
        window_dirty(window);
        nativeWindowSetVisible(window->native, visible);
        return tlBOOL(visible);
    }
    return tlBOOL(nativeWindowVisible(window->native));
}
static tlHandle __window_focus(tlArgs* args) {
    Window* window = WindowAs(tlArgsTarget(args));
    window->dirty = false;
    window_dirty(window);
    nativeWindowFocus(window->native);
    return tlNull;
}
static tlHandle _window_focus(tlArgs* args) { return tl_on_toolkit(__window_focus, args); }

void windowKeyEvent(Window* window, int code, tlString* input) {
    if (!window->onkey) return;

    tlMap *res = tlClone(_keyEventMap);
    tlMapSetSym_(res, _s_key, tlINT(code));
    tlMapSetSym_(res, _s_input, input);
    tlBlockingTaskEval(window->rendertask, tlCallFrom(window->onkey, res, null));
}

// ** boxes **

Box* BoxNew(int x, int y, int width, int height) {
    Box* box = tlAlloc(BoxKind, sizeof(Box));
    box->x = x; box->y = y; box->width = width; box->height = height;
    box->sx = 1.0;
    box->sy = 1.0;
    box->alpha = 1.0;
    return box;
}
static tlHandle _Box_new(tlArgs* args) {
    int x = 0, y = 0, width = 0, height = 0;
    if (tlArgsSize(args) <= 2) {
        width = tl_int_or(tlArgsGet(args, 0), 0);
        height = tl_int_or(tlArgsGet(args, 1), 0);
    } else {
        x = tl_int_or(tlArgsGet(args, 0), 0);
        y = tl_int_or(tlArgsGet(args, 1), 0);
        width = tl_int_or(tlArgsGet(args, 2), 0);
        height = tl_int_or(tlArgsGet(args, 3), 0);
    }
    Box* box = BoxNew(x, y, width, height);
    // TODO allow more arguments, like ondraw and such
    return box;
}
static tlHandle _box_add(tlArgs* args) {
    Box* up = BoxAs(tlArgsTarget(args));
    Box* box = BoxCast(tlArgsGet(args, 0));
    if (!box) TL_THROW("expect a Box to add");
    ATTACH(box, up->window);
    box_dirty(box);

    if (!up->boxes) {
        up->boxes = box;
        return box;
    }
    Box* b = up->boxes;
    while (b->next) b = b->next;
    b->next = box;
    box->prev = b;
    return box;
}
static tlHandle _box_remove(tlArgs* args) {
    TL_THROW("not implemented");
}
static tlHandle _box_get(tlArgs* args) {
    TL_THROW("not implemented");
}
static tlHandle _box_up(tlArgs* args) {
    return tlNull;
}
static tlHandle _box_x(tlArgs* args) {
    Box* box = BoxAs(tlArgsTarget(args));
    if (tlArgsSize(args) > 0) {
        box->x = tl_double_or(tlArgsGet(args, 0), 0);
        box_dirty(box);
    }
    return tlFLOAT(box->x);
}
static tlHandle _box_y(tlArgs* args) {
    Box* box = BoxAs(tlArgsTarget(args));
    if (tlArgsSize(args) > 0) {
        box->y = tl_double_or(tlArgsGet(args, 0), 0);
        box_dirty(box);
    }
    return tlFLOAT(box->y);
}
static tlHandle _box_width(tlArgs* args) {
    Box* box = BoxAs(tlArgsTarget(args));
    if (tlArgsSize(args) > 0) {
        box->width = tl_double_or(tlArgsGet(args, 0), 0);
        box_dirty(box);
    }
    return tlFLOAT(box->width);
}
static tlHandle _box_height(tlArgs* args) {
    Box* box = BoxAs(tlArgsTarget(args));
    if (tlArgsSize(args) > 0) {
        box->height = tl_double_or(tlArgsGet(args, 0), 0);
        box_dirty(box);
    }
    return tlFLOAT(box->height);
}
static tlHandle _box_center(tlArgs* args) {
    Box* box = BoxAs(tlArgsTarget(args));
    if (tlArgsSize(args) > 0) {
        box->cx = tl_double_or(tlArgsGet(args, 0), 0);
        box->cy = tl_double_or(tlArgsGet(args, 1), 0);
        box_dirty(box);
    }
    return tlResultFrom(tlFLOAT(box->cx), tlFLOAT(box->cy), null);
}
static tlHandle _box_scale(tlArgs* args) {
    Box* box = BoxAs(tlArgsTarget(args));
    if (tlArgsSize(args) > 0) {
        box->sx = tl_double_or(tlArgsGet(args, 0), 0);
        box->sy = tl_double_or(tlArgsGet(args, 1), 0);
        if (box->sx <= 0) box->sx = 0.000000001;
        if (box->sy <= 0) box->sy = 0.000000001;
        box_dirty(box);
    }
    return tlResultFrom(tlFLOAT(box->sx), tlFLOAT(box->sy), null);
}
static tlHandle _box_rotate(tlArgs* args) {
    Box* box = BoxAs(tlArgsTarget(args));
    if (tlArgsSize(args) > 0) {
        box->r = tl_double_or(tlArgsGet(args, 0), 0);
        box_dirty(box);
    }
    return tlFLOAT(box->r);
}
static tlHandle _box_alpha(tlArgs* args) {
    Box* box = BoxAs(tlArgsTarget(args));
    if (tlArgsSize(args) > 0) {
        box->alpha = tl_double_or(tlArgsGet(args, 0), 0);
        box_dirty(box);
    }
    return tlFLOAT(box->alpha);
}

static tlHandle _box_ondraw(tlArgs* args) {
    Box* box = BoxAs(tlArgsTarget(args));
    if (tlArgsSize(args) > 0) {
        box->ondraw = tlArgsGet(args, 0);
        box_dirty(box);
    }
    return box->ondraw?box->ondraw : tlNull;
}
static tlHandle _box_redraw(tlArgs* args) {
    Box* box = BoxAs(tlArgsTarget(args));
    box_dirty(box);
    return tlNull;
}

static tlHandle _window_onkey(tlArgs* args) {
    Window* window = WindowAs(tlArgsTarget(args));
    if (tlArgsSize(args) > 0) window->onkey = tlArgsGet(args, 0);
    return window->onkey?window->onkey : tlNull;
}

void window_init(tlVm* vm) {
    _BoxKind.klass = tlClassMapFrom(
        "add", _box_add,
        "remove", _box_remove,
        "get", _box_get,
        "up", _box_up,

        "x", _box_x,
        "y", _box_y,
        "width", _box_width,
        "height", _box_height,

        "center", _box_center,
        "rotate", _box_rotate,
        "scale", _box_scale,

        "alpha", _box_alpha,

        "redraw", _box_redraw,

        "ondraw", _box_ondraw,
        null
    );
    _WindowKind.klass = tlClassMapFrom(
        "add", _window_add,
        "remove", _window_remove,
        "get", _window_get,
        "up", _window_up,

        "x", _window_x,
        "y", _window_y,
        "width", _window_width,
        "height", _window_height,

        "onkey", _window_onkey,

        "title", _window_title,
        "focus", _window_focus,
        "visible", _window_visible,
        "close", _window_close,
        null
    );

    tlMap* BoxStatic = tlClassMapFrom(
        "new", _Box_new,
        null
    );
    tlMap* WindowStatic = tlClassMapFrom(
        "new", _Window_new,
        null
    );

    tlVmGlobalSet(vm, tlSYM("Box"), BoxStatic);
    tlVmGlobalSet(vm, tlSYM("Window"), WindowStatic);

    // for key events
    tlSet* keys = tlSetNew(2);
    _s_key = tlSYM("key"); tlSetAdd_(keys, _s_key);
    _s_input = tlSYM("input"); tlSetAdd_(keys, _s_input);
    _keyEventMap = tlMapNew(keys);
    tlMapToObject_(_keyEventMap);
}

