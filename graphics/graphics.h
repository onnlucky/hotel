#include "vm/tl.h"

TL_REF_TYPE(Graphics);

void graphics_init(tlVm* vm);

Graphics* GraphicsNew(int width, int height);
void graphicsData(Graphics* g, uint8_t** bytes, int* width, int* height, int* stride);

