#include "structs.h"

#ifndef _input_h_
#define _input_h_

void get_motion_vector(int flying, int sz, int sx, float rx, float ry, float *vx, float *vy, float *vz);

int chunked(float x);

Chunk *find_chunk(int p, int q, Model *model);

int collide(int height, float *x, float *y, float *z, Model *model);

int highest_block(float x, float z, Model *model);

void handle_movement(double dt, Model *model);

#endif