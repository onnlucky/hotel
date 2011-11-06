#include <math.h>

#include "graphics.h"
#include "../tl.h"

TL_REF_TYPE(Graphics);

struct Graphics {
    tlHead head;
    cairo_t* cairo;
};

static tlClass _GraphicsClass;
tlClass* GraphicsClass = &_GraphicsClass;

static tlValue _circle(tlTask* task, tlArgs* args) {
    print("!! HERE !!");
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_t* cr = g->cairo;
    int x = tl_int_or(tlArgsAt(args, 0), 0);
    int y = tl_int_or(tlArgsAt(args, 1), 0);
    int r = tl_int_or(tlArgsAt(args, 2), -1);
    if (r <= 0) return tlNull;
    cairo_arc(cr, x, y, r, 0, 2*M_PI);
    cairo_fill_preserve(cr);
    cairo_stroke(cr);
    return tlNull;
}

static tlValue _rgb(tlTask* task, tlArgs* args) {
    Graphics* gr = GraphicsAs(tlArgsTarget(args));
    cairo_t* cr = gr->cairo;
    float r = tl_int_or(tlArgsAt(args, 0), 0)/255.0;
    float g = tl_int_or(tlArgsAt(args, 1), 0)/255.0;
    float b = tl_int_or(tlArgsAt(args, 2), 0)/255.0;
    cairo_set_source_rgb(cr, r, g, b);
    return tlNull;
}

void test_graphics(cairo_t* cr) {
    tl_init();

    _GraphicsClass.map = tlClassMapFrom(
        "rgb", _rgb,
        "circle", _circle,
        null
    );

    Graphics* g = tlAlloc(null, GraphicsClass, sizeof(Graphics));
    g->cairo = cr;

    tlVm* vm = tlVmNew();
    tlVmGlobalSet(vm, tlSYM("g"), g);
    tlVmRun(vm, tlTEXT("g.rgb(20, 20, 150); g.circle(100, 100, 50)"));
    tlVmDelete(vm);
    print("DONE!!");
}

#include "../vm.c"

