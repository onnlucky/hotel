#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>

#include "image.h"

struct Image {
    tlLock lock;
    Graphics* graphics;
    cairo_surface_t* surface;
    cairo_t* cairo;
};
tlKind _ImageKind = { .name = "Image", .locked = true, };
tlKind* ImageKind = &_ImageKind;


#ifdef HAVE_BOEHMGC
#undef malloc
#undef free
#endif
static cairo_user_data_key_t jpeg_free_key;
void jpeg_free(void* data) { free(data); }

// ** jpg **
cairo_surface_t* readjpg(tlBuffer* buf) {
    struct jpeg_decompress_struct info;
    struct jpeg_error_mgr jerr;

    info.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&info);
    jpeg_mem_src(&info, (unsigned char*)tlBufferData(buf), tlBufferSize(buf));
    info.buffered_image = true;

    int r = jpeg_read_header(&info, true);
    if (r != JPEG_HEADER_OK) {
        warning("jpeg parse error: %d", r);
        return null;
    }
    const int width = info.image_width;
    const int height = info.image_height;
    if (width <= 0 || height <= 0) {
        warning("jpeg without width/height");
        return null;
    }
    if (info.num_components != 3) {
        warning("jpeg is not rgb");
        return null;
    }
    info.out_color_space = JCS_RGB;

    // a output bitmap, 32 bits per pixel
    cairo_format_t format = CAIRO_FORMAT_RGB24;
    const int stride = cairo_format_stride_for_width(format, width);
    uint8_t* img = (uint8_t*)malloc(stride * height);

    // a single row, 24 bits per pixel ... sigh we have to convert
    JSAMPROW rows[1];
    uint8_t* row = (uint8_t*)malloc(width * 3);
    rows[0] = row;

    jpeg_start_decompress(&info);
    assert(info.out_color_space == JCS_RGB);
    assert(info.out_color_components == 3);
    for (int i = 0; i < height; i++) {
        jpeg_read_scanlines(&info, rows, 1);
        // lets just assume compiler writers are smart
        for (int p = 0; p < width; p++) {
            img[stride * i + p * 4 + 0] = row[p * 3 + 2];
            img[stride * i + p * 4 + 1] = row[p * 3 + 1];
            img[stride * i + p * 4 + 2] = row[p * 3 + 0];
            img[stride * i + p * 4 + 3] = 0xFF;
        }
    }
    free(row);
    jpeg_finish_decompress(&info);
    jpeg_destroy_decompress(&info);

    cairo_surface_t* surface = cairo_image_surface_create_for_data(img, format, width, height, stride);
    cairo_surface_set_user_data(surface, &jpeg_free_key, img, jpeg_free);
    return surface;
}
#ifdef HAVE_BOEHMGC
#define free GC_free
#define malloc GC_malloc
#endif

// ** png **
cairo_status_t readbuffer(void* _buf, unsigned char* data, unsigned int length) {
    int len = tlBufferRead(tlBufferAs(_buf), (char*)data, length);
    return len? CAIRO_STATUS_SUCCESS : CAIRO_STATUS_READ_ERROR;
}
cairo_surface_t* readpng(tlBuffer* buf) {
    return cairo_image_surface_create_from_png_stream(readbuffer, buf);
}

cairo_status_t writebuffer(void* _buf, const unsigned char *data, unsigned int length) {
    int len = tlBufferWrite(tlBufferAs(_buf), (const char*)data, length);
    return len == length? CAIRO_STATUS_SUCCESS : CAIRO_STATUS_READ_ERROR;
}

cairo_status_t writepngbuffer(cairo_surface_t* surface, tlBuffer* buf) {
    return cairo_surface_write_to_png_stream(surface, writebuffer, buf);
}
// **


void clear_img(void* _img, void* data) {
    Image* img = ImageAs(_img);
    if (img->surface) cairo_surface_destroy(img->surface);
    if (img->cairo) cairo_destroy(img->cairo);
}

