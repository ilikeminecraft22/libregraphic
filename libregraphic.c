/* libregraphic.c - implementation. Depends on GLFW 3 and OpenGL 3.3 core. */

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "libregraphic.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------
 * OpenGL access. On macOS we use the system header. Elsewhere we declare the
 * handful of GL functions we need and load them through GLFW, so no GL
 * loader library (GLAD, GLEW) is required.
 * --------------------------------------------------------------------- */
#ifdef __APPLE__

#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
static int lg_load_gl(void) { return 1; }

#else

typedef unsigned int GLenum, GLuint, GLbitfield;
typedef int GLint, GLsizei;
typedef char GLchar;
typedef float GLfloat;

#define GL_TEXTURE_2D 0x0DE1
#define GL_RGBA 0x1908
#define GL_RGBA8 0x8058
#define GL_UNSIGNED_BYTE 0x1401
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_NEAREST 0x2600
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_TRIANGLES 0x0004
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_TEXTURE0 0x84C0
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_PACK_ALIGNMENT 0x0D05

#define LG_GL_FUNCS(X)                                                         \
    X(GLuint, glCreateShader, (GLenum))                                        \
    X(void, glShaderSource, (GLuint, GLsizei, const GLchar *const *, const GLint *)) \
    X(void, glCompileShader, (GLuint))                                         \
    X(void, glGetShaderiv, (GLuint, GLenum, GLint *))                          \
    X(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *))        \
    X(void, glDeleteShader, (GLuint))                                          \
    X(GLuint, glCreateProgram, (void))                                         \
    X(void, glAttachShader, (GLuint, GLuint))                                  \
    X(void, glLinkProgram, (GLuint))                                           \
    X(void, glGetProgramiv, (GLuint, GLenum, GLint *))                         \
    X(void, glGetProgramInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *))       \
    X(void, glDeleteProgram, (GLuint))                                         \
    X(void, glUseProgram, (GLuint))                                            \
    X(GLint, glGetUniformLocation, (GLuint, const GLchar *))                   \
    X(void, glUniform1i, (GLint, GLint))                                       \
    X(void, glUniform1f, (GLint, GLfloat))                                     \
    X(void, glUniform2f, (GLint, GLfloat, GLfloat))                            \
    X(void, glUniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat))          \
    X(void, glGenVertexArrays, (GLsizei, GLuint *))                            \
    X(void, glBindVertexArray, (GLuint))                                       \
    X(void, glDeleteVertexArrays, (GLsizei, const GLuint *))                   \
    X(void, glGenFramebuffers, (GLsizei, GLuint *))                            \
    X(void, glBindFramebuffer, (GLenum, GLuint))                               \
    X(void, glFramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint))   \
    X(GLenum, glCheckFramebufferStatus, (GLenum))                              \
    X(void, glDeleteFramebuffers, (GLsizei, const GLuint *))                   \
    X(void, glActiveTexture, (GLenum))                                         \
    X(void, glGenTextures, (GLsizei, GLuint *))                                \
    X(void, glBindTexture, (GLenum, GLuint))                                   \
    X(void, glTexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *)) \
    X(void, glTexSubImage2D, (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *)) \
    X(void, glTexParameteri, (GLenum, GLenum, GLint))                          \
    X(void, glDeleteTextures, (GLsizei, const GLuint *))                       \
    X(void, glDrawArrays, (GLenum, GLint, GLsizei))                            \
    X(void, glViewport, (GLint, GLint, GLsizei, GLsizei))                      \
    X(void, glReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *)) \
    X(void, glPixelStorei, (GLenum, GLint))

