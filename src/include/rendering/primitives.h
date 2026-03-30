#ifndef RENDERING_PRIMITIVES_H
#define RENDERING_PRIMITIVES_H

#include "structs.h"

typedef struct Quad {
    Vec3 v[4];
} Quad;

typedef struct Sprite {
    Vec3 v[4];
    TexCoord t[4];
    unsigned int tex_code;
} Sprite;

typedef struct Sprite2 {
    Vec3 v[2];
    TexCoord t[2];
    unsigned int vertex_color;
    unsigned int tex_code;
    unsigned int id;
    float modelX; // Per-character X offset (vertex shader model transform, Opt #3)
    float modelY; // Per-character Y offset (vertex shader model transform, Opt #3)
} Sprite2;

#endif
