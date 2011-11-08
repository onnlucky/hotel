#include <cairo/cairo.h>
#include "../tl.h"

TL_REF_TYPE(Graphics);
void graphics_init(tlVm* vm);
Graphics* graphicsSizeTo(tlTask* task, Graphics* g, int width, int height);
void graphicsDrawOn(Graphics* g, cairo_t* cr);

