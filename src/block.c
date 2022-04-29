#include "client.h"
#include "db.h"
#include "util.h"
#include <math.h>
#include "structs.h"
#include "chunk.h"
#include "item.h"
#include "string.h"

void unset_sign_face(int x, int y, int z, int face, Model *model)
{
    int p = chunked(x);
    int q = chunked(z);
    Chunk *chunk = find_chunk(p, q, model);
    if (chunk)
    {
        SignList *signs = &chunk->signs;
        if (sign_list_remove(signs, x, y, z, face))
        {
            chunk->dirty = 1;
            db_delete_sign(x, y, z, face);
        }
    }
    else
    {
        db_delete_sign(x, y, z, face);
    }
}

void _set_sign(
    int p, int q, int x, int y, int z, int face, const char *text, int dirty, Model *model)
{
    if (strlen(text) == 0)
    {
        unset_sign_face(x, y, z, face, model);
        return;
    }
    Chunk *chunk = find_chunk(p, q, model);
    if (chunk)
    {
        SignList *signs = &chunk->signs;
        sign_list_add(signs, x, y, z, face, text);
        if (dirty)
        {
            chunk->dirty = 1;
        }
    }
    db_insert_sign(p, q, x, y, z, face, text);
}

void set_sign(int x, int y, int z, int face, const char *text, Model *model)
{
    int p = chunked(x);
    int q = chunked(z);
    _set_sign(p, q, x, y, z, face, text, 1, model);
    client_sign(x, y, z, face, text);
}

void unset_sign(int x, int y, int z, Model *model)
{
    int p = chunked(x);
    int q = chunked(z);
    Chunk *chunk = find_chunk(p, q, model);
    if (chunk)
    {
        SignList *signs = &chunk->signs;
        if (sign_list_remove_all(signs, x, y, z))
        {
            chunk->dirty = 1;
            db_delete_signs(x, y, z);
        }
    }
    else
    {
        db_delete_signs(x, y, z);
    }
}

void set_light(int p, int q, int x, int y, int z, int w, Model *model)
{
    Chunk *chunk = find_chunk(p, q, model);
    if (chunk)
    {
        Map *map = &chunk->lights;
        if (map_set(map, x, y, z, w))
        {
            dirty_chunk(chunk, model);
            db_insert_light(p, q, x, y, z, w);
        }
    }
    else
    {
        db_insert_light(p, q, x, y, z, w);
    }
}

void _set_block(int p, int q, int x, int y, int z, int w, int dirty, Model *model)
{
    Chunk *chunk = find_chunk(p, q, model);
    if (chunk)
    {
        Map *map = &chunk->map;
        if (map_set(map, x, y, z, w))
        {
            if (dirty)
            {
                dirty_chunk(chunk, model);
            }
            db_insert_block(p, q, x, y, z, w);
        }
    }
    else
    {
        db_insert_block(p, q, x, y, z, w);
    }
    if (w == 0 && chunked(x) == p && chunked(z) == q)
    {
        unset_sign(x, y, z, model);
        set_light(p, q, x, y, z, 0, model);
    }
}

void set_block(int x, int y, int z, int w, Model *model)
{
    int p = chunked(x);
    int q = chunked(z);
    _set_block(p, q, x, y, z, w, 1, model);
    for (int dx = -1; dx <= 1; dx++)
    {
        for (int dz = -1; dz <= 1; dz++)
        {
            if (dx == 0 && dz == 0)
            {
                continue;
            }
            if (dx && chunked(x + dx) == p)
            {
                continue;
            }
            if (dz && chunked(z + dz) == q)
            {
                continue;
            }
            _set_block(p + dx, q + dz, x, y, z, -w, 1, model);
        }
    }
    client_block(x, y, z, w);
}

void record_block(int x, int y, int z, int w, Model *model)
{
    memcpy(&model->block1, &model->block0, sizeof(Block));
    model->block0.x = x;
    model->block0.y = y;
    model->block0.z = z;
    model->block0.w = w;
}

int get_block(int x, int y, int z, Model *model)
{
    int p = chunked(x);
    int q = chunked(z);
    Chunk *chunk = find_chunk(p, q, model);
    if (chunk)
    {
        Map *map = &chunk->map;
        return map_get(map, x, y, z);
    }
    return 0;
}

int player_intersects_block(
    int height,
    float x, float y, float z,
    int hx, int hy, int hz)
{
    int nx = roundf(x);
    int ny = roundf(y);
    int nz = roundf(z);
    for (int i = 0; i < height; i++)
    {
        if (nx == hx && ny - i == hy && nz == hz)
        {
            return 1;
        }
    }
    return 0;
}

void builder_block(int x, int y, int z, int w, Model *model)
{
    if (y <= 0 || y >= 256)
    {
        return;
    }
    if (is_destructable(get_block(x, y, z, model)))
    {
        set_block(x, y, z, 0, model);
    }
    if (w)
    {
        set_block(x, y, z, w, model);
    }
}

void copy(Model *model)
{
    memcpy(&model->copy0, &model->block0, sizeof(Block));
    memcpy(&model->copy1, &model->block1, sizeof(Block));
}

void paste(Model *model)
{
    Block *c1 = &model->copy1;
    Block *c2 = &model->copy0;
    Block *p1 = &model->block1;
    Block *p2 = &model->block0;
    int scx = SIGN(c2->x - c1->x);
    int scz = SIGN(c2->z - c1->z);
    int spx = SIGN(p2->x - p1->x);
    int spz = SIGN(p2->z - p1->z);
    int oy = p1->y - c1->y;
    int dx = ABS(c2->x - c1->x);
    int dz = ABS(c2->z - c1->z);
    for (int y = 0; y < 256; y++)
    {
        for (int x = 0; x <= dx; x++)
        {
            for (int z = 0; z <= dz; z++)
            {
                int w = get_block(c1->x + x * scx, y, c1->z + z * scz, model);
                builder_block(p1->x + x * spx, y + oy, p1->z + z * spz, w, model);
            }
        }
    }
}

