#include <math.h>

#include "graphics.h"
#include "image.h"

#define PI (3.141592653589793)
#define TAU (PI*2)

#define CAIRO_STATUS(c) if (cairo_status(c)) warning("CAIRO_ERROR: %s", cairo_status_to_string(cairo_status(c)));

static tlSym s_winding;
static tlSym s_even_odd;

static tlSym s_butt;
static tlSym s_round;
static tlSym s_square;
static tlSym s_bevel;
static tlSym s_clear;

static tlSym s_source;
static tlSym s_over;
static tlSym s_in;
static tlSym s_out;
static tlSym s_atop;
static tlSym s_dest;
static tlSym s_over;
static tlSym s_in;
static tlSym s_out;
static tlSym s_atop;
static tlSym s_xor;
static tlSym s_add;
static tlSym s_saturate;
static tlSym s_overlay;
static tlSym s_darken;
static tlSym s_lighten;
static tlSym s_color_dodge;
static tlSym s_color_burn;
static tlSym s_hard_light;
static tlSym s_soft_light;
static tlSym s_difference;
static tlSym s_exclusion;
static tlSym s_hsl_hue;
static tlSym s_hsl_saturation;
static tlSym s_hsl_color;
static tlSym s_hsl_luminosity;

struct Graphics {
    tlLock lock;
    cairo_surface_t* surface;
    cairo_t* cairo;
};

static tlKind _GraphicsKind = {
    .name = "Graphics",
    .locked = true,
};
tlKind* GraphicsKind = &_GraphicsKind;

void graphicsResize(Graphics* g, int width, int height) {
    cairo_t* cr = g->cairo;
    cairo_surface_t* surface = g->surface;
    int oldwidth = 0;
    int oldheight = 0;

    if (surface) {
        oldwidth = cairo_image_surface_get_width(surface);
        oldheight = cairo_image_surface_get_height(surface);
        if (oldwidth == width && oldheight == height) return;
    }

    g->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    assert(g->surface);
    g->cairo = cairo_create(g->surface);
    assert(g->cairo);

    if (surface) {
        cairo_save(g->cairo);
        cairo_set_source_surface(g->cairo, surface, 0, 0);
        cairo_rectangle(g->cairo, 0, 0, MIN(oldwidth, width), MIN(oldheight, height));
        cairo_fill(g->cairo);
        cairo_restore(g->cairo);

        cairo_destroy(cr);
        cairo_surface_destroy(surface);
    }
}

void graphicsData(Graphics* g, uint8_t** buf, int* width, int* height, int* stride) {
    if (!g->surface) return;
    if (buf) *buf = cairo_image_surface_get_data(g->surface);
    if (width) *width = cairo_image_surface_get_width(g->surface);
    if (height) *height = cairo_image_surface_get_height(g->surface);
    if (stride) *stride = cairo_image_surface_get_stride(g->surface);
}

// TODO call this from finalizer, but actually libgc will take care of it right now
void graphicsDelete(Graphics* g) {
    if (g->cairo) { cairo_destroy(g->cairo); g->cairo = null; }
    if (g->surface) { cairo_surface_destroy(g->surface); g->surface = null; }
}

Graphics* GraphicsNew(cairo_t* cairo) {
    trace("new graphics; width: %d, height: %d", width, height);
    Graphics* g = tlAlloc(GraphicsKind, sizeof(Graphics));
    //graphicsResize(g, width, height);
    g->cairo = cairo;
    return g;
}

/*
static tlHandle _Graphics_new(tlArgs* args) {
    int width = tl_int_or(tlArgsGet(args, 0), 0);
    int height = tl_int_or(tlArgsGet(args, 1), 0);
    if (width < 0) width = 0;
    if (height < 0) height = 0;

    return GraphicsNew(width, height);
}
*/

static tlHandle _width(tlArgs* args) {
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    return tlINT(cairo_image_surface_get_width(g->surface));
}
static tlHandle _height(tlArgs* args) {
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    return tlINT(cairo_image_surface_get_height(g->surface));
}

static tlHandle _resize(tlArgs* args) {
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    int width = tl_int_or(tlArgsGet(args, 0), 0);
    int height = tl_int_or(tlArgsGet(args, 1), 0);
    if (width < 0) width = 0;
    if (height < 0) height = 0;
    graphicsResize(g, width, height);
    return tlNull;
}

