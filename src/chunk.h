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

/// This function determines whether a specified chunk
/// should have light by comparing its surrounding chunks.
///\param[in] chunk: The chunk which is being checked whether
/// it should have light
///\param[in] model: The current game instance which includes
/// the other chunks in the game.
///\param[out] int: 1 indicates the chunk should have light, 0
/// indicates that it should not.
int has_lights(Chunk *chunk);

/// Use this function to set the dirty flag for a Chunk, which
/// indicates that the Chunck should stay rendered for the player.
/// It will also set the surrounding Chunks as dirty if they have light.
///\param[in] chunk: The chunk to be set as dirty.
///\param[in,out] model: The game instance containing all
/// chunks. Surrounding chunks to "chunk" may be modified.
void dirty_chunk(Chunk *chunk, Model *model);
#endif
