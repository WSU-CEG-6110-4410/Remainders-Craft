#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <curl/curl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "auth.h"
#include "client.h"
#include "config.h"
#include "cube.h"
#include "db.h"
#include "item.h"
#include "map.h"
#include "matrix.h"
#include "noise.h"
#include "sign.h"
#include "tinycthread.h"
#include "util.h"
#include "world.h"
#include "input.h"
#include "chunk.h"
#include "block.h"
#include "hit.h"

#define MAX_CHUNKS 8192
#define MAX_PLAYERS 128
#define WORKERS 4
#define MAX_TEXT_LENGTH 256
#define MAX_NAME_LENGTH 32
#define MAX_PATH_LENGTH 256
#define MAX_ADDR_LENGTH 256

#define ALIGN_LEFT 0
#define ALIGN_CENTER 1
#define ALIGN_RIGHT 2

#define WORKER_IDLE 0
#define WORKER_BUSY 1
#define WORKER_DONE 2

static Model model;
static Model *g = &model;

/**
This function is used to get the in-game time of day. Returns a float that holds the current time.
\return A float value that represents the current time of day.
*/
float time_of_day()
{
    if (g->day_length <= 0)
    {
        return 0.5;
    }
    float t;
    t = glfwGetTime();
    t = t / g->day_length;
    t = t - (int)t;
    return t;
}

/**
This function is used to determine the daylight that needs to be present in the world based on the time of day.
\return A float value for the amount of light that needs to render.
*/
float get_daylight()
{
    float timer = time_of_day();
    if (timer < 0.5)
    {
        float t = (timer - 0.25) * 100;
        return 1 / (1 + powf(2, -t));
    }
    else
    {
        float t = (timer - 0.85) * 100;
        return 1 - 1 / (1 + powf(2, -t));
    }
}

/**
This function determines how much the game needs to scale renders based on the size of the game window.
\return The amount that graphics need to be scaled by.
*/
int get_scale_factor()
{
    int window_width, window_height;
    int buffer_width, buffer_height;
    glfwGetWindowSize(g->window, &window_width, &window_height);
    glfwGetFramebufferSize(g->window, &buffer_width, &buffer_height);
    int result = buffer_width / window_width;
    result = MAX(1, result);
    result = MIN(2, result);
    return result;
}

/**
This function creates the graphic buffer that is used to render the crosshair on the display.
\return The buffer for the corsshair render.
*/
GLuint gen_crosshair_buffer()
{
    int x = g->width / 2;
    int y = g->height / 2;
    int p = 10 * g->scale;
    float data[] = {
        x, y - p, x, y + p,
        x - p, y, x + p, y};
    return gen_buffer(sizeof(data), data);
}

/**
This function creates the graphic buffer that is used to render the wireframes in the game. Wireframs are used by obstacle items.
\param[in] x: The x value to be used for the wireframe.
\param[in] y: The y value to be used for the wireframe.
\param[in] z: The z value to be used for the wireframe.
\param[in] n: Used to calculate the offset of axes.
\return The buffer for the wireframe render.
*/
GLuint gen_wireframe_buffer(float x, float y, float z, float n)
{
    float data[72];
    make_cube_wireframe(data, x, y, z, n);
    return gen_buffer(sizeof(data), data);
}

/**
This function creates the graphic buffer that is used to render the sky.
\return The buffer for the sky render.
*/
GLuint gen_sky_buffer()
{
    float data[12288];
    make_sphere(data, 1, 3);
    return gen_buffer(sizeof(data), data);
}

/**
This function creates the graphic buffer that is used to render all non-plant items.
\param[in] x: The x value to be used for the cube.
\param[in] y: The y value to be used for the cube.
\param[in] z: The z value to be used for the cube.
\param[in] n: Used to calculate the offset of axes.
\param[in] w: Value for the item that the cube will be used for.
\return The buffer for the cube render.
*/
GLuint gen_cube_buffer(float x, float y, float z, float n, int w)
{
    GLfloat *data = malloc_faces(10, 6);
    float ao[6][4] = {0};
    float light[6][4] = {
        {0.5, 0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5, 0.5}};
    make_cube(data, ao, light, 1, 1, 1, 1, 1, 1, x, y, z, n, w);
    return gen_faces(10, 6, data);
}

/**
This function creates the graphic buffer that is used to render all plant items.
\param[in] x: The x value to be used for the plant.
\param[in] y: The y value to be used for the plant.
\param[in] z: The z value to be used for the plant.
\param[in] n: Used to calculate the offset of axes.
\param[in] w: Value for the plant type that the buffer will be used for.
\return The buffer for the plant render.
*/
GLuint gen_plant_buffer(float x, float y, float z, float n, int w)
{
    GLfloat *data = malloc_faces(10, 4);
    float ao = 0;
    float light = 1;
    make_plant(data, ao, light, x, y, z, n, w, 45);
    return gen_faces(10, 4, data);
}

/**
This function creates the graphic buffer that is used to render players.
\param[in] x: The x value to be used for the player.
\param[in] y: The y value to be used for the player.
\param[in] z: The z value to be used for the player.
\param[in] rx: The x rotation of the player.
\param[in] ry: The y rotation of the player.
\return A function call to generate the faces of the player.
*/
GLuint gen_player_buffer(float x, float y, float z, float rx, float ry)
{
    GLfloat *data = malloc_faces(10, 6);
    make_player(data, x, y, z, rx, ry);
    return gen_faces(10, 6, data);
}

/**
This function creates the graphic buffer that is used to render all in-game text.
\param[in] x: The x value to be used for the text.
\param[in] y: The y value to be used for the text.
\param[in] n: Used to calculate the offset of axes.
\param[in] text: The text that will be displayed.
\return A function call to generate the faces of the text.
*/
GLuint gen_text_buffer(float x, float y, float n, char *text)
{
    int length = strlen(text);
    GLfloat *data = malloc_faces(4, length);
    for (int i = 0; i < length; i++)
    {
        make_character(data + i * 24, x, y, n / 2, n, text[i]);
        x += n;
    }
    return gen_faces(4, length, data);
}

