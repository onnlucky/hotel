#ifndef _graphics_h_
#define _graphics_h_

#include <cairo/cairo.h>
#include <tl.h>

TL_REF_TYPE(Graphics);

Graphics* GraphicsNew(cairo_t* cairo);

// TODO old, create an image like thing for this
void graphicsData(Graphics* g, uint8_t** bytes, int* width, int* height, int* stride);

tlObject* graphics_init();

#endif // _graphics_h_
