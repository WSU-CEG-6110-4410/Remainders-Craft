#ifndef _block_h_
#define _block_h_

#include "structs.h"
#include "db.h"

/// [issue](https://github.com/WSU-CEG-6110-4410/Remainders-Craft/issues/8)
/// [issue](https://github.com/WSU-CEG-6110-4410/Remainders-Craft/issues/21)

/// Use this function to remove a sign face from a chunk.
///\param[in] x: The x coordinate of the sign.
///\param[in] y: The y coordinate of the sign.
///\param[in] z: The z coordinate of the sign.
///\param[in] face: The face of the sign (0,1,2,3,4).
///\param[in,out] model: The current game instance to be modified.
void unset_sign_face(int x, int y, int z, int face, Model *model);

/// Use this function to set the sign text of a chunk.
///\param[in] p: The x coordinate for the chunk.
///\param[in] q: The y coordinate for the chunk.
///\param[in] x: The x coordinate of the sign.
///\param[in] y: The y coordinate of the sign.
///\param[in] z: The z coordinate of the sign.
///\param[in] face: The face of the sign (0,1,2,3,4).
///\param[in] text: The text content to be added.
///\param[in] dirty: 1 or 0. Indicates whether the chunk should
/// stay rendered for the player.
///\param[in,out] model: The current game instance to be modified.
void _set_sign(
    int p, int q, int x, int y, int z, int face, const char *text, int dirty, Model *model);

/// Use this function to set the sign text of a chunk.
///\param[in] x: The x coordinate of the sign.
///\param[in] y: The y coordinate of the sign.
///\param[in] z: The z coordinate of the sign.
///\param[in] face: The face of the sign (0,1,2,3,4).
///\param[in] text: The text content to be added.
///\param[in,out] model: The current game instance to be modified.
void set_sign(int x, int y, int z, int face, const char *text, Model *model);

/// Use this function to remove all sign text on a chunk.
///\param[in] x: The x coordinate of the sign.
///\param[in] y: The y coordinate of the sign.
///\param[in] z: The z coordinate of the sign.
///\param[in,out] model: The current game instance to be modified.
void unset_sign(int x, int y, int z, Model *model);

/// Use this function to apply lighting to a block
///\param[in] p: The x coordinate for the chunk where the block
/// is located.
///\param[in] q: The y coordinate for the chunk where the block
/// is located.
///\param[in] x: The x coordinate of the block.
///\param[in] y: The y coordinate of the block.
///\param[in] z: The z coordinate of the block.
///\param[in] w: The block type.
void set_light(int p, int q, int x, int y, int z, int w);

/// Use this function to place a block.
///\param[in] p: The x coordinate for the chunk where the block
/// is located.
///\param[in] q: The y coordinate for the chunk where the block
/// is located.
///\param[in] x: The x coordinate of the block.
///\param[in] y: The y coordinate of the block.
///\param[in] z: The z coordinate of the block.
///\param[in] w: The block type.
///\param[in] dirty: 1 or 0. Indicates whether the chunk should
/// stay rendered for the player.
///\param[in,out] model: The current game instance to be modified.
void _set_block(int p, int q, int x, int y, int z, int w, int dirty, Model *model);

/// Use this function to place a block.
///\param[in] x: The x coordinate of the block.
///\param[in] y: The y coordinate of the block.
///\param[in] z: The z coordinate of the block.
///\param[in] w: The block type.
///\param[in,out] model: The current game instance to be modified.
void set_block(int x, int y, int z, int w, Model *model);

/// Use this function to save block information to a model
/// to be used later.
///\param[in] x: The x coordinate of the block.
///\param[in] y: The y coordinate of the block.
///\param[in] z: The z coordinate of the block.
///\param[in] w: The block type.
///\param[in,out] model: The current game instance to be modified.
void record_block(int x, int y, int z, int w, Model *model);

/// Use this function to get the type of a block.
///\param[in] x: The x coordinate of the block.
///\param[in] y: The y coordinate of the block.
///\param[in] z: The z coordinate of the block.
///\param[in,out] model: The current game instance to be modified.
///\param[out] int: The type of the block. 
int get_block(int x, int y, int z, Model *model);

/// Use this function to check if a player's coordinates
/// and a block's coordinates intersect.
///\param[in] height: The height of the player.
///\param[in] x: The x coordinate of the block.
///\param[in] y: The y coordinate of the block.
///\param[in] z: The z coordinate of the block.
///\param[in] hx: The x coordinate of the player.
///\param[in] hy: The y coordinate of the player.
///\param[in] hz: The z coordinate of the player.
int player_intersects_block(
    int height,
    float x, float y, float z,
    int hx, int hy, int hz);

/// Use this function to insert a specified block into a
/// coordinate.
///\param[in] x: The x coordinate of the block.
///\param[in] y: The y coordinate of the block.
///\param[in] z: The z coordinate of the block.
///\param[in] w: The block type.
///\param[in,out] model: The current game instance to be modified.
void builder_block(int x, int y, int z, int w, Model *model);

/// Use this function to copy a recorded block temporarily in the
/// model.
///\param[in,out] model: The current game instance to be modified
/// which includes the recorded block and copy block spaces.
void copy(Model *model);

/// Use this function to paste the copied block.
///\param[in,out] model: The current game instance to be modified
/// which includes the copied block.
void paste(Model *model);

/// Use this function to place an array of blocks between two blocks.
///\param[in] b1: The first reference block.
///\param[in] b2: The second reference block.
///\param[in] xc: The amount of blocks to set in the x direction.
///\param[in] yc: The amount of blocks to set in the y direction.
///\param[in] zc: The amount of blocks to set in the z direction.
///\param[in,out] model: The current game instance to be modified.
void array(Block *b1, Block *b2, int xc, int yc, int zc, Model *model);

/// Use this function to place cubes between two blocks.
///\param[in] b1: The first reference block.
///\param[in] b2: The second reference block.
///\param[in] fill: 1 or 0. Indicates whether the whole block should be filled
///\param[in,out] model: The current game instance to be modified.
void cube(Block *b1, Block *b2, int fill, Model *model);

/// Use this function to place a sphere.
///\param[in] center: The block to be made into a sphere.
///\param[in] radius: The radius of the sphere.
///\param[in] fill: 1 or 0. Indicates whether the whole block should be filled.
///\param[in] fx: The x offset for the sphere.
///\param[in] fy: The y offset for the sphere.
///\param[in] fz: The z offset for the sphere.
///\param[in,out] model: The current game instance to be modified.
void sphere(Block *center, int radius, int fill, int fx, int fy, int fz, Model *model);

/// Use this function to place a cylinder between two blocks.
///\param[in] b1: The first reference block.
///\param[in] b2: The second reference block.
///\param[in] fill: 1 or 0. Indicates whether the whole block should be filled
///\param[in,out] model: The current game instance to be modified.
void cylinder(Block *b1, Block *b2, int radius, int fill, Model *model);

/// Use this function to place a tree on a block.
///\param[in] block: The block where the tree will be placed.
///\param[in,out] model: The current game instance to be modified.
void tree(Block *block, Model *model);

#endif