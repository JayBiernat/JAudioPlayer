/* JPlayerGUI.h
 * Author: Jay Biernat
 *
 * Header file for structures and routines specific to the GUI of
 * JAudioPlayer
 *
 */

#ifndef JPLAYERGUI_H_INCLUDED
#define JPLAYERGUI_H_INCLUDED

#include <SDL.h>

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define WINDOW_HEIGHT 200
#define WINDOW_WIDTH 400

/** An enumerated type to be used in JPlayerGUIDraw to show which button has been pressed
  * @see JPlayerGUIDraw
  */
typedef enum
{
    NO_BUTTON_PRESSED,
    PLAY_BUTTON_PRESSED,
    PAUSE_BUTTON_PRESSED,
    STOP_BUTTON_PRESSED
}
JPlayerGUIButtonState;

/** An enumerated type showing which area the cursor is in within the GUI */
typedef enum
{
    CURSOR_ON_BACKGROUND,
    CURSOR_ON_PLAY_BUTTON,
    CURSOR_ON_PAUSE_BUTTON,
    CURSOR_ON_STOP_BUTTON,
    CURSOR_ON_TRACK
}
JPlayerGUICursorSate;

/** Structure holding window, renderer, and textures for SDL handling of GUI */
typedef struct
{
    SDL_Window      *window;          /* The window to be rendered to */
    SDL_Renderer    *renderer;        /* Texture Renderer */
    SDL_Texture     *texture_background;
    SDL_Texture     *texture_play;
    SDL_Texture     *texture_stop;
    SDL_Texture     *texture_pause;
    SDL_Texture     *texture_tracker;

    JPlayerGUIButtonState   buttonState;    /* Which button, if any, is down */
    int                     seekerEngaged;  /* Is the user moving the seeker */
}
JPlayerGUI;

/** Constant SDL_rect types idenitfying placement of buttons */
extern const SDL_Rect ButtonUnpressed, ButtonPressed, StopButtonPos, PlayButtonPos, PauseButtonPos;

/** @brief Initializes the GUI using SDL.  This function initializes the SDL library,
  * allowing the use of other SDL functions after JPlayerGUICreate is called.
  * JPlayerGUIDestroy must be called to free resources and quit the SDL library.
  * @return Pointer to an initialized JPlayerGUI, returns NULL on failure
  */
JPlayerGUI* JPlayerGUICreate( void );

/** @brief Renders the audio player GUI
  * @param playerGUI Pointer to an initialized JPlayerGUI object
  * @param buttonState A value from enumerated type JPlayerGUIButtonState showing
  * which button is pressed on the player
  * @param audioCompletion A float from 0.0 to 1.0 indicating how much of an audio
  * file has been played, which will determine the placement of the time tracker
  */
void JPlayerGUIDraw( JPlayerGUI *playerGUI, float audioCompletion );

/** @brief Returns cursor state (which part of the GUI the cursor is over)
  * @return cursorState Enumerated type JPlayerGUICursorState
  */
JPlayerGUICursorSate JPlayerGUIGetCursorState( void );

/** @brief Frees resources used by SDL GUI and quits SDL library
  * @param playerGUI Pointer to a pointer to a JPlayerGUI structure. Pointer to
  * the JPlayerGUI will be set to NULL after being destroyed.
  */
void JPlayerGUIDestroy( JPlayerGUI **playerGUIPtr );


#endif // JPLAYERGUI_H_INCLUDED
