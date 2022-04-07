#include "config.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "structs.h"
#include <math.h>
#include "util.h"
#include "item.h"
#include "db.h"
#include "client.h"
#include "string.h"
#include <stdio.h>
#include "auth.h"
#include "chunk.h"


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

void handle_mouse_input(Model *model)
{
    int exclusive =
        glfwGetInputMode(model->window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
    static double px = 0;
    static double py = 0;
    State *s = &model->players->state;
    if (exclusive && (px || py))
    {
        double mx, my;
        glfwGetCursorPos(model->window, &mx, &my);
        float m = 0.0025;
        s->rx += (mx - px) * m;
        if (INVERT_MOUSE)
        {
            s->ry += (my - py) * m;
        }
        else
        {
            s->ry -= (my - py) * m;
        }
        if (s->rx < 0)
        {
            s->rx += RADIANS(360);
        }
        if (s->rx >= RADIANS(360))
        {
            s->rx -= RADIANS(360);
        }
        s->ry = MAX(s->ry, -RADIANS(90));
        s->ry = MIN(s->ry, RADIANS(90));
        px = mx;
        py = my;
    }
    else
    {
        glfwGetCursorPos(model->window, &px, &py);
    }
}

void get_sight_vector(float rx, float ry, float *vx, float *vy, float *vz)
{
    float m = cosf(ry);
    *vx = cosf(rx - RADIANS(90)) * m;
    *vy = sinf(ry);
    *vz = sinf(rx - RADIANS(90)) * m;
}

int _hit_test(
    Map *map, float max_distance, int previous,
    float x, float y, float z,
    float vx, float vy, float vz,
    int *hx, int *hy, int *hz)
{
    int m = 32;
    int px = 0;
    int py = 0;
    int pz = 0;
    for (int i = 0; i < max_distance * m; i++)
    {
        int nx = roundf(x);
        int ny = roundf(y);
        int nz = roundf(z);
        if (nx != px || ny != py || nz != pz)
        {
            int hw = map_get(map, nx, ny, nz);
            if (hw > 0)
            {
                if (previous)
                {
                    *hx = px;
                    *hy = py;
                    *hz = pz;
                }
                else
                {
                    *hx = nx;
                    *hy = ny;
                    *hz = nz;
                }
                return hw;
            }
            px = nx;
            py = ny;
            pz = nz;
        }
        x += vx / m;
        y += vy / m;
        z += vz / m;
    }
    return 0;
}

int hit_test(
    int previous, float x, float y, float z, float rx, float ry,
    int *bx, int *by, int *bz, Model* model)
{
    int result = 0;
    float best = 0;
    int p = chunked(x);
    int q = chunked(z);
    float vx, vy, vz;
    get_sight_vector(rx, ry, &vx, &vy, &vz);
    for (int i = 0; i < model->chunk_count; i++)
    {
        Chunk *chunk = model->chunks + i;
        if (chunk_distance(chunk, p, q) > 1)
        {
            continue;
        }
        int hx, hy, hz;
        int hw = _hit_test(&chunk->map, 8, previous,
                           x, y, z, vx, vy, vz, &hx, &hy, &hz);
        if (hw > 0)
        {
            float d = sqrtf(
                powf(hx - x, 2) + powf(hy - y, 2) + powf(hz - z, 2));
            if (best == 0 || d < best)
            {
                best = d;
                *bx = hx;
                *by = hy;
                *bz = hz;
                result = hw;
            }
        }
    }
    return result;
}

int hit_test_face(Player *player, int *x, int *y, int *z, int *face, Model *model)
{
    State *s = &player->state;
    int w = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, x, y, z, model);
    if (is_obstacle(w))
    {
        int hx, hy, hz;
        hit_test(1, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz, model);
        int dx = hx - *x;
        int dy = hy - *y;
        int dz = hz - *z;
        if (dx == -1 && dy == 0 && dz == 0)
        {
            *face = 0;
            return 1;
        }
        if (dx == 1 && dy == 0 && dz == 0)
        {
            *face = 1;
            return 1;
        }
        if (dx == 0 && dy == 0 && dz == -1)
        {
            *face = 2;
            return 1;
        }
        if (dx == 0 && dy == 0 && dz == 1)
        {
            *face = 3;
            return 1;
        }
        if (dx == 0 && dy == 1 && dz == 0)
        {
            int degrees = roundf(DEGREES(atan2f(s->x - hx, s->z - hz)));
            if (degrees < 0)
            {
                degrees += 360;
            }
            int top = ((degrees + 45) / 90) % 4;
            *face = 4 + top;
            return 1;
        }
    }
    return 0;
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

void on_left_click(Model *model)
{
    State *s = &model->players->state;
    int hx, hy, hz;
    int hw = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz, model);
    if (hy > 0 && hy < 256 && is_destructable(hw))
    {
        set_block(hx, hy, hz, 0, model);
        record_block(hx, hy, hz, 0, model);
        if (is_plant(get_block(hx, hy + 1, hz, model)))
        {
            set_block(hx, hy + 1, hz, 0, model);
        }
    }
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


void on_right_click(Model *model)
{
    State *s = &model->players->state;
    int hx, hy, hz;
    int hw = hit_test(1, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz, model);
    if (hy > 0 && hy < 256 && is_obstacle(hw))
    {
        if (!player_intersects_block(2, s->x, s->y, s->z, hx, hy, hz))
        {
            set_block(hx, hy, hz, items[model->item_index], model);
            record_block(hx, hy, hz, items[model->item_index], model);
        }
    }
}

void on_middle_click(Model *model)
{
    State *s = &model->players->state;
    int hx, hy, hz;
    int hw = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz, model);
    for (int i = 0; i < item_count; i++)
    {
        if (items[i] == hw)
        {
            model->item_index = i;
            break;
        }
    }
}

