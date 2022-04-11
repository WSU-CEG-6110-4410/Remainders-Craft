#include "config.h"
#include "structs.h"
#include <math.h>
#include "util.h"

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

int chunk_distance(Chunk *chunk, int p, int q)
{
    int dp = ABS(chunk->p - p);
    int dq = ABS(chunk->q - q);
    return MAX(dp, dq);
}

int has_lights(Chunk *chunk, Model *model)
{
    if (!SHOW_LIGHTS)
    {
        return 0;
    }
    for (int dp = -1; dp <= 1; dp++)
    {
        for (int dq = -1; dq <= 1; dq++)
        {
            Chunk *other = chunk;
            if (dp || dq)
            {
                other = find_chunk(chunk->p + dp, chunk->q + dq, model);
            }
            if (!other)
            {
                continue;
            }
            Map *map = &other->lights;
            if (map->size)
            {
                return 1;
            }
        }
    }
    return 0;
}

void dirty_chunk(Chunk *chunk, Model *model)
{
    chunk->dirty = 1;
    if (has_lights(chunk, model))
    {
        for (int dp = -1; dp <= 1; dp++)
        {
            for (int dq = -1; dq <= 1; dq++)
            {
                Chunk *other = find_chunk(chunk->p + dp, chunk->q + dq, model);
                if (other)
                {
                    other->dirty = 1;
                }
            }
        }
    }
}
