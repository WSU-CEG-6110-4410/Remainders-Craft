#include "structs.h"

#ifndef _input_h_
#define _input_h_

void get_motion_vector(int flying, int sz, int sx, float rx, float ry, float *vx, float *vy, float *vz);

int chunked(float x);

Chunk *find_chunk(int p, int q, Model *model);

int collide(int height, float *x, float *y, float *z, Model *model);

int highest_block(float x, float z, Model *model);

void handle_movement(double dt, Model *model);

void handle_mouse_input(Model *model);

void get_sight_vector(float rx, float ry, float *vx, float *vy, float *vz);

int chunk_distance(Chunk *chunk, int p, int q);

_hit_test(
    Map *map, float max_distance, int previous,
    float x, float y, float z,
    float vx, float vy, float vz,
    int *hx, int *hy, int *hz);

int hit_test(
    int previous, float x, float y, float z, float rx, float ry,
    int *bx, int *by, int *bz, Model *model);

int hit_test_face(Player *player, int *x, int *y, int *z, int *face, Model *model);

int has_lights(Chunk *chunk);

void dirty_chunk(Chunk *chunk, Model *model);

void unset_sign(int x, int y, int z, Model *model);

void set_light(int p, int q, int x, int y, int z, int w);

void _set_block(int p, int q, int x, int y, int z, int w, int dirty, Model *model);

void set_block(int x, int y, int z, int w, Model *model);

void record_block(int x, int y, int z, int w, Model *model);

int get_block(int x, int y, int z, Model *model);

void on_left_click(Model *model);

int player_intersects_block(
    int height,
    float x, float y, float z,
    int hx, int hy, int hz);

void on_right_click(Model *model);

void on_middle_click(Model *model);

void toggle_light(int x, int y, int z, Model *model);

void on_light(Model *model);

void on_mouse_button(GLFWwindow *window, int button, int action, int mods, Model *model);

void on_scroll(GLFWwindow *window, double xdelta, double ydelta, Model *model);

void on_char(GLFWwindow *window, unsigned int u, Model *model);

void unset_sign_face(int x, int y, int z, int face, Model *model);

void _set_sign(
    int p, int q, int x, int y, int z, int face, const char *text, int dirty, Model *model);

void add_message(const char *text, Model *model);

void login(Model *model);

void builder_block(int x, int y, int z, int w, Model *model);

void copy(Model *model);

void paste(Model *model);

void array(Block *b1, Block *b2, int xc, int yc, int zc, Model *model);

void cube(Block *b1, Block *b2, int fill, Model *model);

void sphere(Block *center, int radius, int fill, int fx, int fy, int fz, Model *model);

void cylinder(Block *b1, Block *b2, int radius, int fill, Model *model);

void tree(Block *block, Model *model);

void parse_command(const char *buffer, int forward, Model *model);

void on_key(GLFWwindow *window, int key, int scancode, int action, int mods, Model *model);

#endif