void array(Block *b1, Block *b2, int xc, int yc, int zc, Model *model)
{
    if (b1->w != b2->w)
    {
        return;
    }
    int w = b1->w;
    int dx = b2->x - b1->x;
    int dy = b2->y - b1->y;
    int dz = b2->z - b1->z;
    xc = dx ? xc : 1;
    yc = dy ? yc : 1;
    zc = dz ? zc : 1;
    for (int i = 0; i < xc; i++)
    {
        int x = b1->x + dx * i;
        for (int j = 0; j < yc; j++)
        {
            int y = b1->y + dy * j;
            for (int k = 0; k < zc; k++)
            {
                int z = b1->z + dz * k;
                builder_block(x, y, z, w, model);
            }
        }
    }
}

void cube(Block *b1, Block *b2, int fill, Model *model)
{
    if (b1->w != b2->w)
    {
        return;
    }
    int w = b1->w;
    int x1 = MIN(b1->x, b2->x);
    int y1 = MIN(b1->y, b2->y);
    int z1 = MIN(b1->z, b2->z);
    int x2 = MAX(b1->x, b2->x);
    int y2 = MAX(b1->y, b2->y);
    int z2 = MAX(b1->z, b2->z);
    int a = (x1 == x2) + (y1 == y2) + (z1 == z2);
    for (int x = x1; x <= x2; x++)
    {
        for (int y = y1; y <= y2; y++)
        {
            for (int z = z1; z <= z2; z++)
            {
                if (!fill)
                {
                    int n = 0;
                    n += x == x1 || x == x2;
                    n += y == y1 || y == y2;
                    n += z == z1 || z == z2;
                    if (n <= a)
                    {
                        continue;
                    }
                }
                builder_block(x, y, z, w, model);
            }
        }
    }
}

void sphere(Block *center, int radius, int fill, int fx, int fy, int fz, Model *model)
{
    static const float offsets[8][3] = {
        {-0.5, -0.5, -0.5},
        {-0.5, -0.5, 0.5},
        {-0.5, 0.5, -0.5},
        {-0.5, 0.5, 0.5},
        {0.5, -0.5, -0.5},
        {0.5, -0.5, 0.5},
        {0.5, 0.5, -0.5},
        {0.5, 0.5, 0.5}};
    int cx = center->x;
    int cy = center->y;
    int cz = center->z;
    int w = center->w;
    for (int x = cx - radius; x <= cx + radius; x++)
    {
        if (fx && x != cx)
        {
            continue;
        }
        for (int y = cy - radius; y <= cy + radius; y++)
        {
            if (fy && y != cy)
            {
                continue;
            }
            for (int z = cz - radius; z <= cz + radius; z++)
            {
                if (fz && z != cz)
                {
                    continue;
                }
                int inside = 0;
                int outside = fill;
                for (int i = 0; i < 8; i++)
                {
                    float dx = x + offsets[i][0] - cx;
                    float dy = y + offsets[i][1] - cy;
                    float dz = z + offsets[i][2] - cz;
                    float d = sqrtf(dx * dx + dy * dy + dz * dz);
                    if (d < radius)
                    {
                        inside = 1;
                    }
                    else
                    {
                        outside = 1;
                    }
                }
                if (inside && outside)
                {
                    builder_block(x, y, z, w, model);
                }
            }
        }
    }
}

void cylinder(Block *b1, Block *b2, int radius, int fill, Model *model)
{
    if (b1->w != b2->w)
    {
        return;
    }
    int w = b1->w;
    int x1 = MIN(b1->x, b2->x);
    int y1 = MIN(b1->y, b2->y);
    int z1 = MIN(b1->z, b2->z);
    int x2 = MAX(b1->x, b2->x);
    int y2 = MAX(b1->y, b2->y);
    int z2 = MAX(b1->z, b2->z);
    int fx = x1 != x2;
    int fy = y1 != y2;
    int fz = z1 != z2;
    if (fx + fy + fz != 1)
    {
        return;
    }
    Block block = {x1, y1, z1, w};
    if (fx)
    {
        for (int x = x1; x <= x2; x++)
        {
            block.x = x;
            sphere(&block, radius, fill, 1, 0, 0, model);
        }
    }
    if (fy)
    {
        for (int y = y1; y <= y2; y++)
        {
            block.y = y;
            sphere(&block, radius, fill, 0, 1, 0, model);
        }
    }
    if (fz)
    {
        for (int z = z1; z <= z2; z++)
        {
            block.z = z;
            sphere(&block, radius, fill, 0, 0, 1, model);
        }
    }
}

void tree(Block *block, Model *model)
{
    int bx = block->x;
    int by = block->y;
    int bz = block->z;
    for (int y = by + 3; y < by + 8; y++)
    {
        for (int dx = -3; dx <= 3; dx++)
        {
            for (int dz = -3; dz <= 3; dz++)
            {
                int dy = y - (by + 4);
                int d = (dx * dx) + (dy * dy) + (dz * dz);
                if (d < 11)
                {
                    builder_block(bx + dx, y, bz + dz, 15, model);
                }
            }
        }
    }
    for (int y = by; y < by + 7; y++)
    {
        builder_block(bx, y, bz, 5, model);
    }
}