void toggle_light(int x, int y, int z, Model *model)
{
    int p = chunked(x);
    int q = chunked(z);
    Chunk *chunk = find_chunk(p, q, model);
    if (chunk)
    {
        Map *map = &chunk->lights;
        int w = map_get(map, x, y, z) ? 0 : 15;
        map_set(map, x, y, z, w);
        db_insert_light(p, q, x, y, z, w);
        client_light(x, y, z, w);
        dirty_chunk(chunk, model);
    }
}

void on_light(Model *model)
{
    State *s = &model->players->state;
    int hx, hy, hz;
    int hw = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz, model);
    if (hy > 0 && hy < 256 && is_destructable(hw))
    {
        toggle_light(hx, hy, hz, model);
    }
}

void on_mouse_button(GLFWwindow *window, int button, int action, int mods, Model *model)
{
    int control = mods & (GLFW_MOD_CONTROL | GLFW_MOD_SUPER);
    int exclusive =
        glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
    if (action != GLFW_PRESS)
    {
        return;
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (exclusive)
        {
            if (control)
            {
                on_right_click(model);
            }
            else
            {
                on_left_click(model);
            }
        }
        else
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (exclusive)
        {
            if (control)
            {
                on_light(model);
            }
            else
            {
                on_right_click(model);
            }
        }
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE)
    {
        if (exclusive)
        {
            on_middle_click(model);
        }
    }
}

void on_scroll(GLFWwindow *window, double xdelta, double ydelta, Model *model)
{
    static double ypos = 0;
    ypos += ydelta;
    if (ypos < -SCROLL_THRESHOLD)
    {
        model->item_index = (model->item_index + 1) % item_count;
        ypos = 0;
    }
    if (ypos > SCROLL_THRESHOLD)
    {
        model->item_index--;
        if (model->item_index < 0)
        {
            model->item_index = item_count - 1;
        }
        ypos = 0;
    }
}

void on_char(GLFWwindow *window, unsigned int u, Model *model)
{
    if (model->suppress_char)
    {
        model->suppress_char = 0;
        return;
    }
    if (model->typing)
    {
        if (u >= 32 && u < 128)
        {
            char c = (char)u;
            int n = strlen(model->typing_buffer);
            if (n < MAX_TEXT_LENGTH - 1)
            {
                model->typing_buffer[n] = c;
                model->typing_buffer[n + 1] = '\0';
            }
        }
    }
    else
    {
        if (u == CRAFT_KEY_CHAT)
        {
            model->typing = 1;
            model->typing_buffer[0] = '\0';
        }
        if (u == CRAFT_KEY_COMMAND)
        {
            model->typing = 1;
            model->typing_buffer[0] = '/';
            model->typing_buffer[1] = '\0';
        }
        if (u == CRAFT_KEY_SIGN)
        {
            model->typing = 1;
            model->typing_buffer[0] = CRAFT_KEY_SIGN;
            model->typing_buffer[1] = '\0';
        }
    }
}
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
    int p, int q, int x, int y, int z, int face, const char *text, int dirty, Model* model)
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

