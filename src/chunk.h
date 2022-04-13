#ifndef _chunk_h_
#define _chunk_h_

#include "structs.h"

/// [issue](https://github.com/WSU-CEG-6110-4410/Remainders-Craft/issues/8)
/// [issue](https://github.com/WSU-CEG-6110-4410/Remainders-Craft/issues/22)

/// Call this function to translate a float coordinate to a
/// int chunk coordinate which can be used to locate a
/// chunk.
///\param[in] x: The float coordinate to be translated
///\param[out] x: The resulting int x coordinate
int chunked(float x);

/// Call this function to retrieve a chunk in a Model.
///\param[in] p: The x coordinate for the chunk.
///\param[in] q: The y coordinate for the chunk.
///\param[in] model: The Model pointer used by the game instance.
///\param[out] Chunk: The located Chunk pointer if found,
/// otherwise 0 is returned.
Chunk *find_chunk(int p, int q, Model *model);

/// This function gives the distance between a
/// chunk and a set of chunk coordinates.
///\param[in] chunk: The reference chunk.
///\param[in] p: The x chunk coordinate.
///\param[in] q: The y chunk coordinate.
///\param[out] int: The distance between the chunk
/// and the given coordinates.
int chunk_distance(Chunk *chunk, int p, int q);

int has_lights(Chunk *chunk);

void dirty_chunk(Chunk *chunk, Model *model);

#endif
