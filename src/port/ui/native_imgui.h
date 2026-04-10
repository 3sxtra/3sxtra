#ifndef NATIVE_IMGUI_H
#define NATIVE_IMGUI_H

#include <stdbool.h>
#include <stdint.h>

/* Forward declarations from legacy engine */
struct _TASK;

#ifdef __cplusplus
extern "C" {
#endif

typedef enum NativeUIDir {
    UI_DIR_VERTICAL,
    UI_DIR_HORIZONTAL
} NativeUIDir;

/**
 * @brief Initialize a new Native GUI frame.
 * @param start_x The screen X coordinate where auto-layout begins.
 * @param start_y The screen Y coordinate where auto-layout begins.
 * @param dir The direction that elements will automatically stack.
 */
void NativeUI_Clear(void);
void NativeUI_Begin(int start_x, int start_y, NativeUIDir dir);

/**
 * @brief Initialize a scrolling layout boundary.
 * @param visible_elements The max number of items to display on screen.
 */
void NativeUI_BeginScrollList(int visible_elements);

/**
 * @brief Conclude the scrolling layout boundary context.
 */
void NativeUI_EndScrollList(void);

/**
 * @brief End a Native GUI frame. Any dynamically allocated effects
 *        from the previous frame that weren't drawn this frame 
 *        will be garbage collected and terminated.
 */
void NativeUI_End(void);

/**
 * @brief Draws a large legacy menu header (wraps eff57).
 * @param header_type The MENU_HEADER_* enum value.
 */
void NativeUI_Header(int header_type);

/**
 * @brief Renders a standard menu button using proportional fonts
 *        and the legacy eff61 red-cursor highlight box.
 * @param label The text to display.
 * @return true if the user confirmed/pressed this button on this frame.
 */
bool NativeUI_Button(const char* label);

/**
 * @brief Renders a standard menu button in a disabled state (grey).
 * @param label The text to display.
 * @param disabled If true, the button is rendered grey and cannot be focused.
 *                 Returns false unconditionally if disabled.
 * @return true if the user confirmed/pressed this button on this frame.
 */
bool NativeUI_ButtonEx(const char* label, bool disabled);

/**
 * @brief Draws static proportional text (eff45).
 * @param label The text to display.
 */
void NativeUI_Label(const char* label);

/**
 * @brief Advanced: Force manual layout coordinates for the next element.
 */
void NativeUI_SetNextPos(int x, int y);

/**
 * @brief Pushes an explicitly scoped ID onto the hash stack to prevent
 *        hash collisions between identical labels (like multiple 'Back' buttons).
 * @param dynamic_id Unique integer ID for the current scope/index.
 */
void NativeUI_PushID(int dynamic_id);

/**
 * @brief Pops the previously scoped ID.
 */
void NativeUI_PopID(void);

/**
 * @brief Configures D-Pad navigation for a 2D grid instead of a 1D list.
 *        Up/Down navigates vertically by `columns`, Left/Right navigates by 1.
 * @param columns Number of columns in the grid.
 */
void NativeUI_PushGrid(int columns);

/**
 * @brief Pops the grid configuration, reverting to standard 1D list navigation.
 */
void NativeUI_PopGrid(void);

/**
 * @brief Replaces the legacy manual Menu_Cursor_Y[0] switch.
 *        Polls standard input masks and advances the internal Focus state
 *        up or down based on the layout list.
 * @param pad_input The 16-bit pad mask (e.g., from Check_Menu_Lever)
 * @param io_result The confirmation mask (e.g., IO_Result)
 */
void NativeUI_ProcessInput(uint16_t pad_input, uint16_t io_result);

#ifdef __cplusplus
}
#endif

#endif // NATIVE_IMGUI_H
