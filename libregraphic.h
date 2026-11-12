/*
 * libregraphic - a tiny library that gives you a window and a 2D array of
 * pixels. Write pixels, call lg_present(), and they appear on screen.
 * Optionally run a GLSL function over every pixel on the GPU.
 *
 * Pixel layout: one lg_color (uint32_t) per pixel, row-major, origin at the
 * TOP-LEFT. Bytes in memory are R,G,B,A (assumes a little-endian CPU, which
 * is true for x86 and ARM in practice). Use LG_RGB / LG_RGBA to build colors.
 *
 * Minimal program:
 *
 *     lg_window *w = lg_open("hello", 640, 480);
 *     while (!lg_should_close(w)) {
 *         lg_clear(w, LG_RGB(20, 20, 30));
 *         lg_circle(w, 320, 240, 80, LG_RGB(255, 120, 0));
 *         LG_AT(w, 10, 10) = LG_RGB(255, 255, 255);   // direct pixel access
 *         lg_present(w);                              // show it + poll input
 *     }
 *     lg_close(w);
 *
 * Threading: call everything from the thread that called lg_open().
 */
#ifndef LIBREGRAPHIC_H
#define LIBREGRAPHIC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lg_window lg_window;
typedef struct lg_kernel lg_kernel;
typedef uint32_t lg_color;

#define LG_RGBA(r, g, b, a)                                              \
    ((lg_color)(((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) |          \
                ((uint32_t)(g) << 8) | (uint32_t)(r)))
#define LG_RGB(r, g, b) LG_RGBA(r, g, b, 255)

/* Direct 2D access to the pixel buffer: LG_AT(w, x, y) = color; (no bounds check) */
#define LG_AT(win, x, y) \
    (lg_pixels(win)[(size_t)(y) * (size_t)lg_width(win) + (size_t)(x)])

/* ---------------------------------------------------------------- window */

/* Opens a window with a width x height pixel buffer (cleared to black).
 * Returns NULL on failure. The window can be resized; the buffer is scaled
 * to fit and keeps its resolution. */
lg_window *lg_open(const char *title, int width, int height);
void       lg_close(lg_window *w);

int        lg_should_close(lg_window *w);

/* Uploads the pixel buffer to the screen, swaps buffers (vsync) and
 * processes window/input events. Call once per frame. */
void       lg_present(lg_window *w);

/* ---------------------------------------------------------------- pixels */

int        lg_width(lg_window *w);
int        lg_height(lg_window *w);
lg_color  *lg_pixels(lg_window *w);   /* width*height colors, row-major */

/* Bounds-checked helpers (out-of-range coordinates are clipped/ignored). */
void       lg_clear(lg_window *w, lg_color c);
void       lg_set(lg_window *w, int x, int y, lg_color c);
lg_color   lg_get(lg_window *w, int x, int y);          /* 0 if out of range */
void       lg_rect(lg_window *w, int x, int y, int width, int height, lg_color c);
void       lg_line(lg_window *w, int x0, int y0, int x1, int y1, lg_color c);
void       lg_circle(lg_window *w, int cx, int cy, int radius, lg_color c); /* filled */

/* ----------------------------------------------------------------- input */

double     lg_time(void);                                /* seconds since first window */
void       lg_mouse(lg_window *w, int *x, int *y);       /* in buffer coordinates */
int        lg_mouse_down(lg_window *w, int button);      /* 0 left, 1 right, 2 middle */
int        lg_key_down(lg_window *w, int key);           /* LG_KEY_* or 'A'..'Z', '0'..'9' */

#define LG_KEY_SPACE  32
#define LG_KEY_ESCAPE 256
#define LG_KEY_ENTER  257
#define LG_KEY_RIGHT  262
#define LG_KEY_LEFT   263
#define LG_KEY_DOWN   264
#define LG_KEY_UP     265

/* ------------------------------------------------------------- GPU kernels
 *
 * A kernel is a GLSL function run once per pixel on the GPU:
 *
 *     vec4 lg_kernel(vec2 pos) { ... return color; }
 *
 * `pos` is the pixel coordinate (x right, y down, origin top-left, matching
 * the CPU buffer). Available inside your code (GLSL 3.30):
 *
 *     vec4  lg_pixel(vec2 p)   current color at pixel p (clamped to edges)
 *     vec2  lg_size            buffer size in pixels
 *     float lg_time            seconds since first window
 *     vec2  lg_mouse           mouse position in buffer coordinates
 *     vec4  lg_param           the 4 floats you pass to lg_gpu_run
 *
 * Do not put a #version line in your source. Colors are 0..1 per channel.
 *
 * lg_gpu_run reads the current pixel buffer, runs the kernel on every pixel,
 * and writes the result back into the same buffer, so CPU and GPU drawing
 * can be freely mixed within a frame.
 */

/* Compiles a kernel. On failure returns NULL and, if err != NULL, writes the
 * compiler log (NUL-terminated) into err[0..errlen). */
lg_kernel *lg_kernel_create(lg_window *w, const char *glsl_source,
                            char *err, size_t errlen);
void       lg_kernel_free(lg_window *w, lg_kernel *k);

/* param may be NULL (then lg_param is all zeros). */
void       lg_gpu_run(lg_window *w, lg_kernel *k, const float param[4]);

#ifdef __cplusplus
}
#endif

#endif /* LIBREGRAPHIC_H */
