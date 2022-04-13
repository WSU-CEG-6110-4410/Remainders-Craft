#ifndef _input_h_
#define _input_h_

#include "structs.h"

/// [issue](https://github.com/WSU-CEG-6110-4410/Remainders-Craft/issues/8)
/// [issue](https://github.com/WSU-CEG-6110-4410/Remainders-Craft/issues/10)

/// Use this function to translate cursor movements
/// for a given framerate of the game instance.
///\param[in,out] model: The current game instance.
/// The player state will be translated according
/// to the cursor position.
void handle_mouse_input(Model *model);

/// Use this function to produce an action for
/// a left click. It attempts to set a block.
///\param[in,out]: The current game instance.
/// Might be modified if a block can be set.
void on_left_click(Model *model);

/// Use this function to produce an action for
/// a right click. It attempts to destroy a block.
///\param[in,out]: The current game instance.
/// Might be modified if a block can be destroyed.
void on_right_click(Model *model);

/// Use this function to produce an action for
/// a middle click. It cycles through the block which
/// the user can set.
///\param[in,out]: The current game instance.
/// Will be modified to change item_index.
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
