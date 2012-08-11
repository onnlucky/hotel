#include "image.h"

struct Image {
    tlLock lock;
    Graphics* graphics;
    cairo_surface_t* surface;
};
tlKind _ImageKind = { .name = "Image", .locked = true, };
tlKind* ImageKind = &_ImageKind;

cairo_status_t readbuffer(void* _buf, unsigned char* data, unsigned int length) {
    int len = tlBufferRead(tlBufferAs(_buf), (char*)data, length);
    return len?CAIRO_STATUS_SUCCESS:CAIRO_STATUS_READ_ERROR;
}

Image* ImageNew(int width, int height) {
    Image* img = tlAlloc(ImageKind, sizeof(Image));
    img->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    return img;
}
Image* ImageFromBuffer(tlBuffer* buf) {
    Image* img = tlAlloc(ImageKind, sizeof(Image));
    img->surface = cairo_image_surface_create_from_png_stream(readbuffer, buf);
    return img;
}

Graphics* imageGetGraphics(Image* img) {
    if (!img->graphics) img->graphics = GraphicsNew(cairo_create(img->surface));
    return img->graphics;
}

int imageWidth(Image* img) { return cairo_image_surface_get_width(img->surface); }
int imageHeight(Image* img) { return cairo_image_surface_get_height(img->surface); }
cairo_surface_t* imageSurface(Image* img) { return img->surface; }

static tlHandle _Image_new(tlArgs* args) {
    tlBuffer* buf = tlBufferCast(tlArgsGet(args, 0));
    if (buf) return ImageFromBuffer(buf);

    int width = tl_int_or(tlArgsGet(args, 0), 0);
    int height = tl_int_or(tlArgsGet(args, 1), 0);
    if (width > 0 && height > 0) return ImageNew(width, height);

    tlText* path = tlTextCast(tlArgsGet(args, 0));
    if (path) {
        tlBuffer* buffer = tlBufferFromFile(tlTextData(path));
        if (buffer) return ImageFromBuffer(buffer);
        TL_THROW("cannot read file");
    }
    TL_THROW("Image.new requires a Buffer or a width and height");
}

static tlHandle _image_width(tlArgs* args) {
    Image* img = ImageAs(tlArgsTarget(args));
    return tlINT(cairo_image_surface_get_width(img->surface));
}
static tlHandle _image_height(tlArgs* args) {
    Image* img = ImageAs(tlArgsTarget(args));
    return tlINT(cairo_image_surface_get_height(img->surface));
}
static tlHandle _image_graphics(tlArgs* args) {
    Image* img = ImageAs(tlArgsTarget(args));
    return imageGetGraphics(img);
}
static tlHandle _image_writePNG(tlArgs* args) {
    Image* img = ImageAs(tlArgsTarget(args));
    tlText* path = tlTextCast(tlArgsGet(args, 0));
    if (!path) TL_THROW("require a filename");
    cairo_status_t s = cairo_surface_write_to_png(img->surface, tlTextData(path));
    if (s) TL_THROW("oeps: %s", cairo_status_to_string(s));
    return tlNull;
}

void image_init(tlVm* vm) {
    _ImageKind.klass = tlClassMapFrom(
        "width", _image_width,
        "height", _image_height,
        "graphics", _image_graphics,
        "writePNG", _image_writePNG,
        null
    );
    tlMap* ImageStatic = tlClassMapFrom(
        "new", _Image_new,
        null
    );
    tlVmGlobalSet(vm, tlSYM("Image"), ImageStatic);
}

