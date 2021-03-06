#ifndef _image_h_
#define _image_h_

#include "tl.h"
#include "graphics.h"

TL_REF_TYPE(Image);
void image_init(tlVm* vm);
Image* ImageNew(int width, int height, bool alpha);
Graphics* imageGetGraphics(Image* img);
void GraphicsSetImage(Graphics* g, Image* img);

int imageWidth(Image* img);
int imageHeight(Image* img);
cairo_surface_t* imageSurface(Image* img);

#endif // _image_h_

