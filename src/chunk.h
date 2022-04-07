#ifndef _chunk_h_
#define _chunk_h_

int chunked(float x);

Chunk *find_chunk(int p, int q, Model *model);

int chunk_distance(Chunk *chunk, int p, int q);

int has_lights(Chunk *chunk);

void dirty_chunk(Chunk *chunk, Model *model);


#endif