/**
Used to draw chunks and items in the game world
\param[in,out] attrib: Attrib struct that contains information on what will be drawn.
\param[in] buffer: Buffer that will be used for the drawing.
\param[in] count: How many need to be drawn.
*/
void draw_triangles_3d_ao(Attrib *attrib, GLuint buffer, int count)
{
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(attrib->position);
    glEnableVertexAttribArray(attrib->normal);
    glEnableVertexAttribArray(attrib->uv);
    glVertexAttribPointer(attrib->position, 3, GL_FLOAT, GL_FALSE,
                          sizeof(GLfloat) * 10, 0);
    glVertexAttribPointer(attrib->normal, 3, GL_FLOAT, GL_FALSE,
                          sizeof(GLfloat) * 10, (GLvoid *)(sizeof(GLfloat) * 3));
    glVertexAttribPointer(attrib->uv, 4, GL_FLOAT, GL_FALSE,
                          sizeof(GLfloat) * 10, (GLvoid *)(sizeof(GLfloat) * 6));
    glDrawArrays(GL_TRIANGLES, 0, count);
    glDisableVertexAttribArray(attrib->position);
    glDisableVertexAttribArray(attrib->normal);
    glDisableVertexAttribArray(attrib->uv);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/**
Used to draw text that displays on signs in the world.
\param[in,out] attrib: Attrib struct that contains information on what will be drawn.
\param[in] buffer: Buffer that will be used for the drawing.
\param[in] count: How many need to be drawn.
*/
void draw_triangles_3d_text(Attrib *attrib, GLuint buffer, int count)
{
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(attrib->position);
    glEnableVertexAttribArray(attrib->uv);
    glVertexAttribPointer(attrib->position, 3, GL_FLOAT, GL_FALSE,
                          sizeof(GLfloat) * 5, 0);
    glVertexAttribPointer(attrib->uv, 2, GL_FLOAT, GL_FALSE,
                          sizeof(GLfloat) * 5, (GLvoid *)(sizeof(GLfloat) * 3));
    glDrawArrays(GL_TRIANGLES, 0, count);
    glDisableVertexAttribArray(attrib->position);
    glDisableVertexAttribArray(attrib->uv);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/**
Used to draw the skybox for the world
\param[in] attrib: Attrib struct that contains information on what will be drawn.
\param[in] buffer: Buffer that will be used for the drawing.
\param[in] count: How many need to be drawn.
*/
void draw_triangles_3d(Attrib *attrib, GLuint buffer, int count)
{
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(attrib->position);
    glEnableVertexAttribArray(attrib->normal);
    glEnableVertexAttribArray(attrib->uv);
    glVertexAttribPointer(attrib->position, 3, GL_FLOAT, GL_FALSE,
                          sizeof(GLfloat) * 8, 0);
    glVertexAttribPointer(attrib->normal, 3, GL_FLOAT, GL_FALSE,
                          sizeof(GLfloat) * 8, (GLvoid *)(sizeof(GLfloat) * 3));
    glVertexAttribPointer(attrib->uv, 2, GL_FLOAT, GL_FALSE,
                          sizeof(GLfloat) * 8, (GLvoid *)(sizeof(GLfloat) * 6));
    glDrawArrays(GL_TRIANGLES, 0, count);
    glDisableVertexAttribArray(attrib->position);
    glDisableVertexAttribArray(attrib->normal);
    glDisableVertexAttribArray(attrib->uv);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/**
Used to draw text that displays in the world. Used for the text that displays at the top of the GUI.
\param[in] attrib: Attrib struct that contains information on what will be drawn.
\param[in] buffer: Buffer that will be used for the drawing.
\param[in] count: How many need to be drawn.
*/
void draw_triangles_2d(Attrib *attrib, GLuint buffer, int count)
{
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(attrib->position);
    glEnableVertexAttribArray(attrib->uv);
    glVertexAttribPointer(attrib->position, 2, GL_FLOAT, GL_FALSE,
                          sizeof(GLfloat) * 4, 0);
    glVertexAttribPointer(attrib->uv, 2, GL_FLOAT, GL_FALSE,
                          sizeof(GLfloat) * 4, (GLvoid *)(sizeof(GLfloat) * 2));
    glDrawArrays(GL_TRIANGLES, 0, count);
    glDisableVertexAttribArray(attrib->position);
    glDisableVertexAttribArray(attrib->uv);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/**
Used to draw lines that are used for the crosshairs and wireframes.
\param[in] attrib: Attrib struct that contains information on what will be drawn.
\param[in] buffer: Buffer that will be used for the drawing.
\param[in] count: How many need to be drawn.
*/
void draw_lines(Attrib *attrib, GLuint buffer, int components, int count)
{
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(attrib->position);
    glVertexAttribPointer(
        attrib->position, components, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_LINES, 0, count);
    glDisableVertexAttribArray(attrib->position);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/**
Draws a world chunk that displays the world.
\param[in] attrib: Attrib struct that contains information on what will be drawn.
\param[in] chunk: Pointer to the specific chunk that will be drawn.
*/
void draw_chunk(Attrib *attrib, Chunk *chunk)
{
    draw_triangles_3d_ao(attrib, chunk->buffer, chunk->faces * 6);
}

/**
Used to draw items that appear in the world.
\param[in] attrib: Attrib struct that contains information on what will be drawn.
\param[in] buffer: Buffer that will be used for the drawing.
\param[in] count: How many need to be drawn.
*/
void draw_item(Attrib *attrib, GLuint buffer, int count)
{
    draw_triangles_3d_ao(attrib, buffer, count);
}

/**
Used to handle drawing text.
\param[in] attrib: Attrib struct that contains information on what will be drawn.
\param[in] buffer: Buffer that will be used for the drawing.
\param[in] length: How long the text is that will be drawn.
*/
void draw_text(Attrib *attrib, GLuint buffer, int length)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    draw_triangles_2d(attrib, buffer, length * 6);
    glDisable(GL_BLEND);
}

/**
Draws signs that will be present in the chunk.
\param[in] attrib: Attrib struct that contains information on what will be drawn.
\param[in] chunk: Pointer to the specific chunk that will be drawn.
*/
void draw_signs(Attrib *attrib, Chunk *chunk)
{
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-8, -1024);
    draw_triangles_3d_text(attrib, chunk->sign_buffer, chunk->sign_faces * 6);
    glDisable(GL_POLYGON_OFFSET_FILL);
}

/**
Used to text that appears on signs in the world.
\param[in] attrib: Attrib struct that contains information on what will be drawn.
\param[in] buffer: Buffer that will be used for the drawing.
\param[in] length: How long the text on the sign is.
*/
void draw_sign(Attrib *attrib, GLuint buffer, int length)
{
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-8, -1024);
    draw_triangles_3d_text(attrib, buffer, length * 6);
    glDisable(GL_POLYGON_OFFSET_FILL);
}

/**
Handles drawing all items that appear in the world.
\param[in] attrib: Attrib struct that contains information on what will be drawn.
\param[in] buffer: Buffer that will be used for the drawing.
*/
void draw_cube(Attrib *attrib, GLuint buffer)
{
    draw_item(attrib, buffer, 36);
}

/**
Handles drawing all plants that appear in the world.
\param[in] attrib: Attrib struct that contains information on what will be drawn.
\param[in] buffer: Buffer that will be used for the drawing.
*/
void draw_plant(Attrib *attrib, GLuint buffer)
{
    draw_item(attrib, buffer, 24);
}

/**
Handles drawing a player that appears in the world.
\param[in] attrib: Attrib struct that contains information on what will be drawn.
\param[in] buffer: Buffer that will be used for the drawing.
*/
void draw_player(Attrib *attrib, Player *player)
{
    draw_cube(attrib, player->buffer);
}

/**
Returns that player information that matches the passed player ID.
\param[in] id: The ID for the player that will be searched for.
\return The player struct for the matching ID.
*/
Player *find_player(int id)
{
    for (int i = 0; i < g->player_count; i++)
    {
        Player *player = g->players + i;
        if (player->id == id)
        {
            return player;
        }
    }
    return 0;
}

/**
Updates the position and status of a player in the world.
\param[in] player: Pointer for the player that will be updated.
\param[in] x: x coord of the player.
\param[in] y: y coord of the player.
\param[in] z: z coord of the player.
\param[in] rx: x rotation of the player.
\param[in] ry: y rotation of the player.
\param[in] interpolate: 1 or 0 depending on whether the player is moving when the update function is called.
*/
void update_player(Player *player,
                   float x, float y, float z, float rx, float ry, int interpolate)
{
    if (interpolate)
    {
        State *s1 = &player->state1;
        State *s2 = &player->state2;
        memcpy(s1, s2, sizeof(State));
        s2->x = x;
        s2->y = y;
        s2->z = z;
        s2->rx = rx;
        s2->ry = ry;
        s2->t = glfwGetTime();
        if (s2->rx - s1->rx > PI)
        {
            s1->rx += 2 * PI;
        }
        if (s1->rx - s2->rx > PI)
        {
            s1->rx -= 2 * PI;
        }
    }
    else
    {
        State *s = &player->state;
        s->x = x;
        s->y = y;
        s->z = z;
        s->rx = rx;
        s->ry = ry;
        del_buffer(player->buffer);
        player->buffer = gen_player_buffer(s->x, s->y, s->z, s->rx, s->ry);
    }
}

/**
Handles how movement is displayed for the player model.
\param[out] player: Pointer for the player that will be updated.
\*/
void interpolate_player(Player *player)
{
    State *s1 = &player->state1;
    State *s2 = &player->state2;
    float t1 = s2->t - s1->t;
    float t2 = glfwGetTime() - s2->t;
    t1 = MIN(t1, 1);
    t1 = MAX(t1, 0.1);
    float p = MIN(t2 / t1, 1);
    update_player(
        player,
        s1->x + (s2->x - s1->x) * p,
        s1->y + (s2->y - s1->y) * p,
        s1->z + (s2->z - s1->z) * p,
        s1->rx + (s2->rx - s1->rx) * p,
        s1->ry + (s2->ry - s1->ry) * p,
        0);
}

/**
Removes the player from the world.
\param[in] id: The ID of the player that is to be deleted.
*/
void delete_player(int id)
{
    Player *player = find_player(id);
    if (!player)
    {
        return;
    }
    int count = g->player_count;
    del_buffer(player->buffer);
    Player *other = g->players + (--count);
    memcpy(player, other, sizeof(Player));
    g->player_count = count;
}

/**
Removes all players from the world.
*/
void delete_all_players()
{
    for (int i = 0; i < g->player_count; i++)
    {
        Player *player = g->players + i;
        del_buffer(player->buffer);
    }
    g->player_count = 0;
}

/**
Calculates the distance between two players
\param[in] p1: Pointer for the first player to be used in the equation.
\param[in] p2: Pointer for the second st player to be used in the equation.
\return The calculated distance between two players.
*/
float player_player_distance(Player *p1, Player *p2)
{
    State *s1 = &p1->state;
    State *s2 = &p2->state;
    float x = s2->x - s1->x;
    float y = s2->y - s1->y;
    float z = s2->z - s1->z;
    return sqrtf(x * x + y * y + z * z);
}

/**
Calculates the distance between where one player's crosshair is and the location of another player's character location.
\param[in] p1: The first player whose crosshairs will be checked.
\param[in] p2: The second player, who is the target.
*/
float player_crosshair_distance(Player *p1, Player *p2)
{
    State *s1 = &p1->state;
    State *s2 = &p2->state;
    float d = player_player_distance(p1, p2);
    float vx, vy, vz;
    get_sight_vector(s1->rx, s1->ry, &vx, &vy, &vz);
    vx *= d;
    vy *= d;
    vz *= d;
    float px, py, pz;
    px = s1->x + vx;
    py = s1->y + vy;
    pz = s1->z + vz;
    float x = s2->x - px;
    float y = s2->y - py;
    float z = s2->z - pz;
    return sqrtf(x * x + y * y + z * z);
}

/**
Handles processes involving the player's crosshair. This function is used to call other functions to check what should happen.
\param[in] player: The player whose crosshair is the subject of the check.
\return The result of the action as a player state.
*/
Player *player_crosshair(Player *player)
{
    Player *result = 0;
    float threshold = RADIANS(5);
    float best = 0;
    for (int i = 0; i < g->player_count; i++)
    {
        Player *other = g->players + i;
        if (other == player)
        {
            continue;
        }
        float p = player_crosshair_distance(player, other);
        float d = player_player_distance(player, other);
        if (d < 96 && p / d < threshold)
        {
            if (best == 0 || d < best)
            {
                best = d;
                result = other;
            }
        }
    }
    return result;
}

/**
Checks to see if a chunk is visible to the player.
\param[in] planes: The planes that the chunk is composed of.
\param[in] p: Used to calculate the size of the x-axis of the chunk.
\param[in] q: Used to calculate the size of the z-axis of the chunk.
\param[in] miny: Smallest y value that exists in the chunk.
\param[in] maxy: LArgest y value that exists in the chunk.
\return Returns 1 if the chunk is visible and 0 if it is not.
*/
int chunk_visible(float planes[6][4], int p, int q, int miny, int maxy)
{
    int x = p * CHUNK_SIZE - 1;
    int z = q * CHUNK_SIZE - 1;
    int d = CHUNK_SIZE + 1;
    float points[8][3] = {
        {x + 0, miny, z + 0},
        {x + d, miny, z + 0},
        {x + 0, miny, z + d},
        {x + d, miny, z + d},
        {x + 0, maxy, z + 0},
        {x + d, maxy, z + 0},
        {x + 0, maxy, z + d},
        {x + d, maxy, z + d}};
    int n = g->ortho ? 4 : 6;
    for (int i = 0; i < n; i++)
    {
        int in = 0;
        int out = 0;
        for (int j = 0; j < 8; j++)
        {
            float d =
                planes[i][0] * points[j][0] +
                planes[i][1] * points[j][1] +
                planes[i][2] * points[j][2] +
                planes[i][3];
            if (d < 0)
            {
                out++;
            }
            else
            {
                in++;
            }
            if (in && out)
            {
                break;
            }
        }
        if (in == 0)
        {
            return 0;
        }
    }
    return 1;
}

/**
Used to create the faces of the text when it is being rendered.
\param[in] data: Memory allocated to hold the faces of what will be rendered.
\param[in] x: x value of the sign that will be rendered.
\param[in] y: y value of the sign that will be rendered.
\param[in] z: z value of the sign that will be rendered.
\param[in] face: Number of faces the sign has.
\param[in] text: The text that the sign displays.
\return The length of the sign. (Unsure)
*/
int _gen_sign_buffer(
    GLfloat *data, float x, float y, float z, int face, const char *text)
{
    static const int glyph_dx[8] = {0, 0, -1, 1, 1, 0, -1, 0};
    static const int glyph_dz[8] = {1, -1, 0, 0, 0, -1, 0, 1};
    static const int line_dx[8] = {0, 0, 0, 0, 0, 1, 0, -1};
    static const int line_dy[8] = {-1, -1, -1, -1, 0, 0, 0, 0};
    static const int line_dz[8] = {0, 0, 0, 0, 1, 0, -1, 0};
    if (face < 0 || face >= 8)
    {
        return 0;
    }
    int count = 0;
    float max_width = 64;
    float line_height = 1.25;
    char lines[1024];
    int rows = wrap(text, max_width, lines, 1024);
    rows = MIN(rows, 5);
    int dx = glyph_dx[face];
    int dz = glyph_dz[face];
    int ldx = line_dx[face];
    int ldy = line_dy[face];
    int ldz = line_dz[face];
    float n = 1.0 / (max_width / 10);
    float sx = x - n * (rows - 1) * (line_height / 2) * ldx;
    float sy = y - n * (rows - 1) * (line_height / 2) * ldy;
    float sz = z - n * (rows - 1) * (line_height / 2) * ldz;
    char *key;
    char *line = tokenize(lines, "\n", &key);
    while (line)
    {
        int length = strlen(line);
        int line_width = string_width(line);
        line_width = MIN(line_width, max_width);
        float rx = sx - dx * line_width / max_width / 2;
        float ry = sy;
        float rz = sz - dz * line_width / max_width / 2;
        for (int i = 0; i < length; i++)
        {
            int width = char_width(line[i]);
            line_width -= width;
            if (line_width < 0)
            {
                break;
            }
            rx += dx * width / max_width / 2;
            rz += dz * width / max_width / 2;
            if (line[i] != ' ')
            {
                make_character_3d(
                    data + count * 30, rx, ry, rz, n / 2, face, line[i]);
                count++;
            }
            rx += dx * width / max_width / 2;
            rz += dz * width / max_width / 2;
        }
        sx += n * line_height * ldx;
        sy += n * line_height * ldy;
        sz += n * line_height * ldz;
        line = tokenize(NULL, "\n", &key);
        rows--;
        if (rows <= 0)
        {
            break;
        }
    }
    return count;
}

/**
Creates the buffer that will be used for a sign in a chunk.
\param[in,out] chunk: The chunk that contains the sign.
*/
void gen_sign_buffer(Chunk *chunk)
{
    SignList *signs = &chunk->signs;

    // first pass - count characters
    int max_faces = 0;
    for (int i = 0; i < signs->size; i++)
    {
        Sign *e = signs->data + i;
        max_faces += strlen(e->text);
    }

    // second pass - generate geometry
    GLfloat *data = malloc_faces(5, max_faces);
    int faces = 0;
    for (int i = 0; i < signs->size; i++)
    {
        Sign *e = signs->data + i;
        faces += _gen_sign_buffer(
            data + faces * 30, e->x, e->y, e->z, e->face, e->text);
    }

    del_buffer(chunk->sign_buffer);
    chunk->sign_buffer = gen_faces(5, faces, data);
    chunk->sign_faces = faces;
}

/**
Checks for occlusion in the world. This allows for blocks to be hidden behind other blocks. (Unsure about the params)
\param[in] neighbors: Neighboring items.
\param[in] lights: All lights that will be checked for occlusion.
\param[in] shades: Holds the information that will be used for determining how shade is displayed.
\param[in] ao: Used for ambient occlusion.
\param[in] light: Holds the value for the light after checking for occlusion.
*/
void occlusion(
    char neighbors[27], char lights[27], float shades[27],
    float ao[6][4], float light[6][4])
{
    static const int lookup3[6][4][3] = {
        {{0, 1, 3}, {2, 1, 5}, {6, 3, 7}, {8, 5, 7}},
        {{18, 19, 21}, {20, 19, 23}, {24, 21, 25}, {26, 23, 25}},
        {{6, 7, 15}, {8, 7, 17}, {24, 15, 25}, {26, 17, 25}},
        {{0, 1, 9}, {2, 1, 11}, {18, 9, 19}, {20, 11, 19}},
        {{0, 3, 9}, {6, 3, 15}, {18, 9, 21}, {24, 15, 21}},
        {{2, 5, 11}, {8, 5, 17}, {20, 11, 23}, {26, 17, 23}}};
    static const int lookup4[6][4][4] = {
        {{0, 1, 3, 4}, {1, 2, 4, 5}, {3, 4, 6, 7}, {4, 5, 7, 8}},
        {{18, 19, 21, 22}, {19, 20, 22, 23}, {21, 22, 24, 25}, {22, 23, 25, 26}},
        {{6, 7, 15, 16}, {7, 8, 16, 17}, {15, 16, 24, 25}, {16, 17, 25, 26}},
        {{0, 1, 9, 10}, {1, 2, 10, 11}, {9, 10, 18, 19}, {10, 11, 19, 20}},
        {{0, 3, 9, 12}, {3, 6, 12, 15}, {9, 12, 18, 21}, {12, 15, 21, 24}},
        {{2, 5, 11, 14}, {5, 8, 14, 17}, {11, 14, 20, 23}, {14, 17, 23, 26}}};
    static const float curve[4] = {0.0, 0.25, 0.5, 0.75};
    for (int i = 0; i < 6; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            int corner = neighbors[lookup3[i][j][0]];
            int side1 = neighbors[lookup3[i][j][1]];
            int side2 = neighbors[lookup3[i][j][2]];
            int value = side1 && side2 ? 3 : corner + side1 + side2;
            float shade_sum = 0;
            float light_sum = 0;
            int is_light = lights[13] == 15;
            for (int k = 0; k < 4; k++)
            {
                shade_sum += shades[lookup4[i][j][k]];
                light_sum += lights[lookup4[i][j][k]];
            }
            if (is_light)
            {
                light_sum = 15 * 4 * 10;
            }
            float total = curve[value] + shade_sum / 4.0;
            ao[i][j] = MIN(total, 1.0);
            light[i][j] = light_sum / 15.0 / 4.0;
        }
    }
}

#define XZ_SIZE (CHUNK_SIZE * 3 + 2)
#define XZ_LO (CHUNK_SIZE)
#define XZ_HI (CHUNK_SIZE * 2 + 1)
#define Y_SIZE 258
#define XYZ(x, y, z) ((y)*XZ_SIZE * XZ_SIZE + (x)*XZ_SIZE + (z))
#define XZ(x, z) ((x)*XZ_SIZE + (z))

/**
Generates light for the world
\param[in] opaque: Used to determine if an object is opaque or not.
\param[in] light: Array for the light values in the world.
\param[in] x: x for determining light generation location.
\param[in] y: y for determining light generation location.
\param[in] z: z for determining light generation location.
\param[in] w: Scale for x, y, z
\param[in] force: Determines if light generation should be forced or not.
*/
void light_fill(
    char *opaque, char *light,
    int x, int y, int z, int w, int force)
{
    if (x + w < XZ_LO || z + w < XZ_LO)
    {
        return;
    }
    if (x - w > XZ_HI || z - w > XZ_HI)
    {
        return;
    }
    if (y < 0 || y >= Y_SIZE)
    {
        return;
    }
    if (light[XYZ(x, y, z)] >= w)
    {
        return;
    }
    if (!force && opaque[XYZ(x, y, z)])
    {
        return;
    }
    light[XYZ(x, y, z)] = w--;
    light_fill(opaque, light, x - 1, y, z, w, 0);
    light_fill(opaque, light, x + 1, y, z, w, 0);
    light_fill(opaque, light, x, y - 1, z, w, 0);
    light_fill(opaque, light, x, y + 1, z, w, 0);
    light_fill(opaque, light, x, y, z - 1, w, 0);
    light_fill(opaque, light, x, y, z + 1, w, 0);
}

/**
Handles all the calculations for the generation of a chunk. Generates all of the data that goes into a chunk.
\param[in] item: Struct that contains the data that will be used to calculate the data for the chunk.
*/
void compute_chunk(WorkerItem *item)
{
    char *opaque = (char *)calloc(XZ_SIZE * XZ_SIZE * Y_SIZE, sizeof(char));
    char *light = (char *)calloc(XZ_SIZE * XZ_SIZE * Y_SIZE, sizeof(char));
    char *highest = (char *)calloc(XZ_SIZE * XZ_SIZE, sizeof(char));

    int ox = item->p * CHUNK_SIZE - CHUNK_SIZE - 1;
    int oy = -1;
    int oz = item->q * CHUNK_SIZE - CHUNK_SIZE - 1;

    // check for lights
    int has_light = 0;
    if (SHOW_LIGHTS)
    {
        for (int a = 0; a < 3; a++)
        {
            for (int b = 0; b < 3; b++)
            {
                Map *map = item->light_maps[a][b];
                if (map && map->size)
                {
                    has_light = 1;
                }
            }
        }
    }

    // populate opaque array
    for (int a = 0; a < 3; a++)
    {
        for (int b = 0; b < 3; b++)
        {
            Map *map = item->block_maps[a][b];
            if (!map)
            {
                continue;
            }
            MAP_FOR_EACH(map, ex, ey, ez, ew)
            {
                int x = ex - ox;
                int y = ey - oy;
                int z = ez - oz;
                int w = ew;
                // TODO: this should be unnecessary
                if (x < 0 || y < 0 || z < 0)
                {
                    continue;
                }
                if (x >= XZ_SIZE || y >= Y_SIZE || z >= XZ_SIZE)
                {
                    continue;
                }
                // END TODO
                opaque[XYZ(x, y, z)] = !is_transparent(w);
                if (opaque[XYZ(x, y, z)])
                {
                    highest[XZ(x, z)] = MAX(highest[XZ(x, z)], y);
                }
            }
            END_MAP_FOR_EACH;
        }
    }

    // flood fill light intensities
    if (has_light)
    {
        for (int a = 0; a < 3; a++)
        {
            for (int b = 0; b < 3; b++)
            {
                Map *map = item->light_maps[a][b];
                if (!map)
                {
                    continue;
                }
                MAP_FOR_EACH(map, ex, ey, ez, ew)
                {
                    int x = ex - ox;
                    int y = ey - oy;
                    int z = ez - oz;
                    light_fill(opaque, light, x, y, z, ew, 1);
                }
                END_MAP_FOR_EACH;
            }
        }
    }

    Map *map = item->block_maps[1][1];

    // count exposed faces
    int miny = 256;
    int maxy = 0;
    int faces = 0;
    MAP_FOR_EACH(map, ex, ey, ez, ew)
    {
        if (ew <= 0)
        {
            continue;
        }
        int x = ex - ox;
        int y = ey - oy;
        int z = ez - oz;
        int f1 = !opaque[XYZ(x - 1, y, z)];
        int f2 = !opaque[XYZ(x + 1, y, z)];
        int f3 = !opaque[XYZ(x, y + 1, z)];
        int f4 = !opaque[XYZ(x, y - 1, z)] && (ey > 0);
        int f5 = !opaque[XYZ(x, y, z - 1)];
        int f6 = !opaque[XYZ(x, y, z + 1)];
        int total = f1 + f2 + f3 + f4 + f5 + f6;
        if (total == 0)
        {
            continue;
        }
        if (is_plant(ew))
        {
            total = 4;
        }
        miny = MIN(miny, ey);
        maxy = MAX(maxy, ey);
        faces += total;
    }
    END_MAP_FOR_EACH;

    // generate geometry
    GLfloat *data = malloc_faces(10, faces);
    int offset = 0;
    MAP_FOR_EACH(map, ex, ey, ez, ew)
    {
        if (ew <= 0)
        {
            continue;
        }
        int x = ex - ox;
        int y = ey - oy;
        int z = ez - oz;
        int f1 = !opaque[XYZ(x - 1, y, z)];
        int f2 = !opaque[XYZ(x + 1, y, z)];
        int f3 = !opaque[XYZ(x, y + 1, z)];
        int f4 = !opaque[XYZ(x, y - 1, z)] && (ey > 0);
        int f5 = !opaque[XYZ(x, y, z - 1)];
        int f6 = !opaque[XYZ(x, y, z + 1)];
        int total = f1 + f2 + f3 + f4 + f5 + f6;
        if (total == 0)
        {
            continue;
        }
        char neighbors[27] = {0};
        char lights[27] = {0};
        float shades[27] = {0};
        int index = 0;
        for (int dx = -1; dx <= 1; dx++)
        {
            for (int dy = -1; dy <= 1; dy++)
            {
                for (int dz = -1; dz <= 1; dz++)
                {
                    neighbors[index] = opaque[XYZ(x + dx, y + dy, z + dz)];
                    lights[index] = light[XYZ(x + dx, y + dy, z + dz)];
                    shades[index] = 0;
                    if (y + dy <= highest[XZ(x + dx, z + dz)])
                    {
                        for (int oy = 0; oy < 8; oy++)
                        {
                            if (opaque[XYZ(x + dx, y + dy + oy, z + dz)])
                            {
                                shades[index] = 1.0 - oy * 0.125;
                                break;
                            }
                        }
                    }
                    index++;
                }
            }
        }
        float ao[6][4];
        float light[6][4];
        occlusion(neighbors, lights, shades, ao, light);
        if (is_plant(ew))
        {
            total = 4;
            float min_ao = 1;
            float max_light = 0;
            for (int a = 0; a < 6; a++)
            {
                for (int b = 0; b < 4; b++)
                {
                    min_ao = MIN(min_ao, ao[a][b]);
                    max_light = MAX(max_light, light[a][b]);
                }
            }
            float rotation = simplex2(ex, ez, 4, 0.5, 2) * 360;
            make_plant(
                data + offset, min_ao, max_light,
                ex, ey, ez, 0.5, ew, rotation);
        }
        else
        {
            make_cube(
                data + offset, ao, light,
                f1, f2, f3, f4, f5, f6,
                ex, ey, ez, 0.5, ew);
        }
        offset += total * 60;
    }
    END_MAP_FOR_EACH;

    free(opaque);
    free(light);
    free(highest);

    item->miny = miny;
    item->maxy = maxy;
    item->faces = faces;
    item->data = data;
}

/**
Generates the chunk to prepare it to be rendered.
\param[in,out] chunk: The pointer for the chunk that will be generated.
\param[in] item: Struct that contains the data that will be used to calculate the data for the chunk.
*/
void generate_chunk(Chunk *chunk, WorkerItem *item)
{
    chunk->miny = item->miny;
    chunk->maxy = item->maxy;
    chunk->faces = item->faces;
    del_buffer(chunk->buffer);
    chunk->buffer = gen_faces(10, item->faces, item->data);
    gen_sign_buffer(chunk);
}

/**
Creates the buffer that is used for generating a chunk.
\param[out] chunk: The chunk that the buffer will be for.
*/
void gen_chunk_buffer(Chunk *chunk)
{
    WorkerItem _item;
    WorkerItem *item = &_item;
    item->p = chunk->p;
    item->q = chunk->q;
    for (int dp = -1; dp <= 1; dp++)
    {
        for (int dq = -1; dq <= 1; dq++)
        {
            Chunk *other = chunk;
            if (dp || dq)
            {
                other = find_chunk(chunk->p + dp, chunk->q + dq, g);
            }
            if (other)
            {
                item->block_maps[dp + 1][dq + 1] = &other->map;
                item->light_maps[dp + 1][dq + 1] = &other->lights;
            }
            else
            {
                item->block_maps[dp + 1][dq + 1] = 0;
                item->light_maps[dp + 1][dq + 1] = 0;
            }
        }
    }
    compute_chunk(item);
    generate_chunk(chunk, item);
    chunk->dirty = 0;
}

/**
Used in part of the create_world function. Passed in to be used for map creation.
\param[in] x: Value used for creating the x axis for the map.
\param[in] y: Value used for creating the y axis for the map.
\param[in] z: Value used for creating the z axis for the map.
\param[in] w: Value used to scale the axes for the map.
\param[in] arg: This is used for passing a map into the fucntion.
*/
void map_set_func(int x, int y, int z, int w, void *arg)
{
    Map *map = (Map *)arg;
    map_set(map, x, y, z, w);
}

/**
Loads a chunk into the world and prepares it to be rendered.
\param[in] item: Struct that contains the data that will be used to calculate the data for the chunk.
*/
void load_chunk(WorkerItem *item)
{
    int p = item->p;
    int q = item->q;
    Map *block_map = item->block_maps[1][1];
    Map *light_map = item->light_maps[1][1];
    create_world(p, q, map_set_func, block_map);
    db_load_blocks(block_map, p, q);
    db_load_lights(light_map, p, q);
}

/**
Requests chunk information from the client
\param[in] p: Part of the set to find the referenced chunk.
\param[in] q: Part of the set to find the referenced chunk.
*/
void request_chunk(int p, int q)
{
    int key = db_get_key(p, q);
    client_chunk(p, q, key);
}

/**
Initializes a a chunk with all the data it needs.
\param[out] chunk: The pointer for the chunk that will be initialized.
\param[in] p: Part of the set to identify the chunk.
\param[in] q: Part of the set to identify the chunk.
*/
void init_chunk(Chunk *chunk, int p, int q)
{
    chunk->p = p;
    chunk->q = q;
    chunk->faces = 0;
    chunk->sign_faces = 0;
    chunk->buffer = 0;
    chunk->sign_buffer = 0;
    dirty_chunk(chunk, g);
    SignList *signs = &chunk->signs;
    sign_list_alloc(signs, 16);
    db_load_signs(signs, p, q);
    Map *block_map = &chunk->map;
    Map *light_map = &chunk->lights;
    int dx = p * CHUNK_SIZE - 1;
    int dy = 0;
    int dz = q * CHUNK_SIZE - 1;
    map_alloc(block_map, dx, dy, dz, 0x7fff);
    map_alloc(light_map, dx, dy, dz, 0xf);
}

/**
Calls the chunk initialization and then uses that data to create the chunk.
\param[out] chunk: The pointer for the chunk that will be created.
\param[in] p: Part of the set to identify the chunk.
\param[in] q: Part of the set to identify the chunk.
*/
void create_chunk(Chunk *chunk, int p, int q)
{
    init_chunk(chunk, p, q);

    WorkerItem _item;
    WorkerItem *item = &_item;
    item->p = chunk->p;
    item->q = chunk->q;
    item->block_maps[1][1] = &chunk->map;
    item->light_maps[1][1] = &chunk->lights;
    load_chunk(item);

    request_chunk(p, q);
}

/**
Deletes chunks that are marked for deletion.
*/
void delete_chunks()
{
    int count = g->chunk_count;
    State *s1 = &g->players->state;
    State *s2 = &(g->players + g->observe1)->state;
    State *s3 = &(g->players + g->observe2)->state;
    State *states[3] = {s1, s2, s3};
    for (int i = 0; i < count; i++)
    {
        Chunk *chunk = g->chunks + i;
        int delete = 1;
        for (int j = 0; j < 3; j++)
        {
            State *s = states[j];
            int p = chunked(s->x);
            int q = chunked(s->z);
            if (chunk_distance(chunk, p, q) < g->delete_radius)
            {
                delete = 0;
                break;
            }
        }
        if (delete)
        {
            map_free(&chunk->map);
            map_free(&chunk->lights);
            sign_list_free(&chunk->signs);
            del_buffer(chunk->buffer);
            del_buffer(chunk->sign_buffer);
            Chunk *other = g->chunks + (--count);
            memcpy(chunk, other, sizeof(Chunk));
        }
    }
    g->chunk_count = count;
}

/**
Deletes all chunks in the game world.
*/
void delete_all_chunks()
{
    for (int i = 0; i < g->chunk_count; i++)
    {
        Chunk *chunk = g->chunks + i;
        map_free(&chunk->map);
        map_free(&chunk->lights);
        sign_list_free(&chunk->signs);
        del_buffer(chunk->buffer);
        del_buffer(chunk->sign_buffer);
    }
    g->chunk_count = 0;
}

/**
Checks if worker objects have finished their tasks and sets them to idle if they have.
*/
void check_workers()
{
    for (int i = 0; i < WORKERS; i++)
    {
        Worker *worker = g->workers + i;
        mtx_lock(&worker->mtx);
        if (worker->state == WORKER_DONE)
        {
            WorkerItem *item = &worker->item;
            Chunk *chunk = find_chunk(item->p, item->q, g);
            if (chunk)
            {
                if (item->load)
                {
                    Map *block_map = item->block_maps[1][1];
                    Map *light_map = item->light_maps[1][1];
                    map_free(&chunk->map);
                    map_free(&chunk->lights);
                    map_copy(&chunk->map, block_map);
                    map_copy(&chunk->lights, light_map);
                    request_chunk(item->p, item->q);
                }
                generate_chunk(chunk, item);
            }
            for (int a = 0; a < 3; a++)
            {
                for (int b = 0; b < 3; b++)
                {
                    Map *block_map = item->block_maps[a][b];
                    Map *light_map = item->light_maps[a][b];
                    if (block_map)
                    {
                        map_free(block_map);
                        free(block_map);
                    }
                    if (light_map)
                    {
                        map_free(light_map);
                        free(light_map);
                    }
                }
            }
            worker->state = WORKER_IDLE;
        }
        mtx_unlock(&worker->mtx);
    }
}

/**
Forces a chunk to be generated.
*/
void force_chunks(Player *player)
{
    State *s = &player->state;
    int p = chunked(s->x);
    int q = chunked(s->z);
    int r = 1;
    for (int dp = -r; dp <= r; dp++)
    {
        for (int dq = -r; dq <= r; dq++)
        {
            int a = p + dp;
            int b = q + dq;
            Chunk *chunk = find_chunk(a, b, g);
            if (chunk)
            {
                if (chunk->dirty)
                {
                    gen_chunk_buffer(chunk);
                }
            }
            else if (g->chunk_count < MAX_CHUNKS)
            {
                chunk = g->chunks + g->chunk_count++;
                create_chunk(chunk, a, b);
                gen_chunk_buffer(chunk);
            }
        }
    }
}

/**
Checks that the chunks have been created correctly.
\param[in] player: The player that caused the chunk to spawn.
\param[in] worker: The worker in charge of the chunk.
*/
void ensure_chunks_worker(Player *player, Worker *worker)
{
    State *s = &player->state;
    float matrix[16];
    set_matrix_3d(
        matrix, g->width, g->height,
        s->x, s->y, s->z, s->rx, s->ry, g->fov, g->ortho, g->render_radius);
    float planes[6][4];
    frustum_planes(planes, g->render_radius, matrix);
    int p = chunked(s->x);
    int q = chunked(s->z);
    int r = g->create_radius;
    int start = 0x0fffffff;
    int best_score = start;
    int best_a = 0;
    int best_b = 0;
    for (int dp = -r; dp <= r; dp++)
    {
        for (int dq = -r; dq <= r; dq++)
        {
            int a = p + dp;
            int b = q + dq;
            int index = (ABS(a) ^ ABS(b)) % WORKERS;
            if (index != worker->index)
            {
                continue;
            }
            Chunk *chunk = find_chunk(a, b, g);
            if (chunk && !chunk->dirty)
            {
                continue;
            }
            int distance = MAX(ABS(dp), ABS(dq));
            int invisible = !chunk_visible(planes, a, b, 0, 256);
            int priority = 0;
            if (chunk)
            {
                priority = chunk->buffer && chunk->dirty;
            }
            int score = (invisible << 24) | (priority << 16) | distance;
            if (score < best_score)
            {
                best_score = score;
                best_a = a;
                best_b = b;
            }
        }
    }
    if (best_score == start)
    {
        return;
    }
    int a = best_a;
    int b = best_b;
    int load = 0;
    Chunk *chunk = find_chunk(a, b, g);
    if (!chunk)
    {
        load = 1;
        if (g->chunk_count < MAX_CHUNKS)
        {
            chunk = g->chunks + g->chunk_count++;
            init_chunk(chunk, a, b);
        }
        else
        {
            return;
        }
    }
    WorkerItem *item = &worker->item;
    item->p = chunk->p;
    item->q = chunk->q;
    item->load = load;
    for (int dp = -1; dp <= 1; dp++)
    {
        for (int dq = -1; dq <= 1; dq++)
        {
            Chunk *other = chunk;
            if (dp || dq)
            {
                other = find_chunk(chunk->p + dp, chunk->q + dq, g);
            }
            if (other)
            {
                Map *block_map = malloc(sizeof(Map));
                map_copy(block_map, &other->map);
                Map *light_map = malloc(sizeof(Map));
                map_copy(light_map, &other->lights);
                item->block_maps[dp + 1][dq + 1] = block_map;
                item->light_maps[dp + 1][dq + 1] = light_map;
            }
            else
            {
                item->block_maps[dp + 1][dq + 1] = 0;
                item->light_maps[dp + 1][dq + 1] = 0;
            }
        }
    }
    chunk->dirty = 0;
    worker->state = WORKER_BUSY;
    cnd_signal(&worker->cnd);
}

/**
Creates the worker for ensure_chunks and calls it to prepare for the creation of a chunk
\param[in] player: 
*/
void ensure_chunks(Player *player)
{
    check_workers();
    force_chunks(player);
    for (int i = 0; i < WORKERS; i++)
    {
        Worker *worker = g->workers + i;
        mtx_lock(&worker->mtx);
        if (worker->state == WORKER_IDLE)
        {
            ensure_chunks_worker(player, worker);
        }
        mtx_unlock(&worker->mtx);
    }
}

/**
Starts up a worker on a new task.
\param[in] arg: What the worker will run.
*/
int worker_run(void *arg)
{
    Worker *worker = (Worker *)arg;
    int running = 1;
    while (running)
    {
        mtx_lock(&worker->mtx);
        while (worker->state != WORKER_BUSY)
        {
            cnd_wait(&worker->cnd, &worker->mtx);
        }
        mtx_unlock(&worker->mtx);
        WorkerItem *item = &worker->item;
        if (item->load)
        {
            load_chunk(item);
        }
        compute_chunk(item);
        mtx_lock(&worker->mtx);
        worker->state = WORKER_DONE;
        mtx_unlock(&worker->mtx);
    }
    return 0;
}

/**
Renders chunks to be displayed in the world.
\param[in] attrib: Attrib struct that contains data for all the shaders that will be used.
\param[in] player: Pointer to the player that caused the chunk to render.
*/
int render_chunks(Attrib *attrib, Player *player)
{
    int result = 0;
    State *s = &player->state;
    ensure_chunks(player);
    int p = chunked(s->x);
    int q = chunked(s->z);
    float light = get_daylight();
    float matrix[16];
    set_matrix_3d(
        matrix, g->width, g->height,
        s->x, s->y, s->z, s->rx, s->ry, g->fov, g->ortho, g->render_radius);
    float planes[6][4];
    frustum_planes(planes, g->render_radius, matrix);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform3f(attrib->camera, s->x, s->y, s->z);
    glUniform1i(attrib->sampler, 0);
    glUniform1i(attrib->extra1, 2);
    glUniform1f(attrib->extra2, light);
    glUniform1f(attrib->extra3, g->render_radius * CHUNK_SIZE);
    glUniform1i(attrib->extra4, g->ortho);
    glUniform1f(attrib->timer, time_of_day());
    for (int i = 0; i < g->chunk_count; i++)
    {
        Chunk *chunk = g->chunks + i;
        if (chunk_distance(chunk, p, q) > g->render_radius)
        {
            continue;
        }
        if (!chunk_visible(
                planes, chunk->p, chunk->q, chunk->miny, chunk->maxy))
        {
            continue;
        }
        draw_chunk(attrib, chunk);
        result += chunk->faces;
    }
    return result;
}

/**
Renders signs that appear in the world.
\param[in] attrib: Attrib struct that contains data for all the shaders that will be used.
\param[in] player: Pointer to the player that caused the signs to render.
*/
void render_signs(Attrib *attrib, Player *player)
{
    State *s = &player->state;
    int p = chunked(s->x);
    int q = chunked(s->z);
    float matrix[16];
    set_matrix_3d(
        matrix, g->width, g->height,
        s->x, s->y, s->z, s->rx, s->ry, g->fov, g->ortho, g->render_radius);
    float planes[6][4];
    frustum_planes(planes, g->render_radius, matrix);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform1i(attrib->sampler, 3);
    glUniform1i(attrib->extra1, 1);
    for (int i = 0; i < g->chunk_count; i++)
    {
        Chunk *chunk = g->chunks + i;
        if (chunk_distance(chunk, p, q) > g->sign_radius)
        {
            continue;
        }
        if (!chunk_visible(
                planes, chunk->p, chunk->q, chunk->miny, chunk->maxy))
        {
            continue;
        }
        draw_signs(attrib, chunk);
    }
}

/**
Renders sign text to be displayed in the world.
\param[in] attrib: Attrib struct that contains data for all the shaders that will be used.
\param[in] player: Pointer to the player that caused the sign text to render.
*/
void render_sign(Attrib *attrib, Player *player)
{
    if (!g->typing || g->typing_buffer[0] != CRAFT_KEY_SIGN)
    {
        return;
    }
    int x, y, z, face;
    if (!hit_test_face(player, &x, &y, &z, &face, g))
    {
        return;
    }
    State *s = &player->state;
    float matrix[16];
    set_matrix_3d(
        matrix, g->width, g->height,
        s->x, s->y, s->z, s->rx, s->ry, g->fov, g->ortho, g->render_radius);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform1i(attrib->sampler, 3);
    glUniform1i(attrib->extra1, 1);
    char text[MAX_SIGN_LENGTH];
    strncpy(text, g->typing_buffer + 1, MAX_SIGN_LENGTH);
    text[MAX_SIGN_LENGTH - 1] = '\0';
    GLfloat *data = malloc_faces(5, strlen(text));
    int length = _gen_sign_buffer(data, x, y, z, face, text);
    GLuint buffer = gen_faces(5, length, data);
    draw_sign(attrib, buffer, length);
    del_buffer(buffer);
}

/**
Renders the players in the world.
\param[in] attrib: Attrib struct that contains data for all the shaders that will be used.
\param[in] player: Pointer to the player that is being rendered.
*/
void render_players(Attrib *attrib, Player *player)
{
    State *s = &player->state;
    float matrix[16];
    set_matrix_3d(
        matrix, g->width, g->height,
        s->x, s->y, s->z, s->rx, s->ry, g->fov, g->ortho, g->render_radius);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform3f(attrib->camera, s->x, s->y, s->z);
    glUniform1i(attrib->sampler, 0);
    glUniform1f(attrib->timer, time_of_day());
    for (int i = 0; i < g->player_count; i++)
    {
        Player *other = g->players + i;
        if (other != player)
        {
            draw_player(attrib, other);
        }
    }
}

/**
Renders the sky box.
\param[in] attrib: Attrib struct that contains data for all the shaders that will be used.
\param[in] player: The player that caused the render action.
\param[in] buffer: The sky buffer that will be used for the render.
*/
void render_sky(Attrib *attrib, Player *player, GLuint buffer)
{
    State *s = &player->state;
    float matrix[16];
    set_matrix_3d(
        matrix, g->width, g->height,
        0, 0, 0, s->rx, s->ry, g->fov, 0, g->render_radius);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform1i(attrib->sampler, 2);
    glUniform1f(attrib->timer, time_of_day());
    draw_triangles_3d(attrib, buffer, 512 * 3);
}

/**
Renders wireframes.
\param[in] attrib: Attrib struct that contains data for all the shaders that will be used.
\param[in] player: The player that caused the render action.
*/
void render_wireframe(Attrib *attrib, Player *player)
{
    State *s = &player->state;
    float matrix[16];
    set_matrix_3d(
        matrix, g->width, g->height,
        s->x, s->y, s->z, s->rx, s->ry, g->fov, g->ortho, g->render_radius);
    int hx, hy, hz;
    int hw = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz, g);
    if (is_obstacle(hw))
    {
        glUseProgram(attrib->program);
        glLineWidth(1);
        glEnable(GL_COLOR_LOGIC_OP);
        glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
        GLuint wireframe_buffer = gen_wireframe_buffer(hx, hy, hz, 0.53);
        draw_lines(attrib, wireframe_buffer, 3, 24);
        del_buffer(wireframe_buffer);
        glDisable(GL_COLOR_LOGIC_OP);
    }
}

