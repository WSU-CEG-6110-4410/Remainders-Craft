#ifndef _input_h_
#define _input_h_

#include "structs.h"

void handle_mouse_input(Model *model);

void on_left_click(Model *model);

void on_right_click(Model *model);

void on_middle_click(Model *model);

void toggle_light(int x, int y, int z, Model *model);

void on_light(Model *model);

void on_mouse_button(GLFWwindow *window, int button, int action, int mods, Model *model);

void on_scroll(GLFWwindow *window, double xdelta, double ydelta, Model *model);

void on_char(GLFWwindow *window, unsigned int u, Model *model);

void add_message(const char *text, Model *model);

void login(Model *model);

void parse_command(const char *buffer, int forward, Model *model);

void on_key(GLFWwindow *window, int key, int scancode, int action, int mods, Model *model);

#endif