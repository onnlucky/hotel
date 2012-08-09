#include "vm/tl.h"
#include <cairo/cairo.h>

TL_REF_TYPE(Graphics);

void graphics_init(tlVm* vm);

Graphics* GraphicsNew(cairo_t* cairo);

// TODO old, create an image like thing for this
void graphicsData(Graphics* g, uint8_t** bytes, int* width, int* height, int* stride);

