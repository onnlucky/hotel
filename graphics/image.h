#ifndef _image_h_
#define _image_h_

#include "vm/tl.h"
#include "graphics.h"

TL_REF_TYPE(Image);
void image_init(tlVm* vm);
Graphics* imageGetGraphics(Image* img);
void GraphicsSetImage(Graphics* g, Image* img);

int imageWidth(Image* img);
int imageHeight(Image* img);
cairo_surface_t* imageSurface(Image* img);

#endif // _image_h_