void add_message(const char *text, Model* model)
{
    printf("%s\n", text);
    snprintf(
        model->messages[model->message_index], MAX_TEXT_LENGTH, "%s", text);
    model->message_index = (model->message_index + 1) % MAX_MESSAGES;
}

void login(Model *model)
{
    char username[128] = {0};
    char identity_token[128] = {0};
    char access_token[128] = {0};
    if (db_auth_get_selected(username, 128, identity_token, 128))
    {
        printf("Contacting login server for username: %s\n", username);
        if (get_access_token(
                access_token, 128, username, identity_token))
        {
            printf("Successfully authenticated with the login server\n");
            client_login(username, access_token);
        }
        else
        {
            printf("Failed to authenticate with the login server\n");
            client_login("", "");
        }
    }
    else
    {
        printf("Logging in anonymously\n");
        client_login("", "");
    }
}

void builder_block(int x, int y, int z, int w, Model* model)
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

void parse_command(const char *buffer, int forward, Model *model)
{
    char username[128] = {0};
    char token[128] = {0};
    char server_addr[MAX_ADDR_LENGTH];
    int server_port = DEFAULT_PORT;
    char filename[MAX_PATH_LENGTH];
    int radius, count, xc, yc, zc;
    if (sscanf(buffer, "/identity %128s %128s", username, token) == 2)
    {
        db_auth_set(username, token);
        add_message("Successfully imported identity token!", model);
        login(model);
    }
    else if (strcmp(buffer, "/logout") == 0)
    {
        db_auth_select_none();
        login(model);
    }
    else if (sscanf(buffer, "/login %128s", username) == 1)
    {
        if (db_auth_select(username))
        {
            login(model);
        }
        else
        {
            add_message("Unknown username.", model);
        }
    }
    else if (sscanf(buffer,
                    "/online %128s %d", server_addr, &server_port) >= 1)
    {
        model->mode_changed = 1;
        model->mode = MODE_ONLINE;
        strncpy(model->server_addr, server_addr, MAX_ADDR_LENGTH);
        model->server_port = server_port;
        snprintf(model->db_path, MAX_PATH_LENGTH,
                 "cache.%s.%d.db", model->server_addr, model->server_port);
    }
    else if (sscanf(buffer, "/offline %128s", filename) == 1)
    {
        model->mode_changed = 1;
        model->mode = MODE_OFFLINE;
        snprintf(model->db_path, MAX_PATH_LENGTH, "%s.db", filename);
    }
    else if (strcmp(buffer, "/offline") == 0)
    {
        model->mode_changed = 1;
        model->mode = MODE_OFFLINE;
        snprintf(model->db_path, MAX_PATH_LENGTH, "%s", DB_PATH);
    }
    else if (sscanf(buffer, "/view %d", &radius) == 1)
    {
        if (radius >= 1 && radius <= 24)
        {
            model->create_radius = radius;
            model->render_radius = radius;
            model->delete_radius = radius + 4;
        }
        else
        {
            add_message("Viewing distance must be between 1 and 24.", model);
        }
    }
    else if (strcmp(buffer, "/copy") == 0)
    {
        copy(model);
    }
    else if (strcmp(buffer, "/paste") == 0)
    {
        paste(model);
    }
    else if (strcmp(buffer, "/tree") == 0)
    {
        tree(&model->block0, model);
    }
    else if (sscanf(buffer, "/array %d %d %d", &xc, &yc, &zc) == 3)
    {
        array(&model->block1, &model->block0, xc, yc, zc, model);
    }
    else if (sscanf(buffer, "/array %d", &count) == 1)
    {
        array(&model->block1, &model->block0, count, count, count, model);
    }
    else if (strcmp(buffer, "/fcube") == 0)
    {
        cube(&model->block0, &model->block1, 1, model);
    }
    else if (strcmp(buffer, "/cube") == 0)
    {
        cube(&model->block0, &model->block1, 0, model);
    }
    else if (sscanf(buffer, "/fsphere %d", &radius) == 1)
    {
        sphere(&model->block0, radius, 1, 0, 0, 0, model);
    }
    else if (sscanf(buffer, "/sphere %d", &radius) == 1)
    {
        sphere(&model->block0, radius, 0, 0, 0, 0, model);
    }
    else if (sscanf(buffer, "/fcirclex %d", &radius) == 1)
    {
        sphere(&model->block0, radius, 1, 1, 0, 0, model);
    }
    else if (sscanf(buffer, "/circlex %d", &radius) == 1)
    {
        sphere(&model->block0, radius, 0, 1, 0, 0, model);
    }
    else if (sscanf(buffer, "/fcircley %d", &radius) == 1)
    {
        sphere(&model->block0, radius, 1, 0, 1, 0, model);
    }
    else if (sscanf(buffer, "/circley %d", &radius) == 1)
    {
        sphere(&model->block0, radius, 0, 0, 1, 0, model);
    }
    else if (sscanf(buffer, "/fcirclez %d", &radius) == 1)
    {
        sphere(&model->block0, radius, 1, 0, 0, 1, model);
    }
    else if (sscanf(buffer, "/circlez %d", &radius) == 1)
    {
        sphere(&model->block0, radius, 0, 0, 0, 1, model);
    }
    else if (sscanf(buffer, "/fcylinder %d", &radius) == 1)
    {
        cylinder(&model->block0, &model->block1, radius, 1, model);
    }
    else if (sscanf(buffer, "/cylinder %d", &radius) == 1)
    {
        cylinder(&model->block0, &model->block1, radius, 0, model);
    }
    else if (forward)
    {
        client_talk(buffer);
    }
}

