#include "config.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "structs.h"
#include <math.h>
#include "util.h"
#include "item.h"

void get_motion_vector(int flying, int sz, int sx, float rx, float ry,
                       float *vx, float *vy, float *vz)
{
    *vx = 0;
    *vy = 0;
    *vz = 0;
    if (!sz && !sx)
    {
        return;
    }
    float strafe = atan2f(sz, sx);
    if (flying)
    {
        float m = cosf(ry);
        float y = sinf(ry);
        if (sx)
        {
            if (!sz)
            {
                y = 0;
            }
            m = 1;
        }
        if (sz > 0)
        {
            y = -y;
        }
        *vx = cosf(rx + strafe) * m;
        *vy = y;
        *vz = sinf(rx + strafe) * m;
    }
    else
    {
        *vx = cosf(rx + strafe);
        *vy = 0;
        *vz = sinf(rx + strafe);
    }
}

int chunked(float x)
{
    return floorf(roundf(x) / CHUNK_SIZE);
}

Chunk *find_chunk(int p, int q, Model *model)
{
    for (int i = 0; i < model->chunk_count; i++)
    {
        Chunk *chunk = model->chunks + i;
        if (chunk->p == p && chunk->q == q)
        {
            return chunk;
        }
    }
    return 0;
}

int collide(int height, float *x, float *y, float *z, Model *model)
{
    int result = 0;
    int p = chunked(*x);
    int q = chunked(*z);
    Chunk *chunk = find_chunk(p, q, model);
    if (!chunk)
    {
        return result;
    }
    Map *map = &chunk->map;
    int nx = roundf(*x);
    int ny = roundf(*y);
    int nz = roundf(*z);
    float px = *x - nx;
    float py = *y - ny;
    float pz = *z - nz;
    float pad = 0.25;
    for (int dy = 0; dy < height; dy++)
    {
        if (px < -pad && is_obstacle(map_get(map, nx - 1, ny - dy, nz)))
        {
            *x = nx - pad;
        }
        if (px > pad && is_obstacle(map_get(map, nx + 1, ny - dy, nz)))
        {
            *x = nx + pad;
        }
        if (py < -pad && is_obstacle(map_get(map, nx, ny - dy - 1, nz)))
        {
            *y = ny - pad;
            result = 1;
        }
        if (py > pad && is_obstacle(map_get(map, nx, ny - dy + 1, nz)))
        {
            *y = ny + pad;
            result = 1;
        }
        if (pz < -pad && is_obstacle(map_get(map, nx, ny - dy, nz - 1)))
        {
            *z = nz - pad;
        }
        if (pz > pad && is_obstacle(map_get(map, nx, ny - dy, nz + 1)))
        {
            *z = nz + pad;
        }
    }
    return result;
}

int highest_block(float x, float z, Model *model)
{
    int result = -1;
    int nx = roundf(x);
    int nz = roundf(z);
    int p = chunked(x);
    int q = chunked(z);
    Chunk *chunk = find_chunk(p, q, model);
    if (chunk)
    {
        Map *map = &chunk->map;
        MAP_FOR_EACH(map, ex, ey, ez, ew)
        {
            if (is_obstacle(ew) && ex == nx && ez == nz)
            {
                result = MAX(result, ey);
            }
        }
        END_MAP_FOR_EACH;
    }
    return result;
}

void handle_movement(double dt, Model *model)
{
    static float dy = 0;
    State *s = &model->players->state;
    int sz = 0;
    int sx = 0;
    if (!model->typing)
    {
        float m = dt * 1.0;
        model->ortho = glfwGetKey(model->window, CRAFT_KEY_ORTHO) ? 64 : 0;
        model->fov = glfwGetKey(model->window, CRAFT_KEY_ZOOM) ? 15 : 65;
        if (glfwGetKey(model->window, CRAFT_KEY_FORWARD))
            sz--;
        if (glfwGetKey(model->window, CRAFT_KEY_BACKWARD))
            sz++;
        if (glfwGetKey(model->window, CRAFT_KEY_LEFT))
            sx--;
        if (glfwGetKey(model->window, CRAFT_KEY_RIGHT))
            sx++;
        if (glfwGetKey(model->window, GLFW_KEY_LEFT))
            s->rx -= m;
        if (glfwGetKey(model->window, GLFW_KEY_RIGHT))
            s->rx += m;
        if (glfwGetKey(model->window, GLFW_KEY_UP))
            s->ry += m;
        if (glfwGetKey(model->window, GLFW_KEY_DOWN))
            s->ry -= m;
    }
    float vx, vy, vz;
    //HERE
    get_motion_vector(model->flying, sz, sx, s->rx, s->ry, &vx, &vy, &vz);
    if (!model->typing)
    {
        if (glfwGetKey(model->window, CRAFT_KEY_JUMP))
        {
            if (model->flying)
            {
                vy = 1;
            }
            else if (dy == 0)
            {
                dy = 8;
            }
        }
    }
    float speed = model->flying ? 20 : 5;
    int estimate = roundf(sqrtf(
                              powf(vx * speed, 2) +
                              powf(vy * speed + ABS(dy) * 2, 2) +
                              powf(vz * speed, 2)) *
                          dt * 8);
    int step = MAX(8, estimate);
    float ut = dt / step;
    vx = vx * ut * speed;
    vy = vy * ut * speed;
    vz = vz * ut * speed;
    for (int i = 0; i < step; i++)
    {
        if (model->flying)
        {
            dy = 0;
        }
        else
        {
            dy -= ut * 25;
            dy = MAX(dy, -250);
        }
        s->x += vx;
        s->y += vy + dy * ut;
        s->z += vz;
        //HERE
        if (collide(2, &s->x, &s->y, &s->z, model))
        {
            dy = 0;
        }
    }
    if (s->y < 0)
    {
        s->y = highest_block(s->x, s->z, model) + 2;
    }
}