/**
Renders the crosshair that appears on the player HUD.
\param[in] attrib: Attrib struct that contains data for all the shaders that will be used.
*/
void render_crosshairs(Attrib *attrib)
{
    float matrix[16];
    set_matrix_2d(matrix, g->width, g->height);
    glUseProgram(attrib->program);
    glLineWidth(4 * g->scale);
    glEnable(GL_COLOR_LOGIC_OP);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    GLuint crosshair_buffer = gen_crosshair_buffer();
    draw_lines(attrib, crosshair_buffer, 2, 4);
    del_buffer(crosshair_buffer);
    glDisable(GL_COLOR_LOGIC_OP);
}

/**
Renders items in the world.
\param[in] attrib: Attrib struct that contains data for all the shaders that will be used.
*/
void render_item(Attrib *attrib)
{
    float matrix[16];
    set_matrix_item(matrix, g->width, g->height, g->scale);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform3f(attrib->camera, 0, 0, 5);
    glUniform1i(attrib->sampler, 0);
    glUniform1f(attrib->timer, time_of_day());
    int w = items[g->item_index];
    if (is_plant(w))
    {
        GLuint buffer = gen_plant_buffer(0, 0, 0, 0.5, w);
        draw_plant(attrib, buffer);
        del_buffer(buffer);
    }
    else
    {
        GLuint buffer = gen_cube_buffer(0, 0, 0, 0.5, w);
        draw_cube(attrib, buffer);
        del_buffer(buffer);
    }
}