static tlHandle _clear(tlArgs* args) {
    Graphics* g = GraphicsAs(tlArgsTarget(args));

    cairo_surface_flush(g->surface);
    uint8_t* data = cairo_image_surface_get_data(g->surface);
    int stride = cairo_image_surface_get_stride(g->surface);
    int height = cairo_image_surface_get_height(g->surface);
    memset(data, 0, stride * height);
    cairo_surface_mark_dirty(g->surface);
    return tlNull;
}

/** cairo api **/

static tlHandle _save(tlArgs* args) {
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_save(g->cairo);
    return g;
}
static tlHandle _restore(tlArgs* args) {
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_restore(g->cairo);
    return g;
}

static tlHandle _setFillRule(tlArgs* args) {
    tlHandle h = tlArgsGet(args, 0);
    cairo_fill_rule_t rule = CAIRO_FILL_RULE_WINDING;
    if (h == s_even_odd) rule = CAIRO_FILL_RULE_EVEN_ODD;
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_set_fill_rule(g->cairo, rule);
    return g;
}

static tlHandle _setLineCap(tlArgs* args) {
    tlHandle h = tlArgsGet(args, 0);
    cairo_line_cap_t cap = CAIRO_LINE_CAP_BUTT;
    if (h == s_round) {
        cap = CAIRO_LINE_CAP_ROUND;
    } else if (h == s_square) {
        cap = CAIRO_LINE_CAP_SQUARE;
    }
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_set_line_cap(g->cairo, cap);
    return g;
}

static tlHandle _setLineJoin(tlArgs* args) {
    tlHandle h = tlArgsGet(args, 0);
    cairo_line_join_t join = CAIRO_LINE_JOIN_MITER;
    if (h == s_round) {
        join = CAIRO_LINE_JOIN_ROUND;
    } else if (h == s_bevel) {
        join = CAIRO_LINE_JOIN_BEVEL;
    }
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_set_line_join(g->cairo, join);
    return g;
}

// TODO support floats
static tlHandle _setLineWidth(tlArgs* args) {
    int width = tl_int_or(tlArgsGet(args, 0), 2);
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_set_line_width(g->cairo, width);
    return g;
}

static tlHandle _setMiterLimit(tlArgs* args) {
    int limit = tl_int_or(tlArgsGet(args, 0), 10);
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_set_line_width(g->cairo, limit);
    return g;
}

static tlHandle _setOperator(tlArgs* args) {
    tlHandle h = tlArgsGet(args, 0);
    cairo_operator_t op = CAIRO_OPERATOR_OVER;
    if (h == s_clear) {
        op = CAIRO_OPERATOR_CLEAR;
    } else if (h == s_source) {
        op = CAIRO_OPERATOR_SOURCE;
    } else if (h == s_over) {
        op = CAIRO_OPERATOR_OVER;
    } else if (h == s_in) {
        op = CAIRO_OPERATOR_IN;
    } else if (h == s_out) {
        op = CAIRO_OPERATOR_OUT;
    } else if (h == s_atop) {
        op = CAIRO_OPERATOR_ATOP;
    } else if (h == s_dest) {
        op = CAIRO_OPERATOR_DEST;
    } else if (h == s_over) {
        op = CAIRO_OPERATOR_DEST_OVER;
    } else if (h == s_in) {
        op = CAIRO_OPERATOR_DEST_IN;
    } else if (h == s_out) {
        op = CAIRO_OPERATOR_DEST_OUT;
    } else if (h == s_atop) {
        op = CAIRO_OPERATOR_DEST_ATOP;
    } else if (h == s_xor) {
        op = CAIRO_OPERATOR_XOR;
    } else if (h == s_add) {
        op = CAIRO_OPERATOR_ADD;
    } else if (h == s_saturate) {
        op = CAIRO_OPERATOR_SATURATE;
    } else if (h == s_overlay) {
        op = CAIRO_OPERATOR_OVERLAY;
    } else if (h == s_darken) {
        op = CAIRO_OPERATOR_DARKEN;
    } else if (h == s_lighten) {
        op = CAIRO_OPERATOR_LIGHTEN;
    } else if (h == s_color_dodge) {
        op = CAIRO_OPERATOR_COLOR_DODGE;
    } else if (h == s_color_burn) {
        op = CAIRO_OPERATOR_COLOR_BURN;
    } else if (h == s_hard_light) {
        op = CAIRO_OPERATOR_HARD_LIGHT;
    } else if (h == s_soft_light) {
        op = CAIRO_OPERATOR_SOFT_LIGHT;
    } else if (h == s_difference) {
        op = CAIRO_OPERATOR_DIFFERENCE;
    } else if (h == s_exclusion) {
        op = CAIRO_OPERATOR_EXCLUSION;
    } else if (h == s_hsl_hue) {
        op = CAIRO_OPERATOR_HSL_HUE;
    } else if (h == s_hsl_saturation) {
        op = CAIRO_OPERATOR_HSL_SATURATION;
    } else if (h == s_hsl_color) {
        op = CAIRO_OPERATOR_HSL_COLOR;
    } else if (h == s_hsl_luminosity) {
        op = CAIRO_OPERATOR_HSL_LUMINOSITY;
    }
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_set_operator(g->cairo, op);
    return g;
}

