#ifndef _hit_h_
#define _hit_h_

void get_sight_vector(float rx, float ry, float *vx, float *vy, float *vz);

_hit_test(
    Map *map, float max_distance, int previous,
    float x, float y, float z,
    float vx, float vy, float vz,
    int *hx, int *hy, int *hz);

int hit_test(
    int previous, float x, float y, float z, float rx, float ry,
    int *bx, int *by, int *bz, Model *model);

int hit_test_face(Player *player, int *x, int *y, int *z, int *face, Model *model);

#endif