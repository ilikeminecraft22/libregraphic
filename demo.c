/* demo.c - paint with the mouse (CPU), glowing fading trails (GPU kernel).
 *
 *   hold left mouse : paint
 *   C               : clear
 *   SPACE (hold)    : turn the GPU kernel off to compare
 *   ESC             : quit
 */
#include "libregraphic.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* Runs on the GPU for every pixel: blur with the 4 neighbours, then fade. */
static const char *TRAIL_KERNEL =
    "vec4 lg_kernel(vec2 p) {\n"
    "    vec4 c = lg_pixel(p) * 4.0\n"
    "           + lg_pixel(p + vec2( 1.0,  0.0)) + lg_pixel(p - vec2( 1.0,  0.0))\n"
    "           + lg_pixel(p + vec2( 0.0,  1.0)) + lg_pixel(p - vec2( 0.0,  1.0));\n"
    "    c /= 8.0;\n"
    "    return vec4(max(c.rgb * lg_param.x - 2.0 / 255.0, 0.0), 1.0);\n"
    "}\n";

int main(void)
{
    lg_window *w = lg_open("libregraphic demo", 800, 600);
    lg_kernel *trail;
    char err[2048];
    int lx = -1, ly = -1;

    if (!w) {
        fprintf(stderr, "could not open a window (need OpenGL 3.3)\n");
        return 1;
    }
    trail = lg_kernel_create(w, TRAIL_KERNEL, err, sizeof err);
    if (!trail) {
        fprintf(stderr, "kernel failed to compile:\n%s\n", err);
        lg_close(w);
        return 1;
    }

    while (!lg_should_close(w) && !lg_key_down(w, LG_KEY_ESCAPE)) {
        double t = lg_time();
        int mx, my;
        lg_color col = LG_RGB((unsigned char)(127 + 127 * sin(t * 2.0)),
                              (unsigned char)(127 + 127 * sin(t * 2.0 + 2.1)),
                              (unsigned char)(127 + 127 * sin(t * 2.0 + 4.2)));

        if (lg_key_down(w, 'C')) lg_clear(w, LG_RGB(0, 0, 0));

        /* CPU drawing: an orbiting dot... */
        lg_circle(w, 400 + (int)(200 * cos(t)), 300 + (int)(150 * sin(t * 1.3)), 8,
                  LG_RGB(255, 255, 255));

        /* ...and a brush that interpolates between mouse positions. */
        lg_mouse(w, &mx, &my);
        if (lg_mouse_down(w, 0)) {
            int dx, dy, steps, i;
            if (lx < 0) { lx = mx; ly = my; }
            dx = mx - lx;
            dy = my - ly;
            steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
            if (steps < 1) steps = 1;
            for (i = 0; i <= steps; i++)
                lg_circle(w, lx + dx * i / steps, ly + dy * i / steps, 6, col);
            lx = mx;
            ly = my;
        } else {
            lx = -1;
        }

        /* GPU: one function applied to every pixel. */
        if (!lg_key_down(w, LG_KEY_SPACE)) {
            float param[4] = {0.97f, 0, 0, 0};
            lg_gpu_run(w, trail, param);
        }

        lg_present(w);
    }

    lg_kernel_free(w, trail);
    lg_close(w);
    return 0;
}
