#include "map.h"
#include <math.h>
#include "structs.h"
#include "chunk.h"
#include "item.h"
#include "util.h"

void get_sight_vector(float rx, float ry, float *vx, float *vy, float *vz)
{
    float m = cosf(ry);
    *vx = cosf(rx - RADIANS(90)) * m;
    *vy = sinf(ry);
    *vz = sinf(rx - RADIANS(90)) * m;
}

int _hit_test(
    Map *map, float max_distance, int previous,
    float x, float y, float z,
    float vx, float vy, float vz,
    int *hx, int *hy, int *hz)
{
    int m = 32;
    int px = 0;
    int py = 0;
    int pz = 0;
    for (int i = 0; i < max_distance * m; i++)
    {
        int nx = roundf(x);
        int ny = roundf(y);
        int nz = roundf(z);
        if (nx != px || ny != py || nz != pz)
        {
            int hw = map_get(map, nx, ny, nz);
            if (hw > 0)
            {
                if (previous)
                {
                    *hx = px;
                    *hy = py;
                    *hz = pz;
                }
                else
                {
                    *hx = nx;
                    *hy = ny;
                    *hz = nz;
                }
                return hw;
            }
            px = nx;
            py = ny;
            pz = nz;
        }
        x += vx / m;
        y += vy / m;
        z += vz / m;
    }
    return 0;
}

int hit_test(
    int previous, float x, float y, float z, float rx, float ry,
    int *bx, int *by, int *bz, Model *model)
{
    int result = 0;
    float best = 0;
    int p = chunked(x);
    int q = chunked(z);
    float vx, vy, vz;
    get_sight_vector(rx, ry, &vx, &vy, &vz);
    for (int i = 0; i < model->chunk_count; i++)
    {
        Chunk *chunk = model->chunks + i;
        if (chunk_distance(chunk, p, q) > 1)
        {
            continue;
        }
        int hx, hy, hz;
        int hw = _hit_test(&chunk->map, 8, previous,
                           x, y, z, vx, vy, vz, &hx, &hy, &hz);
        if (hw > 0)
        {
            float d = sqrtf(
                powf(hx - x, 2) + powf(hy - y, 2) + powf(hz - z, 2));
            if (best == 0 || d < best)
            {
                best = d;
                *bx = hx;
                *by = hy;
                *bz = hz;
                result = hw;
            }
        }
    }
    return result;
}

int hit_test_face(Player *player, int *x, int *y, int *z, int *face, Model *model)
{
    State *s = &player->state;
    int w = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, x, y, z, model);
    if (is_obstacle(w))
    {
        int hx, hy, hz;
        hit_test(1, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz, model);
        int dx = hx - *x;
        int dy = hy - *y;
        int dz = hz - *z;
        if (dx == -1 && dy == 0 && dz == 0)
        {
            *face = 0;
            return 1;
        }
        if (dx == 1 && dy == 0 && dz == 0)
        {
            *face = 1;
            return 1;
        }
        if (dx == 0 && dy == 0 && dz == -1)
        {
            *face = 2;
            return 1;
        }
        if (dx == 0 && dy == 0 && dz == 1)
        {
            *face = 3;
            return 1;
        }
        if (dx == 0 && dy == 1 && dz == 0)
        {
            int degrees = roundf(DEGREES(atan2f(s->x - hx, s->z - hz)));
            if (degrees < 0)
            {
                degrees += 360;
            }
            int top = ((degrees + 45) / 90) % 4;
            *face = 4 + top;
            return 1;
        }
    }
    return 0;
}
