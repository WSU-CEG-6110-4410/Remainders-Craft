#ifndef _hit_h_
#define _hit_h_

/// [issue](https://github.com/WSU-CEG-6110-4410/Remainders-Craft/issues/8)
/// [issue](https://github.com/WSU-CEG-6110-4410/Remainders-Craft/issues/20)

/// Use this function get a vector representing a player's
/// sight range
///\param[in] rx: The x coordinate of the player position
///\param[in] ry: The y coordinate of the player position
///\param[in,out] vx: The x coordinate of the sight vector
///\param[in,out] vy: The y coordinate of the sight vector
///\param[in,out] vz: The z coordinate of the sight vector
void get_sight_vector(float rx, float ry, float *vx, float *vy, float *vz);

/// Use this function to see if a hit action (which can also
/// include placing a block) has a result. It also provides
/// the chunk coordinate where an hit action would take place.
///\param[in] map: The map where the hit test is taking place.
///\param[in] max_distance: The maximum distance away where a hit
/// can occur
///\param[in] previous: 1 or 0 to represent whether a hit action
/// should occur on the block closer to the player. This is useful
/// for destroying blocks.
///\param[in] x: The x coordinate of the player position
///\param[in] y: The y coordinate of the player position
///\param[in] z: The z coordinate of the player position
///\param[in] vx: The x coordinate of the sight vector
///\param[in] vy: The y coordinate of the sight vector
///\param[in] vz: The z coordinate of the sight vector
///\param[in,out] hx: The x coordinate chunk which would be hit
///\param[in,out] hy: The y coordinate chunk which would be hit
///\param[in,out] hz: The z coordinate chunk which would be hit
_hit_test(
    Map *map, float max_distance, int previous,
    float x, float y, float z,
    float vx, float vy, float vz,
    int *hx, int *hy, int *hz);

/// Use this function to see if a hit action has a result. It
/// also provides the block coordinate where the hit action would
/// take place.
///\param[in] previous: 1 or 0 to represent whether a hit action
/// should occur on the block closer to the player. This is useful
/// for destroying blocks.
///\param[in] x: The x coordinate of the player position
///\param[in] y: The y coordinate of the player position
///\param[in] z: The z coordinate of the player position
///\param[in] rx: The x value representing the player's sight range
///\param[in] ry: The y value representing the player's sight range
///\param[in,out] bx: The x coordinate block which would be hit
///\param[in,out] by: The y coordinate block which would be hit
///\param[in,out] bz: The z coordinate block which would be hit
///\param[in] model: The current game instance.
///\param[out] int: 0 if no hit action, greater than 0 if there is a
/// hit action
int hit_test(
    int previous, float x, float y, float z, float rx, float ry,
    int *bx, int *by, int *bz, Model *model);

/// Use this function to see if a hit action has a result, what block
/// coordinate the hit action would occur, and on what face of the 3-d block
/// the action would occur.
///\param[in] player: The player who is performing the hit action.
///\param[in,out] x: The x coordinate block which would be hit
///\param[in,out] y: The y coordinate block which would be hit
///\param[in,out] z: The z coordinate block which would be hit
///\param[in,out] face: The face where the action would occur. Can be
/// 0, 1, 2, 3, or 4
///\param[in] model: The current game instance.
int hit_test_face(Player *player, int *x, int *y, int *z, int *face, Model *model);

#endif