/**
Renders text that displays on the player HUD.
\param[in] attrib: Attrib struct that contains data for all the shaders that will be used.
\param[in] justify: How the text will be justified where it is drawn.
\param[in] x: The x location of where the text will start drawing.
\param[in] y: The y location of where the text will start drawing.
\param[in] n: Size of the text.
\param[in] text: Text that will be rendered.
*/
void render_text(
    Attrib *attrib, int justify, float x, float y, float n, char *text)
{
    float matrix[16];
    set_matrix_2d(matrix, g->width, g->height);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform1i(attrib->sampler, 1);
    glUniform1i(attrib->extra1, 0);
    int length = strlen(text);
    x -= n * justify * (length - 1) / 2;
    GLuint buffer = gen_text_buffer(x, y, n, text);
    draw_text(attrib, buffer, length);
    del_buffer(buffer);
}

/**
Creates the display window for the game.
*/
void create_window()
{
    int window_width = WINDOW_WIDTH;
    int window_height = WINDOW_HEIGHT;
    GLFWmonitor *monitor = NULL;
    if (FULLSCREEN)
    {
        int mode_count;
        monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode *modes = glfwGetVideoModes(monitor, &mode_count);
        window_width = modes[mode_count - 1].width;
        window_height = modes[mode_count - 1].height;
    }
    g->window = glfwCreateWindow(
        window_width, window_height, "Craft", monitor, NULL);
}