static tlHandle _clip(tlArgs* args) {
    bool preserve = tl_bool(tlArgsGet(args, 0));
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    (!preserve)? cairo_clip(g->cairo) : cairo_clip_preserve(g->cairo);
    return g;
}
static tlHandle _resetClip(tlArgs* args) {
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_reset_clip(g->cairo);
    return g;
}

// TODO handle color names in string and symbol form
// TODO also support floats and 3 or 4 length float arrays
static tlHandle _color(tlArgs* args) {
    float r = tl_int_or(tlArgsGet(args, 0), 0)/255.0;
    float g = tl_int_or(tlArgsGet(args, 1), 0)/255.0;
    float b = tl_int_or(tlArgsGet(args, 2), 0)/255.0;
    float a = tl_int_or(tlArgsGet(args, 3), 255)/255.0;
    Graphics* gr = GraphicsAs(tlArgsTarget(args));
    cairo_set_source_rgba(gr->cairo, r, g, b, a);
    return gr;
}

static tlHandle _fill(tlArgs* args) {
    bool preserve = tl_bool(tlArgsGet(args, 0));
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    (!preserve)? cairo_fill(g->cairo) : cairo_fill_preserve(g->cairo);
    return g;
}

static tlHandle _stroke(tlArgs* args) {
    bool preserve = tl_bool(tlArgsGet(args, 0));
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    (!preserve)? cairo_stroke(g->cairo) : cairo_stroke_preserve(g->cairo);
    return g;
}

//** path api **

static tlHandle _newPath(tlArgs* args) {
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_new_path(g->cairo);
    return g;
}
static tlHandle _closePath(tlArgs* args) {
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_close_path(g->cairo);
    return g;
}

static tlHandle _curveTo(tlArgs* args) {
    int x1 = tl_int_or(tlArgsGet(args, 0), 0);
    int y1 = tl_int_or(tlArgsGet(args, 1), 0);
    int x2 = tl_int_or(tlArgsGet(args, 2), 0);
    int y2 = tl_int_or(tlArgsGet(args, 3), 0);
    int x3 = tl_int_or(tlArgsGet(args, 4), 0);
    int y3 = tl_int_or(tlArgsGet(args, 5), 0);
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_curve_to(g->cairo, x1, y1, x2, y2, x3, y3);
    return g;
}
static tlHandle _lineTo(tlArgs* args) {
    int x = tl_int_or(tlArgsGet(args, 0), 0);
    int y = tl_int_or(tlArgsGet(args, 1), 0);
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_line_to(g->cairo, x, y);
    return g;
}
static tlHandle _moveTo(tlArgs* args) {
    int x = tl_int_or(tlArgsGet(args, 0), 0);
    int y = tl_int_or(tlArgsGet(args, 1), 0);
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_move_to(g->cairo, x, y);
    return g;
}
static tlHandle _curveToRel(tlArgs* args) {
    int x1 = tl_int_or(tlArgsGet(args, 0), 0);
    int y1 = tl_int_or(tlArgsGet(args, 1), 0);
    int x2 = tl_int_or(tlArgsGet(args, 2), 0);
    int y2 = tl_int_or(tlArgsGet(args, 3), 0);
    int x3 = tl_int_or(tlArgsGet(args, 4), 0);
    int y3 = tl_int_or(tlArgsGet(args, 5), 0);
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_rel_curve_to(g->cairo, x1, y1, x2, y2, x3, y3);
    return g;
}
static tlHandle _lineToRel(tlArgs* args) {
    int x = tl_int_or(tlArgsGet(args, 0), 0);
    int y = tl_int_or(tlArgsGet(args, 1), 0);
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_rel_line_to(g->cairo, x, y);
    return g;
}
static tlHandle _moveToRel(tlArgs* args) {
    int x = tl_int_or(tlArgsGet(args, 0), 0);
    int y = tl_int_or(tlArgsGet(args, 1), 0);
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_rel_move_to(g->cairo, x, y);
    return g;
}

