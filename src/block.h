#ifndef _block_h_
#define _block_h_

#include "structs.h"
#include "db.h"

void unset_sign(int x, int y, int z, Model *model);

void set_light(int p, int q, int x, int y, int z, int w);

void _set_block(int p, int q, int x, int y, int z, int w, int dirty, Model *model);

void set_block(int x, int y, int z, int w, Model *model);

void record_block(int x, int y, int z, int w, Model *model);

int get_block(int x, int y, int z, Model *model);

int player_intersects_block(
    int height,
    float x, float y, float z,
    int hx, int hy, int hz);

void builder_block(int x, int y, int z, int w, Model *model);

void copy(Model *model);

void paste(Model *model);

void array(Block *b1, Block *b2, int xc, int yc, int zc, Model *model);

void cube(Block *b1, Block *b2, int fill, Model *model);

void sphere(Block *center, int radius, int fill, int fx, int fy, int fz, Model *model);

void cylinder(Block *b1, Block *b2, int radius, int fill, Model *model);

void tree(Block *block, Model *model);

#endif