/**
Resets the opengl model for the game.
*/
void reset_model()
{
    memset(g->chunks, 0, sizeof(Chunk) * MAX_CHUNKS);
    g->chunk_count = 0;
    memset(g->players, 0, sizeof(Player) * MAX_PLAYERS);
    g->player_count = 0;
    g->observe1 = 0;
    g->observe2 = 0;
    g->flying = 0;
    g->item_index = 0;
    memset(g->typing_buffer, 0, sizeof(char) * MAX_TEXT_LENGTH);
    g->typing = 0;
    memset(g->messages, 0, sizeof(char) * MAX_MESSAGES * MAX_TEXT_LENGTH);
    g->message_index = 0;
    g->day_length = DAY_LENGTH;
    glfwSetTime(g->day_length / 3.0);
    g->time_changed = 1;
}

/**
Calculates the vector for motion occuring in game.
\param[in] flying: Used to determine if the model is currently flying.
\param[in] sz: Determines forward/backward movement speed.
\param[in] sx: Determines left/right movement speed.
\param[in] rx: Determines left/right rotation.
\param[in] ry: Determines up/down rotation.
\param[out] vx: The x value of the motion vector.
\param[out] vy: The y value of the motion vector.
\param[out] vz: The z value of the motion vector.
*/
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

/**
Determines if collision occurs with an item in the world.
\param[in] height: The height of what is initiating the collision. Currently is only for players.
\param[out] x: x position of what is initiating the collision.
\param[out] y: y position of what is initiating the collision.
\param[out] z: z position of what is initiating the collision.
\param[in] model: The gl model struct.
*/
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