static tlHandle _arc(tlArgs* args) {
    int x = tl_int_or(tlArgsGet(args, 0), 0);
    int y = tl_int_or(tlArgsGet(args, 1), 0);
    int r = tl_int_or(tlArgsGet(args, 2), 0);
    double a1 = tl_int_or(tlArgsGet(args, 3), 0)/360.0 * TAU;
    double a2 = tl_int_or(tlArgsGet(args, 4), 360)/360.0 * TAU;
    bool flip = tl_bool(tlArgsGet(args, 5));
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    (!flip)? cairo_arc(g->cairo, x, y, r, a1, a2) : cairo_arc_negative(g->cairo, x, y, r, a1, a2);
    return g;
}
static tlHandle _rectangle(tlArgs* args) {
    int x = tl_int_or(tlArgsGet(args, 0), 0);
    int y = tl_int_or(tlArgsGet(args, 1), 0);
    int w = tl_int_or(tlArgsGet(args, 2), 0);
    int h = tl_int_or(tlArgsGet(args, 3), 0);
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_rectangle(g->cairo, x, y, w, h);
    return g;
};

static tlHandle _textPath(tlArgs* args) {
    tlText* text = tlTextCast(tlArgsGet(args, 0));
    const char* utf8 = (text)?tlTextData(text):"";
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_text_path(g->cairo, utf8);
    return g;
}

// ** transforms **
static tlHandle _translate(tlArgs* args) {
    int x = tl_int_or(tlArgsGet(args, 0), 0);
    int y = tl_int_or(tlArgsGet(args, 1), 0);
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_translate(g->cairo, x, y);
    return g;
}
static tlHandle _rotate(tlArgs* args) {
    double r = tl_int_or(tlArgsGet(args, 0), 0)/360.0 * TAU;
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_rotate(g->cairo, r);
    return g;
}
static tlHandle _scale(tlArgs* args) {
    int x = tl_int_or(tlArgsGet(args, 0), 0);
    int y = tl_int_or(tlArgsGet(args, 1), 0);
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_scale(g->cairo, x, y);
    return g;
}

// ** text **
static tlHandle _setFont(tlArgs* args) {
    tlText* name = tlTextCast(tlArgsGet(args, 0));
    int size = tl_int_or(tlArgsGet(args, 1), 16);

    // TODO set these two also from args
    cairo_font_slant_t slant = CAIRO_FONT_SLANT_NORMAL;
    cairo_font_weight_t weight = CAIRO_FONT_WEIGHT_NORMAL;

    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_set_font_size(g->cairo, size);
    cairo_select_font_face(g->cairo, tlTextData(name), slant, weight);
    return g;
}
static tlHandle _fillText(tlArgs* args) {
    tlText* text = tlTextCast(tlArgsGet(args, 0));
    const char* utf8 = (text)?tlTextData(text):"";
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_show_text(g->cairo, utf8);
    return g;
}
static tlHandle _measureText(tlArgs* args) {
    cairo_text_extents_t extents;
    tlText* text = tlTextCast(tlArgsGet(args, 0));
    const char* utf8 = (text)?tlTextData(text):"";
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_text_extents(g->cairo, utf8,  &extents);
    return tlResultFrom(tlINT(extents.width), tlINT(extents.height), null);
}

