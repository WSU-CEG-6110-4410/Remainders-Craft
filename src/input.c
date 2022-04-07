#include "config.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <math.h>
#include "util.h"
#include "item.h"
#include "db.h"
#include "client.h"
#include "string.h"
#include <stdio.h>
#include "auth.h"
#include "structs.h"
#include "chunk.h"
#include "block.h"
#include "hit.h"

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
