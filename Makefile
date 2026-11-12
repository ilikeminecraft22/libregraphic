CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -std=c99
GLFW_CFLAGS := $(shell pkg-config --cflags glfw3)
GLFW_LIBS   := $(shell pkg-config --libs glfw3)

UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
  EXTRA_LIBS := -framework OpenGL
else
  EXTRA_LIBS := -ldl
endif

all: libregraphic.a demo

libregraphic.o: libregraphic.c libregraphic.h
	$(CC) $(CFLAGS) $(GLFW_CFLAGS) -c libregraphic.c -o $@

libregraphic.a: libregraphic.o
	ar rcs $@ $^

demo: examples/demo.c libregraphic.a
	$(CC) $(CFLAGS) -I. $(GLFW_CFLAGS) examples/demo.c libregraphic.a $(GLFW_LIBS) $(EXTRA_LIBS) -lm -o $@

clean:
	rm -f libregraphic.o libregraphic.a demo

.PHONY: all clean