// ** images **
static tlHandle _image(tlArgs* args) {
    Image* img = ImageCast(tlArgsGet(args, 0));
    if (!img) TL_THROW("require an image");

    int width = imageWidth(img);
    int height = imageHeight(img);

    int dx = tl_int_or(tlArgsGet(args, 1), 0);
    int dy = tl_int_or(tlArgsGet(args, 2), 0);
    int dw = tl_int_or(tlArgsGet(args, 3), width);
    int dh = tl_int_or(tlArgsGet(args, 4), height);

    print("%d %d %d %d", dx, dy, dw, dh);

    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_save(g->cairo);
    cairo_translate(g->cairo, dx, dy);
    cairo_rectangle(g->cairo, 0, 0, dw, dh);
    cairo_clip(g->cairo);

    if (dw != width || dh != height) {
        cairo_scale(g->cairo, dw/(double)width, dh/(double)height);
    }
    cairo_set_source_surface(g->cairo, imageSurface(img), 0, 0);
    cairo_paint(g->cairo);
    cairo_restore(g->cairo);
    CAIRO_STATUS(g->cairo);
    return g;
}

void graphics_init(tlVm* vm) {

    s_winding = tlSYM("winding");
    s_even_odd = tlSYM("even-odd");

    s_butt = tlSYM("butt");
    s_round = tlSYM("round");
    s_square = tlSYM("square");
    s_bevel = tlSYM("bevel");
    s_clear = tlSYM("clear");

    s_source = tlSYM("source");
    s_over = tlSYM("over");
    s_in = tlSYM("in");
    s_out = tlSYM("out");
    s_atop = tlSYM("atop");
    s_dest = tlSYM("dest");
    s_over = tlSYM("over");
    s_xor = tlSYM("xor");
    s_add = tlSYM("add");
    s_saturate = tlSYM("saturate");
    s_overlay = tlSYM("overlay");
    s_darken = tlSYM("darken");
    s_lighten = tlSYM("lighten");
    s_color_dodge = tlSYM("color-dodge");
    s_color_burn = tlSYM("color-burn");
    s_hard_light = tlSYM("hard-light");
    s_soft_light = tlSYM("soft-light");
    s_difference = tlSYM("difference");
    s_exclusion = tlSYM("exclusion");
    s_hsl_hue = tlSYM("hsl_hue");
    s_hsl_saturation = tlSYM("hsl_saturation");
    s_hsl_color = tlSYM("hsl_color");
    s_hsl_luminosity = tlSYM("hsl_luminosity");

    _GraphicsKind.klass = tlClassMapFrom(
        "resize", _resize,
        "width", _width,
        "height", _height,

        "clear", _clear,
        "save", _save,
        "restore", _restore,

        "setOperator", _setOperator,

        "setFillRule", _setFillRule,
        "setLineCap", _setLineCap,
        "setLineJoin", _setLineJoin,
        "setLineWidth", _setLineWidth,
        "setMiterLimit", _setMiterLimit,

        "clip", _clip,
        "resetClip", _resetClip,

        "color", _color,
        "fill", _fill,
        "stroke", _stroke,

        "newPath", _newPath,
        "closePath", _closePath,
        "moveTo", _moveTo,
        "lineTo", _lineTo,
        "curveTo", _curveTo,
        "moveToRel", _moveToRel,
        "lineToRel", _lineToRel,
        "curveToRel", _curveToRel,
        "arc", _arc,
        "rectangle", _rectangle,
        "text", _textPath,

        "translate", _translate,
        "scale", _scale,
        "rotate", _rotate,

        "setFont", _setFont,
        "fillText", _fillText,
        "measureText", _measureText,

        "image", _image,

        null
    );

    tlMap* GraphicsStatic = tlClassMapFrom(
        //"new", _Graphics_new,

        "winding", s_winding,
        "even_odd", s_even_odd,

        "butt", s_butt,
        "round", s_round,
        "square", s_square,
        "bevel", s_bevel,
        "clear", s_clear,

        "source", s_source,
        "over", s_over,
        "in", s_in,
        "out", s_out,
        "atop", s_atop,
        "dest", s_dest,
        "over", s_over,
        "xor", s_xor,
        "add", s_add,
        "saturate", s_saturate,
        "overlay", s_overlay,
        "darken", s_darken,
        "lighten", s_lighten,
        "color_dodge", s_color_dodge,
        "color_burn", s_color_burn,
        "hard_light", s_hard_light,
        "soft_light", s_soft_light,
        "difference", s_difference,
        "exclusion", s_exclusion,
        "hsl_hue", s_hsl_hue,
        "hsl_saturation", s_hsl_saturation,
        "hsl_color", s_hsl_color,
        "hsl_luminosity", s_hsl_luminosity,

        null
    );
    tlVmGlobalSet(vm, tlSYM("Graphics"), GraphicsStatic);
}

