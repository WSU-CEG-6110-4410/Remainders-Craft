#ifndef _chunk_h_
#define _chunk_h_

#include "structs.h"

/// [issue](https://github.com/WSU-CEG-6110-4410/Remainders-Craft/issues/8)
/// These functions were derived from main.c. Further documentation is necessary.

int chunked(float x);

Chunk *find_chunk(int p, int q, Model *model);

int chunk_distance(Chunk *chunk, int p, int q);

int has_lights(Chunk *chunk);

void dirty_chunk(Chunk *chunk, Model *model);


#endif
