#ifndef _graphics_h_
#define _graphics_h_

#include <cairo/cairo.h>
#include <tl.h>

TL_REF_TYPE(Graphics);

Graphics* GraphicsNew(cairo_t* cairo);

tlObject* graphics_init();

#endif // _graphics_h_
