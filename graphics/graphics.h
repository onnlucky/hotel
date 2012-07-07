#include <cairo/cairo.h>
#include "vm/tl.h"

TL_REF_TYPE(Graphics);

void graphics_init(tlVm* vm);

Graphics* graphicsSizeTo(Graphics* g, int width, int height);
void graphicsDrawOn(Graphics* g, cairo_t* cr);
void graphicsDelete(Graphics* g);

