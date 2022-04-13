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
///\param[in,out] model: The current game instance.
/// Will be modified to change item_index.
void on_middle_click(Model *model);

/// Use this function to toggle the light in a
/// specified coordinate area.
///\param[in] x: The x coordinate of the area.
///\param[in] y: The y coordinate of the area.
///\param[in] z: The z coordinate of the area.
///\param[in,out] model: The current game instance.
/// Will be modified if the given coordinate is associated
/// with a chunk.
void toggle_light(int x, int y, int z, Model *model);

/// Use this function to trigger a light toggle effect
/// where the player is currently looking.
///\param[in,out] model: The current game instance.
/// Will be modified if a light toggle can be applied
/// to where the player is currently pointing.
void on_light(Model *model);

/// Use this function to translate a mouse input to
/// the various actions which it can produce. Should
/// be used as a result of [glfwSetMouseButtonCallback](https://www.glfw.org/docs/3.3/group__input.html#ga6ab84420974d812bee700e45284a723c).
/// Reference glfw3.h for openGL constants.
///\param[in] window: The openGL window object.
///\param[in] button: An integer associated with a button
/// constant.
///\param[in] action: One of GLFW_PRESS or GLFW_RELEASE.
///\param[in] mods: Bit field describing which modifier
/// keys were held down
///\param[in,out] model: The current game instance.
/// Will be modified make actions take effect.
void on_mouse_button(GLFWwindow *window, int button, int action, int mods, Model *model);

/// Use this function to translate a scroll event. Should
/// be used as a result of [glfwSetScrollCallback](https://www.glfw.org/docs/3.3/group__input.html#ga571e45a030ae4061f746ed56cb76aede).
///\param[in] window: The openGL window object.
///\param[in] xdelta: The scroll offset along the x-axis.
///\param[in] ydelta: The scroll offset along the y-axis.
///\param[in,out] model: The current game instance.
/// Will be modified set item_index.
void on_scroll(GLFWwindow *window, double xdelta, double ydelta, Model *model);

/// Use this function to translate an input character. Should
/// be used as a result of [glfwSetCharCallback](https://www.glfw.org/docs/3.3/group__input.html#gab25c4a220fd8f5717718dbc487828996).
///\param[in] window: The openGL window object.
///\param[in] u: The Unicode code point of the character.
///\param[in,out] model: The current game instance.
/// Will be modified write the typed chars to a buffer.
void on_char(GLFWwindow *window, unsigned int u, Model *model);

/// Use this function to add a message to the game's
/// message logs.
///\param[in] text: The message to be added.
///\param[in,out] model: The current game instance which
/// the message will be added to.
void add_message(const char *text, Model *model);

/// Use this function to login the db selected username
/// to the server.
void login();

/// Use this function to parse a buffer as chat text, or as a
/// game command.
///\param[in] buffer: The buffer to be parsed.
///\param[in] forward: Indicates whether the buffer is a command.
///\param[in,out] model: Current game instance. Will be modified
/// to record text or respond to a command.
void parse_command(const char *buffer, int forward, Model *model);

/// Use this function to translate key controls. Should
/// be used as a result of [glfwSetKeyCallback](https://www.glfw.org/docs/3.3/group__input.html#ga1caf18159767e761185e49a3be019f8d).
///\param[in] window: The openGL window object.
///\param[in] key: The keyboard key that was pressed or released.
///\param[in] scancode: The system-specific scancode of the key.
///\param[in] action: GLFW_PRESS, GLFW_RELEASE or GLFW_REPEAT
///\param[in] mods: Bit field describing which modifier keys 
/// were held down.
///\param[in,out] model: The current game instance. May be
/// modified to respond to keyboard actions.
void on_key(GLFWwindow *window, int key, int scancode, int action, int mods, Model *model);

#endif
