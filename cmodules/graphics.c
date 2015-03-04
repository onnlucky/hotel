#include "debug.h"
#include "graphics.h"
#include "image.h"

#include "trace-off.h"

#define max(l, r) (((l) >= (r))?(l):(r))

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
    Image* img;
    cairo_t* cairo;
};

static tlKind _GraphicsKind = {
    .name = "Graphics",
    .locked = true,
};
tlKind* GraphicsKind = &_GraphicsKind;

Graphics* GraphicsNew(cairo_t* cairo) {
#if CAIRO_VERSION_MAJOR >= 1 && CAIRO_VERSION_MINOR >= 12
    cairo_set_antialias(cairo, CAIRO_ANTIALIAS_BEST);
#else
    cairo_set_antialias(cairo, CAIRO_ANTIALIAS_DEFAULT);
#endif
    trace("new graphics; width: %d, height: %d", width, height);
    Graphics* g = tlAlloc(GraphicsKind, sizeof(Graphics));
    g->cairo = cairo;
    return g;
}

void GraphicsSetImage(Graphics* g, Image* img) {
    g->img = img;
}

/** cairo api **/

static tlHandle _save(tlTask* task, tlArgs* args) {
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_save(g->cairo);
    return g;
}
static tlHandle _restore(tlTask* task, tlArgs* args) {
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_restore(g->cairo);
    return g;
}

static tlHandle _setFillRule(tlTask* task, tlArgs* args) {
    tlHandle h = tlArgsGet(args, 0);
    cairo_fill_rule_t rule = CAIRO_FILL_RULE_WINDING;
    if (h == s_even_odd) rule = CAIRO_FILL_RULE_EVEN_ODD;
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_set_fill_rule(g->cairo, rule);
    return g;
}