/**
Calculates the height of the highest block in the game.
\param[in] x: x position of the player.
\param[in] z: z position of the player.
\param[in] model: The gl model struct.
\return The value for the highest height in the game.
*/
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

/**
Handles movement for players in the game. Calls helper functions.
\param[in] dt: Change in time since the last frame was drawn.
\param[in] model: The gl model struct.
*/
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
            if (glfwGetKey(model->window, CRAFT_KEY_CROUCH))
            	sz = sz * 2;
	    else
		    sz--;
        
/*
The crouch function is implemented here utilizing an event caused by sz and sx
When the iteration occurs, it fails to itterate, ending the if statement.
*/   

	if (glfwGetKey(model->window, CRAFT_KEY_FORWARD))
            sz--;
        if (glfwGetKey(model->window, CRAFT_KEY_BACKWARD))
            if (glfwGetKey(model->window, CRAFT_KEY_CROUCH))
                sz = sz * 2;
            else
                    sz++;

        if (glfwGetKey(model->window, CRAFT_KEY_LEFT))
            if (glfwGetKey(model->window, CRAFT_KEY_CROUCH))
                sx = sx * 2;
            else
                    sx--;

        if (glfwGetKey(model->window, CRAFT_KEY_RIGHT))
            if (glfwGetKey(model->window, CRAFT_KEY_CROUCH))
                sx = sx * 2;
            else
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

/**
Handles key presses.
\param[in] window: The GL window that the key was pressed in.
\param[in] key: key that was pressed.
\param[in] scancode: Does not appear to be used in the on_key function.
\param[in] action: The action that was performed on the key.
\param[in] mods: If there modified controls are used. (Unsure)
*/
void on_key_wrapper(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if(g){
        on_key(window, key, scancode, action, mods, g);
    }
}