void on_key(GLFWwindow *window, int key, int scancode, int action, int mods, Model *model)
{
    int control = mods & (GLFW_MOD_CONTROL | GLFW_MOD_SUPER);
    int exclusive =
        glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
    if (action == GLFW_RELEASE)
    {
        return;
    }
    if (key == GLFW_KEY_BACKSPACE)
    {
        if (model->typing)
        {
            int n = strlen(model->typing_buffer);
            if (n > 0)
            {
                model->typing_buffer[n - 1] = '\0';
            }
        }
    }
    if (action != GLFW_PRESS)
    {
        return;
    }
    if (key == GLFW_KEY_ESCAPE)
    {
        if (model->typing)
        {
            model->typing = 0;
        }
        else if (exclusive)
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
    if (key == GLFW_KEY_ENTER)
    {
        if (model->typing)
        {
            if (mods & GLFW_MOD_SHIFT)
            {
                int n = strlen(model->typing_buffer);
                if (n < MAX_TEXT_LENGTH - 1)
                {
                    model->typing_buffer[n] = '\r';
                    model->typing_buffer[n + 1] = '\0';
                }
            }
            else
            {
                model->typing = 0;
                if (model->typing_buffer[0] == CRAFT_KEY_SIGN)
                {
                    Player *player = model->players;
                    int x, y, z, face;
                    if (hit_test_face(player, &x, &y, &z, &face, model))
                    {
                        set_sign(x, y, z, face, model->typing_buffer + 1, model);
                    }
                }
                else if (model->typing_buffer[0] == '/')
                {
                    parse_command(model->typing_buffer, 1, model);
                }
                else
                {
                    client_talk(model->typing_buffer);
                }
            }
        }
        else
        {
            if (control)
            {
                on_right_click(model);
            }
            else
            {
                on_left_click(model);
            }
        }
    }
    if (control && key == 'V')
    {
        const char *buffer = glfwGetClipboardString(window);
        if (model->typing)
        {
            model->suppress_char = 1;
            strncat(model->typing_buffer, buffer,
                    MAX_TEXT_LENGTH - strlen(model->typing_buffer) - 1);
        }
        else
        {
            parse_command(buffer, 0, model);
        }
    }
    if (!model->typing)
    {
        if (key == CRAFT_KEY_FLY)
        {
            model->flying = !model->flying;
        }
        if (key >= '1' && key <= '9')
        {
            model->item_index = key - '1';
        }
        if (key == '0')
        {
            model->item_index = 9;
        }
        if (key == CRAFT_KEY_ITEM_NEXT)
        {
            model->item_index = (model->item_index + 1) % item_count;
        }
        if (key == CRAFT_KEY_ITEM_PREV)
        {
            model->item_index--;
            if (model->item_index < 0)
            {
                model->item_index = item_count - 1;
            }
        }
        if (key == CRAFT_KEY_OBSERVE)
        {
            model->observe1 = (model->observe1 + 1) % model->player_count;
        }
        if (key == CRAFT_KEY_OBSERVE_INSET)
        {
            model->observe2 = (model->observe2 + 1) % model->player_count;
        }
    }
}