static tlHandle _setLineCap(tlTask* task, tlArgs* args) {
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

static tlHandle _setLineJoin(tlTask* task, tlArgs* args) {
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

static tlHandle _setLineWidth(tlTask* task, tlArgs* args) {
    double width = tl_double_or(tlArgsGet(args, 0), 2.0);
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_set_line_width(g->cairo, width);
    return g;
}

static tlHandle _setMiterLimit(tlTask* task, tlArgs* args) {
    double limit = tl_double_or(tlArgsGet(args, 0), 10.0);
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_set_line_width(g->cairo, limit);
    return g;
}

static tlHandle _setOperator(tlTask* task, tlArgs* args) {
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
#if CAIRO_VERSION_MAJOR >= 1 && CAIRO_VERSION_MINOR >= 10
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
#endif
    }
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_set_operator(g->cairo, op);
    return g;
}

static tlHandle _clip(tlTask* task, tlArgs* args) {
    bool preserve = tl_bool(tlArgsGet(args, 0));
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    (!preserve)? cairo_clip(g->cairo) : cairo_clip_preserve(g->cairo);
    return g;
}
static tlHandle _resetClip(tlTask* task, tlArgs* args) {
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_reset_clip(g->cairo);
    return g;
}

// TODO handle color names in string and symbol form
// TODO also support floats and 3 or 4 length float arrays
static tlHandle _color(tlTask* task, tlArgs* args) {
    double r = tl_double_or(tlArgsGet(args, 0), 0);
    double g = tl_double_or(tlArgsGet(args, 1), 0);
    double b = tl_double_or(tlArgsGet(args, 2), 0);
    double a = tl_double_or(tlArgsGet(args, 3), 1.0);
    if (tlListIs(tlArgsGet(args, 0))) {
        tlList* list = tlListAs(tlArgsGet(args, 0));
        r = tl_double_or(tlListGet(list, 0), 0);
        g = tl_double_or(tlListGet(list, 1), 0);
        b = tl_double_or(tlListGet(list, 2), 0);
        a = tl_double_or(tlListGet(list, 3), 1.0);
    }
    Graphics* gr = GraphicsAs(tlArgsTarget(args));
    cairo_set_source_rgba(gr->cairo, r, g, b, a);
    return gr;
}

static tlHandle _fill(tlTask* task, tlArgs* args) {
    bool preserve = tl_bool(tlArgsGet(args, 0));
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    (!preserve)? cairo_fill(g->cairo) : cairo_fill_preserve(g->cairo);
    return g;
}

static tlHandle _stroke(tlTask* task, tlArgs* args) {
    bool preserve = tl_bool(tlArgsGet(args, 0));
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    (!preserve)? cairo_stroke(g->cairo) : cairo_stroke_preserve(g->cairo);
    return g;
}

//** path api **

static tlHandle _newPath(tlTask* task, tlArgs* args) {
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_new_path(g->cairo);
    return g;
}
static tlHandle _closePath(tlTask* task, tlArgs* args) {
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_close_path(g->cairo);
    return g;
}

static tlHandle _curveTo(tlTask* task, tlArgs* args) {
    double x1 = tl_double_or(tlArgsGet(args, 0), 0);
    double y1 = tl_double_or(tlArgsGet(args, 1), 0);
    double x2 = tl_double_or(tlArgsGet(args, 2), 0);
    double y2 = tl_double_or(tlArgsGet(args, 3), 0);
    double x3 = tl_double_or(tlArgsGet(args, 4), 0);
    double y3 = tl_double_or(tlArgsGet(args, 5), 0);
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_curve_to(g->cairo, x1, y1, x2, y2, x3, y3);
    return g;
}
static tlHandle _lineTo(tlTask* task, tlArgs* args) {
    double x = tl_double_or(tlArgsGet(args, 0), 0);
    double y = tl_double_or(tlArgsGet(args, 1), 0);
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_line_to(g->cairo, x, y);
    return g;
}
static tlHandle _moveTo(tlTask* task, tlArgs* args) {
    double x = tl_double_or(tlArgsGet(args, 0), 0);
    double y = tl_double_or(tlArgsGet(args, 1), 0);
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_move_to(g->cairo, x, y);
    return g;
}
static tlHandle _curveToRel(tlTask* task, tlArgs* args) {
    double x1 = tl_double_or(tlArgsGet(args, 0), 0);
    double y1 = tl_double_or(tlArgsGet(args, 1), 0);
    double x2 = tl_double_or(tlArgsGet(args, 2), 0);
    double y2 = tl_double_or(tlArgsGet(args, 3), 0);
    double x3 = tl_double_or(tlArgsGet(args, 4), 0);
    double y3 = tl_double_or(tlArgsGet(args, 5), 0);
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_rel_curve_to(g->cairo, x1, y1, x2, y2, x3, y3);
    return g;
}
static tlHandle _lineToRel(tlTask* task, tlArgs* args) {
    double x = tl_double_or(tlArgsGet(args, 0), 0);
    double y = tl_double_or(tlArgsGet(args, 1), 0);
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_rel_line_to(g->cairo, x, y);
    return g;
}
static tlHandle _moveToRel(tlTask* task, tlArgs* args) {
    double x = tl_double_or(tlArgsGet(args, 0), 0);
    double y = tl_double_or(tlArgsGet(args, 1), 0);
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_rel_move_to(g->cairo, x, y);
    return g;
}

static tlHandle _arc(tlTask* task, tlArgs* args) {
    double x = tl_double_or(tlArgsGet(args, 0), 0);
    double y = tl_double_or(tlArgsGet(args, 1), 0);
    double r = tl_double_or(tlArgsGet(args, 2), 0);
    double a1 = tl_double_or(tlArgsGet(args, 3), 0);
    double a2 = tl_double_or(tlArgsGet(args, 4), TAU);
    bool flip = tl_bool(tlArgsGet(args, 5));
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    (!flip)? cairo_arc(g->cairo, x, y, r, a1, a2) : cairo_arc_negative(g->cairo, x, y, r, a1, a2);
    return g;
}
static tlHandle _rectangle(tlTask* task, tlArgs* args) {
    double x = tl_double_or(tlArgsGet(args, 0), 0);
    double y = tl_double_or(tlArgsGet(args, 1), 0);
    double w = tl_double_or(tlArgsGet(args, 2), 0);
    double h = tl_double_or(tlArgsGet(args, 3), 0);
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_rectangle(g->cairo, x, y, w, h);
    return g;
};

void write_utf8(int c, char buf[], int* len);
static tlHandle _textPath(tlTask* task, tlArgs* args) {
    tlHandle in = tlArgsGet(args, 0);
    const char* utf8;
    if (tlStringIs(in)) {
        utf8 = tlStringData(tlStringAs(in));
    } else if (tlNumberIs(in) || tlCharIs(in)) {
        int len;
        char buf[8];
        write_utf8(tl_int(in), buf, &len);
        buf[len] = 0;
        utf8 = buf;
    } else {
        TL_THROW("fillText requires a String or Char");
    }
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_text_path(g->cairo, utf8);
    return g;
}

// ** transforms **
static tlHandle _translate(tlTask* task, tlArgs* args) {
    double x = tl_double_or(tlArgsGet(args, 0), 0);
    double y = tl_double_or(tlArgsGet(args, 1), 0);
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_translate(g->cairo, x, y);
    return g;
}
static tlHandle _rotate(tlTask* task, tlArgs* args) {
    double r = tl_double_or(tlArgsGet(args, 0), 0);
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_rotate(g->cairo, r);
    return g;
}
static tlHandle _scale(tlTask* task, tlArgs* args) {
    double x = tl_double_or(tlArgsGet(args, 0), 0);
    double y = tl_double_or(tlArgsGet(args, 1), 0);
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_scale(g->cairo, x, y);
    return g;
}

// ** text **
static tlHandle _setFont(tlTask* task, tlArgs* args) {
    tlString* name = tlStringCast(tlArgsGet(args, 0));
    int size = tl_double_or(tlArgsGet(args, 1), 16);
    bool italic = tl_bool(tlArgsGet(args, 2));
    cairo_font_slant_t slant = italic? CAIRO_FONT_SLANT_ITALIC : CAIRO_FONT_SLANT_NORMAL;
    bool bold = tl_bool(tlArgsGet(args, 3));
    cairo_font_weight_t weight = bold? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL;

    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_set_font_size(g->cairo, size);
    cairo_select_font_face(g->cairo, tlStringData(name), slant, weight);
    return g;
}

static tlHandle _fontSize(tlTask* task, tlArgs* args) {
    cairo_font_extents_t extents;
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_font_extents(g->cairo, &extents);
    return tlResultFrom(tlFLOAT(extents.height), tlFLOAT(extents.ascent), tlFLOAT(extents.descent), null);
}

static tlHandle _fillText(tlTask* task, tlArgs* args) {
    tlHandle in = tlArgsGet(args, 0);
    const char* utf8;
    if (tlStringIs(in)) {
        utf8 = tlStringData(tlStringAs(in));
    } else if (tlNumberIs(in) || tlCharIs(in)) {
        int len;
        char buf[8];
        write_utf8(tl_int(in), buf, &len);
        buf[len] = 0;
        utf8 = buf;
    } else {
        TL_THROW("fillText requires a String or Char");
    }
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_show_text(g->cairo, utf8);
    return g;
}

static tlHandle _measureText(tlTask* task, tlArgs* args) {
    cairo_text_extents_t extents;
    tlHandle in = tlArgsGet(args, 0);
    const char* utf8;
    if (tlStringIs(in)) {
        utf8 = tlStringData(tlStringAs(in));
    } else if (tlNumberIs(in) || tlCharIs(in)) {
        int len;
        char buf[8];
        write_utf8(tl_int(in), buf, &len);
        buf[len] = 0;
        utf8 = buf;
    } else {
        TL_THROW("measureText requires a String or Char");
    }
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    cairo_text_extents(g->cairo, utf8,  &extents);
    return tlResultFrom(tlFLOAT(max(extents.width,extents.x_advance)), tlFLOAT(max(extents.height,extents.y_advance)), null);
}

static tlHandle _flush(tlTask* task, tlArgs* args) {
    TL_TARGET(Graphics, g);
    cairo_surface_flush(cairo_get_target(g->cairo));
    return tlNull;
}

// ** images **
static tlHandle _image(tlTask* task, tlArgs* args) {
    Image* img = ImageCast(tlArgsGet(args, 0));
    if (!img) {
        Graphics* g = GraphicsCast(tlArgsGet(args, 0));
        if (!g) TL_THROW("require an image or graphic");
        if (!g->img) TL_THROW("require a graphics with image surface");
        cairo_surface_flush(cairo_get_target(g->cairo));
        img = g->img;
    }

    Graphics* g = GraphicsAs(tlArgsTarget(args));
    if (!imageSurface(img)) return g;

    double width = imageWidth(img);
    double height = imageHeight(img);
    double dx = tl_double_or(tlArgsGet(args, 1), 0);
    double dy = tl_double_or(tlArgsGet(args, 2), 0);
    double dw = tl_double_or(tlArgsGet(args, 3), width);
    double dh = tl_double_or(tlArgsGet(args, 4), height);

    cairo_save(g->cairo);
    cairo_translate(g->cairo, dx, dy);
    cairo_rectangle(g->cairo, 0.5, 0.5, dw, dh);
    cairo_clip(g->cairo);

    if (dw != width || dh != height) {
        cairo_scale(g->cairo, dw/width, dh/height);
    }
    cairo_set_source_surface(g->cairo, imageSurface(img), 0, 0);
    //cairo_set_source_rgba(g->cairo, 0, 0, 0, 0.2);
    cairo_paint(g->cairo);
    cairo_restore(g->cairo);
    CAIRO_STATUS(g->cairo);
    return g;
}

static tlHandle _Graphics_new(tlTask* task, tlArgs* args) {
    static tlSym s_alpha; if (!s_alpha) s_alpha = tlSYM("alpha");
    int width = tl_int_or(tlArgsGet(args, 0), 250);
    int height = tl_int_or(tlArgsGet(args, 1), width);
    bool alpha = tl_bool_or(tlArgsGetNamed(args, s_alpha), true);

    Image* image = ImageNew(width, height, alpha);
    return imageGetGraphics(image);
}

static tlHandle _graphics_width(tlTask* task, tlArgs* args) {
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    if (!g->img) return tlNull;
    return tlINT(imageWidth(g->img));
}

static tlHandle _graphics_height(tlTask* task, tlArgs* args) {
    Graphics* g = GraphicsAs(tlArgsTarget(args));
    if (!g->img) return tlNull;
    return tlINT(imageHeight(g->img));
}

static tlObject* graphicsStatic;
tlObject* graphics_init() {
    if (graphicsStatic) return graphicsStatic;

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

    _GraphicsKind.klass = tlClassObjectFrom(
        "width", _graphics_width,
        "height", _graphics_height,

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
        "fontSize", _fontSize,
        "fillText", _fillText,
        "measureText", _measureText,

        "flush", _flush,
        "image", _image,

        null
    );

    graphicsStatic = tlClassObjectFrom(
        "new", _Graphics_new,
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

    return graphicsStatic;
}