/**
Handles character input when typing in chat.
\param[in] window: The GL window that the key was pressed in.
\param[in] u: The unicode value of the character.
*/
void on_char_wrapper(GLFWwindow *window, unsigned int u)
{
    if (g)
    {
        on_char(window, u, g);
    }
}

/**
Handles mouse inputs.
\param[in] window: The GL window that the key was pressed in.
\param[in] button: Which mouse button was pressed.
\param[in] action: The action that was performed on the key.
\param[in] mods: If there modified controls are used. (Unsure)
*/
void on_mouse_button_wrapper(GLFWwindow *window, int button, int action, int mods)
{
    if (g)
    {
        on_mouse_button(window, button, action, mods, g);
    }
}

/**
Handles mouse scroll action.
\param[in] window: The GL window that the key was pressed in.
\param[in] xdelta: Change in the x scrolling, unused.
\param[in] ydelta: Change in the y scrolling.
*/
void on_scroll_wrapper(GLFWwindow *window, double xdelta, double ydelta)
{
    if (g)
    {
        on_scroll(window, xdelta, ydelta, g);
    }
}

/**
Parses the data that is received from a multiplayer server.
\param[in] buffer: The data buffer containing data sent from the server.
*/
void parse_buffer(char *buffer)
{
    Player *me = g->players;
    State *s = &g->players->state;
    char *key;
    char *line = tokenize(buffer, "\n", &key);
    while (line)
    {
        int pid;
        float ux, uy, uz, urx, ury;
        if (sscanf(line, "U,%d,%f,%f,%f,%f,%f",
                   &pid, &ux, &uy, &uz, &urx, &ury) == 6)
        {
            me->id = pid;
            s->x = ux;
            s->y = uy;
            s->z = uz;
            s->rx = urx;
            s->ry = ury;
            force_chunks(me);
            if (uy == 0)
            {
                s->y = highest_block(s->x, s->z, g) + 2;
            }
        }
        int bp, bq, bx, by, bz, bw;
        if (sscanf(line, "B,%d,%d,%d,%d,%d,%d",
                   &bp, &bq, &bx, &by, &bz, &bw) == 6)
        {
            _set_block(bp, bq, bx, by, bz, bw, 0, g);
            if (player_intersects_block(2, s->x, s->y, s->z, bx, by, bz))
            {
                s->y = highest_block(s->x, s->z, g) + 2;
            }
        }
        if (sscanf(line, "L,%d,%d,%d,%d,%d,%d",
                   &bp, &bq, &bx, &by, &bz, &bw) == 6)
        {
            set_light(bp, bq, bx, by, bz, bw);
        }
        float px, py, pz, prx, pry;
        if (sscanf(line, "P,%d,%f,%f,%f,%f,%f",
                   &pid, &px, &py, &pz, &prx, &pry) == 6)
        {
            Player *player = find_player(pid);
            if (!player && g->player_count < MAX_PLAYERS)
            {
                player = g->players + g->player_count;
                g->player_count++;
                player->id = pid;
                player->buffer = 0;
                snprintf(player->name, MAX_NAME_LENGTH, "player%d", pid);
                update_player(player, px, py, pz, prx, pry, 1); // twice
            }
            if (player)
            {
                update_player(player, px, py, pz, prx, pry, 1);
            }
        }
        if (sscanf(line, "D,%d", &pid) == 1)
        {
            delete_player(pid);
        }
        int kp, kq, kk;
        if (sscanf(line, "K,%d,%d,%d", &kp, &kq, &kk) == 3)
        {
            db_set_key(kp, kq, kk);
        }
        if (sscanf(line, "R,%d,%d", &kp, &kq) == 2)
        {
            Chunk *chunk = find_chunk(kp, kq, g);
            if (chunk)
            {
                dirty_chunk(chunk, g);
            }
        }
        double elapsed;
        int day_length;
        if (sscanf(line, "E,%lf,%d", &elapsed, &day_length) == 2)
        {
            glfwSetTime(fmod(elapsed, day_length));
            g->day_length = day_length;
            g->time_changed = 1;
        }
        if (line[0] == 'T' && line[1] == ',')
        {
            char *text = line + 2;
            add_message(text, g);
        }
        char format[64];
        snprintf(
            format, sizeof(format), "N,%%d,%%%ds", MAX_NAME_LENGTH - 1);
        char name[MAX_NAME_LENGTH];
        if (sscanf(line, format, &pid, name) == 2)
        {
            Player *player = find_player(pid);
            if (player)
            {
                strncpy(player->name, name, MAX_NAME_LENGTH);
            }
        }
        snprintf(
            format, sizeof(format),
            "S,%%d,%%d,%%d,%%d,%%d,%%d,%%%d[^\n]", MAX_SIGN_LENGTH - 1);
        int face;
        char text[MAX_SIGN_LENGTH] = {0};
        if (sscanf(line, format,
                   &bp, &bq, &bx, &by, &bz, &face, text) >= 6)
        {
            _set_sign(bp, bq, bx, by, bz, face, text, 0, g);
        }
        line = tokenize(NULL, "\n", &key);
    }
}

/**
Gets the file path of what is passed in.
\param[in] relative_folder: The folder that contains the file.
\param[in] filename: That name of the file that will be used to create the path.
\return A char array that contains the file path for the requested file.
*/
char* get_file_path(char* relative_folder, char* filename){
    char resolved_path[4096];
    realpath(relative_folder, resolved_path);
    int path_len = strlen(resolved_path);

    int file_len = strlen(filename) + 1;
    char *texture_path = (char *)malloc((path_len + file_len + 1) * sizeof(char));
    texture_path[0] = '\0';
    strncat(texture_path, resolved_path, path_len + 1);
    strncat(texture_path, "/", 1);
    strncat(texture_path, filename, file_len + 1);
    return texture_path;
}