Image* ImageNew(int width, int height) {
    Image* img = tlAlloc(ImageKind, sizeof(Image));
    img->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
#ifdef HAVE_BOEHMGC
    GC_REGISTER_FINALIZER_NO_ORDER(img, clear_img, null, null, null);
#endif
    return img;
}
Image* ImageFromBuffer(tlBuffer* buf) {
    Image* img = tlAlloc(ImageKind, sizeof(Image));
    if (tlBufferFind(buf, "PNG", 3) == 1) {
        img->surface = readpng(buf);
    } else {
        img->surface = readjpg(buf);
    }
#ifdef HAVE_BOEHMGC
    GC_REGISTER_FINALIZER_NO_ORDER(img, clear_img, null, null, null);
#endif
    return img;
}

Graphics* imageGetGraphics(Image* img) {
    if (!img->surface) return tlNull;
    if (!img->graphics) {
        img->cairo = cairo_create(img->surface);
        img->graphics = GraphicsNew(img->cairo);
        GraphicsSetImage(img->graphics, img); // for gc, doubly bind the two together
    }
    return img->graphics;
}

int imageWidth(Image* img) {
    if (!img->surface) return 0;
    return cairo_image_surface_get_width(img->surface);
}
int imageHeight(Image* img) {
    if (!img->surface) return 0;
    return cairo_image_surface_get_height(img->surface);
}
cairo_surface_t* imageSurface(Image* img) { return img->surface; }

static tlHandle _Image_new(tlTask* task, tlArgs* args) {
    tlBuffer* buf = tlBufferCast(tlArgsGet(args, 0));
    if (buf) return ImageFromBuffer(buf);

    int width = tl_int_or(tlArgsGet(args, 0), 0);
    int height = tl_int_or(tlArgsGet(args, 1), 0);
    if (width > 0 && height > 0) return ImageNew(width, height);

    tlString* path = tlStringCast(tlArgsGet(args, 0));
    if (path) {
        tlBuffer* buffer = tlBufferFromFile(tlStringData(path));
        if (buffer) return ImageFromBuffer(buffer);
        TL_THROW("cannot read file");
    }
    TL_THROW("Image.new requires a Buffer or a width and height");
}

static tlHandle _image_width(tlTask* task, tlArgs* args) {
    Image* img = ImageAs(tlArgsTarget(args));
    return tlINT(imageWidth(img));
}
static tlHandle _image_height(tlTask* task, tlArgs* args) {
    Image* img = ImageAs(tlArgsTarget(args));
    return tlINT(imageHeight(img));
}
static tlHandle _image_graphics(tlTask* task, tlArgs* args) {
    Image* img = ImageAs(tlArgsTarget(args));
    return imageGetGraphics(img);
}
static tlHandle _image_writePNG(tlTask* task, tlArgs* args) {
    Image* img = ImageAs(tlArgsTarget(args));
    tlBuffer* buf = tlBufferCast(tlArgsGet(args, 0));
    if (buf) {
        cairo_status_t s = writepngbuffer(img->surface, buf);
        if (s) TL_THROW("oeps: %s", cairo_status_to_string(s));
        return tlNull;
    }

    tlString* path = tlStringCast(tlArgsGet(args, 0));
    if (!path) TL_THROW("require a filename or buffer");
    cairo_status_t s = cairo_surface_write_to_png(img->surface, tlStringData(path));
    if (s) TL_THROW("oeps: %s", cairo_status_to_string(s));
    return tlNull;
}

static tlObject* imageStatic;
tlObject* image_init() {
    if (imageStatic) return imageStatic;

    _ImageKind.klass = tlClassObjectFrom(
        "width", _image_width,
        "height", _image_height,
        "graphics", _image_graphics,
        "writePNG", _image_writePNG,
        null
    );
    imageStatic = tlClassObjectFrom(
        "new", _Image_new,
        null
    );

    return imageStatic;
}

tlHandle tl_load() {
    return tlObjectFrom("Image", image_init(), "Graphics", graphics_init());
}