#define X(ret, name, args) \
    typedef ret(*lg_pfn_##name) args; \
    static lg_pfn_##name name;
LG_GL_FUNCS(X)
#undef X

static int lg_load_gl(void)
{
#define X(ret, name, args)                                   \
    name = (lg_pfn_##name)glfwGetProcAddress(#name);         \
    if (!name) return 0;
    LG_GL_FUNCS(X)
#undef X
    return 1;
}

#endif /* __APPLE__ */

/* ------------------------------------------------------------------------ */

struct lg_window {
    GLFWwindow *win;
    int width, height;
    lg_color *px;
    GLuint tex;      /* CPU buffer uploaded here (also the kernel's input) */
    GLuint out_tex;  /* kernel render target */
    GLuint fbo;
    GLuint vao;      /* empty VAO; the fullscreen triangle uses gl_VertexID */
    GLuint show;     /* program that draws tex to the window */
};

struct lg_kernel {
    GLuint prog;
    GLint l_src, l_size, l_time, l_mouse, l_param;
};

static int g_windows = 0;
static int g_gl_loaded = 0;

/* Fullscreen triangle. u_flip=1 maps the top of the screen to v=0, so that
 * row 0 of the CPU buffer is the top row. Kernel programs never set u_flip
 * (defaults to 0) so that glReadPixels returns rows in CPU-buffer order. */
static const char *VERT_SRC =
    "#version 330 core\n"
    "uniform float u_flip;\n"
    "out vec2 uv;\n"
    "void main() {\n"
    "    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
    "    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
    "    uv = vec2(p.x, mix(p.y, 1.0 - p.y, u_flip));\n"
    "}\n";

static const char *SHOW_FRAG_SRC =
    "#version 330 core\n"
    "uniform sampler2D tex;\n"
    "in vec2 uv;\n"
    "out vec4 color;\n"
    "void main() { color = vec4(texture(tex, uv).rgb, 1.0); }\n";

static const char *KERNEL_PRELUDE =
    "#version 330 core\n"
    "uniform sampler2D lg_src;\n"
    "uniform vec2 lg_size;\n"
    "uniform float lg_time;\n"
    "uniform vec2 lg_mouse;\n"
    "uniform vec4 lg_param;\n"
    "out vec4 lg_out;\n"
    "vec4 lg_pixel(vec2 p) {\n"
    "    ivec2 q = clamp(ivec2(p), ivec2(0), ivec2(lg_size) - ivec2(1));\n"
    "    return texelFetch(lg_src, q, 0);\n"
    "}\n"
    "#line 1\n";

static const char *KERNEL_EPILOGUE =
    "\nvoid main() { lg_out = lg_kernel(floor(gl_FragCoord.xy)); }\n";

/* ------------------------------------------------------------ GL helpers */

static GLuint compile_shader(GLenum type, const char *src, char *err, size_t errlen)
{
    GLuint s = glCreateShader(type);
    GLint ok = 0;
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        if (err && errlen) {
            err[0] = '\0';
            glGetShaderInfoLog(s, (GLsizei)errlen, NULL, err);
        }
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint build_program(const char *vsrc, const char *fsrc, char *err, size_t errlen)
{
    GLuint vs, fs, p;
    GLint ok = 0;
    vs = compile_shader(GL_VERTEX_SHADER, vsrc, err, errlen);
    if (!vs) return 0;
    fs = compile_shader(GL_FRAGMENT_SHADER, fsrc, err, errlen);
    if (!fs) { glDeleteShader(vs); return 0; }
    p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs);
    glDeleteShader(fs);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        if (err && errlen) {
            err[0] = '\0';
            glGetProgramInfoLog(p, (GLsizei)errlen, NULL, err);
        }
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

static GLuint make_texture(int w, int h)
{
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}

static void upload(lg_window *w)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, w->tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w->width, w->height,
                    GL_RGBA, GL_UNSIGNED_BYTE, w->px);
}

/* ---------------------------------------------------------------- window */

lg_window *lg_open(const char *title, int width, int height)
{
    lg_window *w;
    GLFWwindow *gw;
    GLint loc;

    if (width <= 0 || height <= 0) return NULL;
    if (g_windows == 0 && !glfwInit()) return NULL;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    gw = glfwCreateWindow(width, height, title ? title : "libregraphic", NULL, NULL);
    if (!gw) {
        if (g_windows == 0) glfwTerminate();
        return NULL;
    }
    glfwMakeContextCurrent(gw);

    if (!g_gl_loaded) {
        if (!lg_load_gl()) {
            glfwDestroyWindow(gw);
            if (g_windows == 0) glfwTerminate();
            return NULL;
        }
        g_gl_loaded = 1;
    }

    w = (lg_window *)calloc(1, sizeof *w);
    if (w) w->px = (lg_color *)malloc((size_t)width * (size_t)height * sizeof(lg_color));
    if (!w || !w->px) {
        free(w);
        glfwDestroyWindow(gw);
        if (g_windows == 0) glfwTerminate();
        return NULL;
    }
    w->win = gw;
    w->width = width;
    w->height = height;
    g_windows++;

    glfwSwapInterval(1);
    lg_clear(w, LG_RGB(0, 0, 0));

    w->tex = make_texture(width, height);
    w->out_tex = make_texture(width, height);
    glGenFramebuffers(1, &w->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, w->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, w->out_tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        lg_close(w);
        return NULL;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGenVertexArrays(1, &w->vao);
    w->show = build_program(VERT_SRC, SHOW_FRAG_SRC, NULL, 0);
    if (!w->show) {
        lg_close(w);
        return NULL;
    }
    glUseProgram(w->show);
    loc = glGetUniformLocation(w->show, "tex");
    glUniform1i(loc, 0);
    loc = glGetUniformLocation(w->show, "u_flip");
    glUniform1f(loc, 1.0f);

    return w;
}

void lg_close(lg_window *w)
{
    if (!w) return;
    glfwMakeContextCurrent(w->win);
    glDeleteProgram(w->show);
    glDeleteVertexArrays(1, &w->vao);
    glDeleteFramebuffers(1, &w->fbo);
    glDeleteTextures(1, &w->tex);
    glDeleteTextures(1, &w->out_tex);
    glfwDestroyWindow(w->win);
    free(w->px);
    free(w);
    if (--g_windows == 0) {
        glfwTerminate();
        g_gl_loaded = 0;
    }
}

int lg_should_close(lg_window *w)
{
    return glfwWindowShouldClose(w->win);
}

void lg_present(lg_window *w)
{
    int fw, fh;
    glfwMakeContextCurrent(w->win);
    upload(w);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glfwGetFramebufferSize(w->win, &fw, &fh);
    glViewport(0, 0, fw, fh);
    glUseProgram(w->show);
    glBindVertexArray(w->vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glfwSwapBuffers(w->win);
    glfwPollEvents();
}

/* ---------------------------------------------------------------- pixels */

int lg_width(lg_window *w) { return w->width; }
int lg_height(lg_window *w) { return w->height; }
lg_color *lg_pixels(lg_window *w) { return w->px; }

void lg_clear(lg_window *w, lg_color c)
{
    size_t i, n = (size_t)w->width * (size_t)w->height;
    for (i = 0; i < n; i++) w->px[i] = c;
}

void lg_set(lg_window *w, int x, int y, lg_color c)
{
    if (x < 0 || y < 0 || x >= w->width || y >= w->height) return;
    w->px[(size_t)y * (size_t)w->width + (size_t)x] = c;
}

lg_color lg_get(lg_window *w, int x, int y)
{
    if (x < 0 || y < 0 || x >= w->width || y >= w->height) return 0;
    return w->px[(size_t)y * (size_t)w->width + (size_t)x];
}

static void hspan(lg_window *w, int x0, int x1, int y, lg_color c)
{
    int x;
    if (y < 0 || y >= w->height) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (x0 < 0) x0 = 0;
    if (x1 >= w->width) x1 = w->width - 1;
    for (x = x0; x <= x1; x++) w->px[(size_t)y * (size_t)w->width + (size_t)x] = c;
}

void lg_rect(lg_window *w, int x, int y, int rw, int rh, lg_color c)
{
    int j;
    if (rw <= 0 || rh <= 0) return;
    for (j = y; j < y + rh; j++) hspan(w, x, x + rw - 1, j, c);
}

void lg_line(lg_window *w, int x0, int y0, int x1, int y1, lg_color c)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        lg_set(w, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        {
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
}

void lg_circle(lg_window *w, int cx, int cy, int r, lg_color c)
{
    int dy;
    if (r < 0) return;
    for (dy = -r; dy <= r; dy++) {
        int dx = (int)floor(sqrt((double)(r * r - dy * dy)) + 0.5);
        hspan(w, cx - dx, cx + dx, cy + dy, c);
    }
}

/* ----------------------------------------------------------------- input */

double lg_time(void) { return glfwGetTime(); }

void lg_mouse(lg_window *w, int *x, int *y)
{
    double mx, my;
    int ww, wh;
    glfwGetCursorPos(w->win, &mx, &my);
    glfwGetWindowSize(w->win, &ww, &wh);
    if (ww < 1) ww = 1;
    if (wh < 1) wh = 1;
    if (x) *x = (int)floor(mx * (double)w->width / (double)ww);
    if (y) *y = (int)floor(my * (double)w->height / (double)wh);
}

int lg_mouse_down(lg_window *w, int button)
{
    return glfwGetMouseButton(w->win, button) == GLFW_PRESS;
}

int lg_key_down(lg_window *w, int key)
{
    return glfwGetKey(w->win, key) == GLFW_PRESS;
}

/* ------------------------------------------------------------ GPU kernels */

lg_kernel *lg_kernel_create(lg_window *w, const char *glsl, char *err, size_t errlen)
{
    lg_kernel *k;
    char *src;
    size_t n;
    GLuint prog;

    if (err && errlen) err[0] = '\0';
    if (!glsl) return NULL;

    n = strlen(KERNEL_PRELUDE) + strlen(glsl) + strlen(KERNEL_EPILOGUE) + 1;
    src = (char *)malloc(n);
    if (!src) return NULL;
    snprintf(src, n, "%s%s%s", KERNEL_PRELUDE, glsl, KERNEL_EPILOGUE);

    glfwMakeContextCurrent(w->win);
    prog = build_program(VERT_SRC, src, err, errlen);
    free(src);
    if (!prog) return NULL;

    k = (lg_kernel *)calloc(1, sizeof *k);
    if (!k) { glDeleteProgram(prog); return NULL; }
    k->prog = prog;
    k->l_src = glGetUniformLocation(prog, "lg_src");
    k->l_size = glGetUniformLocation(prog, "lg_size");
    k->l_time = glGetUniformLocation(prog, "lg_time");
    k->l_mouse = glGetUniformLocation(prog, "lg_mouse");
    k->l_param = glGetUniformLocation(prog, "lg_param");
    return k;
}

void lg_kernel_free(lg_window *w, lg_kernel *k)
{
    if (!k) return;
    glfwMakeContextCurrent(w->win);
    glDeleteProgram(k->prog);
    free(k);
}

void lg_gpu_run(lg_window *w, lg_kernel *k, const float param[4])
{
    static const float zero[4] = {0, 0, 0, 0};
    int mx, my;
    if (!k) return;
    if (!param) param = zero;

    glfwMakeContextCurrent(w->win);
    upload(w);

    glBindFramebuffer(GL_FRAMEBUFFER, w->fbo);
    glViewport(0, 0, w->width, w->height);
    glUseProgram(k->prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, w->tex);

    lg_mouse(w, &mx, &my);
    glUniform1i(k->l_src, 0);
    glUniform2f(k->l_size, (GLfloat)w->width, (GLfloat)w->height);
    glUniform1f(k->l_time, (GLfloat)glfwGetTime());
    glUniform2f(k->l_mouse, (GLfloat)mx, (GLfloat)my);
    glUniform4f(k->l_param, param[0], param[1], param[2], param[3]);

    glBindVertexArray(w->vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w->width, w->height, GL_RGBA, GL_UNSIGNED_BYTE, w->px);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