/**
The main function for the game. Handles all function calls, creates the window, and runs a loop that updates all game information during play.
\param[in] argc: Number of arguments passed into the exe.
\param[in] argv: Arguments that are passed into the game exe.
\return 0 when the game is closed.
*/
int main(int argc, char **argv)
{
    // INITIALIZATION //
    curl_global_init(CURL_GLOBAL_DEFAULT);
    srand(time(NULL));
    rand();

    // WINDOW INITIALIZATION //
    if (!glfwInit())
    {
        return -1;
    }
    create_window();
    if (!g->window)
    {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(g->window);
    glfwSwapInterval(VSYNC);
    glfwSetInputMode(g->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetKeyCallback(g->window, on_key_wrapper);
    glfwSetCharCallback(g->window, on_char_wrapper);
    glfwSetMouseButtonCallback(g->window, on_mouse_button_wrapper);
    glfwSetScrollCallback(g->window, on_scroll_wrapper);

    if (glewInit() != GLEW_OK)
    {
        return -1;
    }

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glLogicOp(GL_INVERT);
    glClearColor(0, 0, 0, 1);

    // LOAD TEXTURES //

    char *texture_path = get_file_path("./textures/", "texture.png");
    GLuint texture;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    load_png_texture(texture_path);
    free(texture_path);

    char *font_path = get_file_path("./textures/", "font.png");
    GLuint font;
    glGenTextures(1, &font);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, font);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    load_png_texture(font_path);
    free(font_path);

    char *sky_path = get_file_path("./textures/", "sky.png");
    GLuint sky;
    glGenTextures(1, &sky);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, sky);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    load_png_texture(sky_path);
    free(sky_path);

    char *sign_path = get_file_path("./textures/", "sign.png");
    GLuint sign;
    glGenTextures(1, &sign);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, sign);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    load_png_texture(sign_path);
    free(sign_path);

    // LOAD SHADERS //
    Attrib block_attrib = {0};
    Attrib line_attrib = {0};
    Attrib text_attrib = {0};
    Attrib sky_attrib = {0};
    GLuint program;

    char *block_vertex_path = get_file_path("./shaders/", "block_vertex.glsl");
    char *block_fragment_path = get_file_path("./shaders/", "block_fragment.glsl");
    program = load_program(block_vertex_path, block_fragment_path);
    free(block_vertex_path);
    free(block_fragment_path);
    block_attrib.program = program;
    block_attrib.position = glGetAttribLocation(program, "position");
    block_attrib.normal = glGetAttribLocation(program, "normal");
    block_attrib.uv = glGetAttribLocation(program, "uv");
    block_attrib.matrix = glGetUniformLocation(program, "matrix");
    block_attrib.sampler = glGetUniformLocation(program, "sampler");
    block_attrib.extra1 = glGetUniformLocation(program, "sky_sampler");
    block_attrib.extra2 = glGetUniformLocation(program, "daylight");
    block_attrib.extra3 = glGetUniformLocation(program, "fog_distance");
    block_attrib.extra4 = glGetUniformLocation(program, "ortho");
    block_attrib.camera = glGetUniformLocation(program, "camera");
    block_attrib.timer = glGetUniformLocation(program, "timer");

    char *line_vertex_path = get_file_path("./shaders/", "line_vertex.glsl");
    char *line_fragment_path = get_file_path("./shaders/", "line_fragment.glsl");
    program = load_program(line_vertex_path, line_fragment_path);
    free(line_vertex_path);
    free(line_fragment_path);
    line_attrib.program = program;
    line_attrib.position = glGetAttribLocation(program, "position");
    line_attrib.matrix = glGetUniformLocation(program, "matrix");

    char *text_vertex_path = get_file_path("./shaders/", "text_vertex.glsl");
    char *text_fragment_path = get_file_path("./shaders/", "text_fragment.glsl");
    program = load_program(text_vertex_path, text_fragment_path);
    free(text_vertex_path);
    free(text_fragment_path);
    text_attrib.program = program;
    text_attrib.position = glGetAttribLocation(program, "position");
    text_attrib.uv = glGetAttribLocation(program, "uv");
    text_attrib.matrix = glGetUniformLocation(program, "matrix");
    text_attrib.sampler = glGetUniformLocation(program, "sampler");
    text_attrib.extra1 = glGetUniformLocation(program, "is_sign");

    char *sky_vertex_path = get_file_path("./shaders/", "sky_vertex.glsl");
    char *sky_fragment_path = get_file_path("./shaders/", "sky_fragment.glsl");
    program = load_program( sky_vertex_path, sky_fragment_path);
    free(sky_vertex_path);
    free(sky_fragment_path);
    sky_attrib.program = program;
    sky_attrib.position = glGetAttribLocation(program, "position");
    sky_attrib.normal = glGetAttribLocation(program, "normal");
    sky_attrib.uv = glGetAttribLocation(program, "uv");
    sky_attrib.matrix = glGetUniformLocation(program, "matrix");
    sky_attrib.sampler = glGetUniformLocation(program, "sampler");
    sky_attrib.timer = glGetUniformLocation(program, "timer");

    // CHECK COMMAND LINE ARGUMENTS //
    if (argc == 2 || argc == 3)
    {
        g->mode = MODE_ONLINE;
        strncpy(g->server_addr, argv[1], MAX_ADDR_LENGTH);
        g->server_port = argc == 3 ? atoi(argv[2]) : DEFAULT_PORT;
        snprintf(g->db_path, MAX_PATH_LENGTH,
                 "cache.%s.%d.db", g->server_addr, g->server_port);
    }
    else
    {
        g->mode = MODE_OFFLINE;
        snprintf(g->db_path, MAX_PATH_LENGTH, "%s", DB_PATH);
    }

    g->create_radius = CREATE_CHUNK_RADIUS;
    g->render_radius = RENDER_CHUNK_RADIUS;
    g->delete_radius = DELETE_CHUNK_RADIUS;
    g->sign_radius = RENDER_SIGN_RADIUS;

    // INITIALIZE WORKER THREADS
    for (int i = 0; i < WORKERS; i++)
    {
        Worker *worker = g->workers + i;
        worker->index = i;
        worker->state = WORKER_IDLE;
        mtx_init(&worker->mtx, mtx_plain);
        cnd_init(&worker->cnd);
        thrd_create(&worker->thrd, worker_run, worker);
    }

    // OUTER LOOP //
    int running = 1;
    while (running)
    {
        // DATABASE INITIALIZATION //
        if (g->mode == MODE_OFFLINE || USE_CACHE)
        {
            db_enable();
            if (db_init(g->db_path))
            {
                return -1;
            }
            if (g->mode == MODE_ONLINE)
            {
                // TODO: support proper caching of signs (handle deletions)
                db_delete_all_signs();
            }
        }

        // CLIENT INITIALIZATION //
        if (g->mode == MODE_ONLINE)
        {
            client_enable();
            client_connect(g->server_addr, g->server_port);
            client_start();
            client_version(1);
            login(g);
        }

        // LOCAL VARIABLES //
        reset_model();
        FPS fps = {0, 0, 0};
        double last_commit = glfwGetTime();
        double last_update = glfwGetTime();
        GLuint sky_buffer = gen_sky_buffer();

        Player *me = g->players;
		me->health = 100;
        State *s = &g->players->state;
        me->id = 0;
        me->name[0] = '\0';
        me->buffer = 0;
        g->player_count = 1;

        // LOAD STATE FROM DATABASE //
        int loaded = db_load_state(&s->x, &s->y, &s->z, &s->rx, &s->ry);
        force_chunks(me);
        if (!loaded)
        {
            s->y = highest_block(s->x, s->z, g) + 2;
        }

        // BEGIN MAIN LOOP //
        double previous = glfwGetTime();
        while (1)
        {
            // WINDOW SIZE AND SCALE //
            g->scale = get_scale_factor();
            glfwGetFramebufferSize(g->window, &g->width, &g->height);
            glViewport(0, 0, g->width, g->height);

            // FRAME RATE //
            if (g->time_changed)
            {
                g->time_changed = 0;
                last_commit = glfwGetTime();
                last_update = glfwGetTime();
                memset(&fps, 0, sizeof(fps));
            }
            update_fps(&fps);
            double now = glfwGetTime();
            double dt = now - previous;
            dt = MIN(dt, 0.2);
            dt = MAX(dt, 0.0);
            previous = now;

            // HANDLE MOUSE INPUT //
            handle_mouse_input(g);

            // HANDLE MOVEMENT //
            handle_movement(dt, g);

            // HANDLE DATA FROM SERVER //
            char *buffer = client_recv();
            if (buffer)
            {
                parse_buffer(buffer);
                free(buffer);
            }

            // FLUSH DATABASE //
            if (now - last_commit > COMMIT_INTERVAL)
            {
                last_commit = now;
                db_commit();
            }

            // SEND POSITION TO SERVER //
            if (now - last_update > 0.1)
            {
                last_update = now;
                client_position(s->x, s->y, s->z, s->rx, s->ry);
            }

            // PREPARE TO RENDER //
            g->observe1 = g->observe1 % g->player_count;
            g->observe2 = g->observe2 % g->player_count;
            delete_chunks();
            del_buffer(me->buffer);
            me->buffer = gen_player_buffer(s->x, s->y, s->z, s->rx, s->ry);
            for (int i = 1; i < g->player_count; i++)
            {
                interpolate_player(g->players + i);
            }
            Player *player = g->players + g->observe1;

            // RENDER 3-D SCENE //
            glClear(GL_COLOR_BUFFER_BIT);
            glClear(GL_DEPTH_BUFFER_BIT);
            render_sky(&sky_attrib, player, sky_buffer);
            glClear(GL_DEPTH_BUFFER_BIT);
            int face_count = render_chunks(&block_attrib, player);
            render_signs(&text_attrib, player);
            render_sign(&text_attrib, player);
            render_players(&block_attrib, player);
            if (SHOW_WIREFRAME)
            {
                render_wireframe(&line_attrib, player);
            }

            // RENDER HUD //
            glClear(GL_DEPTH_BUFFER_BIT);
            if (SHOW_CROSSHAIRS)
            {
                render_crosshairs(&line_attrib);
            }
            if (SHOW_ITEM)
            {
                render_item(&block_attrib);
            }

            // RENDER TEXT //
            char text_buffer[1024];
            float ts = 12 * g->scale;
            float tx = ts / 2;
            float ty = g->height - ts;
            if (SHOW_INFO_TEXT)
            {
                int hour = time_of_day() * 24;
                char am_pm = hour < 12 ? 'a' : 'p';
                hour = hour % 12;
                hour = hour ? hour : 12;
                snprintf(
                    text_buffer, 1024,
                    "(%d, %d) (%.2f, %.2f, %.2f) [%d, %d, %d] %d%cm %dfps Health: %d",
                    chunked(s->x), chunked(s->z), s->x, s->y, s->z,
                    g->player_count, g->chunk_count,
                    face_count * 2, hour, am_pm, fps.fps, player->health);
                render_text(&text_attrib, ALIGN_LEFT, tx, ty, ts, text_buffer);
                ty -= ts * 2;
            }
            if (SHOW_CHAT_TEXT)
            {
                for (int i = 0; i < MAX_MESSAGES; i++)
                {
                    int index = (g->message_index + i) % MAX_MESSAGES;
                    if (strlen(g->messages[index]))
                    {
                        render_text(&text_attrib, ALIGN_LEFT, tx, ty, ts,
                                    g->messages[index]);
                        ty -= ts * 2;
                    }
                }
            }
            if (g->typing)
            {
                snprintf(text_buffer, 1024, "> %s", g->typing_buffer);
                render_text(&text_attrib, ALIGN_LEFT, tx, ty, ts, text_buffer);
                ty -= ts * 2;
            }
            if (SHOW_PLAYER_NAMES)
            {
                if (player != me)
                {
                    render_text(&text_attrib, ALIGN_CENTER,
                                g->width / 2, ts, ts, player->name);
                }
                Player *other = player_crosshair(player);
                if (other)
                {
                    render_text(&text_attrib, ALIGN_CENTER,
                                g->width / 2, g->height / 2 - ts - 24, ts,
                                other->name);
                }
            }

            // RENDER PICTURE IN PICTURE //
            if (g->observe2)
            {
                player = g->players + g->observe2;

                int pw = 256 * g->scale;
                int ph = 256 * g->scale;
                int offset = 32 * g->scale;
                int pad = 3 * g->scale;
                int sw = pw + pad * 2;
                int sh = ph + pad * 2;

                glEnable(GL_SCISSOR_TEST);
                glScissor(g->width - sw - offset + pad, offset - pad, sw, sh);
                glClear(GL_COLOR_BUFFER_BIT);
                glDisable(GL_SCISSOR_TEST);
                glClear(GL_DEPTH_BUFFER_BIT);
                glViewport(g->width - pw - offset, offset, pw, ph);

                g->width = pw;
                g->height = ph;
                g->ortho = 0;
                g->fov = 65;

                render_sky(&sky_attrib, player, sky_buffer);
                glClear(GL_DEPTH_BUFFER_BIT);
                render_chunks(&block_attrib, player);
                render_signs(&text_attrib, player);
                render_players(&block_attrib, player);
                glClear(GL_DEPTH_BUFFER_BIT);
                if (SHOW_PLAYER_NAMES)
                {
                    render_text(&text_attrib, ALIGN_CENTER,
                                pw / 2, ts, ts, player->name);
                }
            }

            // SWAP AND POLL //
            glfwSwapBuffers(g->window);
            glfwPollEvents();
            if (glfwWindowShouldClose(g->window))
            {
                running = 0;
                break;
            }
            if (g->mode_changed)
            {
                g->mode_changed = 0;
                break;
            }
        }

        // SHUTDOWN //
        db_save_state(s->x, s->y, s->z, s->rx, s->ry);
        db_close();
        db_disable();
        client_stop();
        client_disable();
        del_buffer(sky_buffer);
        delete_all_chunks();
        delete_all_players();
    }

    glfwTerminate();
    curl_global_cleanup();
    return 0;
}
