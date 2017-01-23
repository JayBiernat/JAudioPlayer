/* JAudioPlayer.h
 * Author: Jay Biernat
 *
 * Header file for structures and routines specific to JAudioPlayer
 *
 */

#ifndef JAUDIOPLAYER_H_INCLUDED
#define JAUDIOPLAYER_H_INCLUDED

#include <SDL.h>
#include "portaudio.h"
#include "math.h"

#ifndef TRUE
#def TRUE 1
#def FALSE 0
#endif

#ifndef PI
#define PI 3.1415926536
#endif // PI

#define SAMPLE_RATE 44100
#define NUM_CHANNELS 2
#define FRAMES_PER_BUFFER 256
#define CIRCULAR_BUFFER_LENGTH_SCALING 3

#define WINDOW_HEIGHT 200
#define WINDOW_WIDTH 400

#define TABLE_SIZE   (200)
typedef struct
{
    float wave[TABLE_SIZE];
    int phase;
}
waveTable;

/** State of the audio player - specifically what the state of the PsStream is */
typedef enum
{
    JPLAYER_STOPPED,    /* Producer thread put to sleep */
    JPLAYER_PAUSED,     /* Producer thread in a busy wait state */
    JPLAYER_PLAYING
}
JPlayerState;

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

/** Buffer used to transfer audio from file to output stream */
typedef struct
{
    float   *bufferPtr;
    unsigned head;
    unsigned tail;
    unsigned length;    /* In samples (floats) */
    volatile unsigned availableFrames;  /* Frames available for output */
}
JCircularBuffer;

/** Contains information used by PortAudio API and information used by the producer thread
  * @see JAudioPlayerCreate
  * @see JAudioPlayerStart
  * @see JAudioPlayerStop
  * @see JAudioPlayerDestroy
  */
typedef struct
{
    /* PortAudio variables */
    PaStreamParameters  outputParameters;
    PaStream            *stream;

    /* Buffer producer thread variables */
    HANDLE      handle_Producer;
    unsigned    threadID_Producer;
    volatile int  bTimeToQuit;        /* Flag signal time for thread shutdown */

    JCircularBuffer audioBuffer;
    JPlayerState    state;
}
JAudioPlayer;

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
}
JPlayerGUI;

/** Constant SDL_rect types idenitfying placement of buttons */
extern const SDL_Rect ButtonUnpressed, ButtonPressed, StopButtonPos, PlayButtonPos, PauseButtonPos;

/** @brief Initializes JAudioPlayer.  JAudioPlayerDestroy must be called to free
  * resources allocated by JAudioPlayerCreate.
  * @return Pointer to an initialized JAudioPlayer object, returns NULL on failure
  */
JAudioPlayer* JAudioPlayerCreate( void );

/** @brief Starts the playing the audio stream */
void JAudioPlayerPlay( JAudioPlayer *audioPlayer );

/** @brief Stops the audio stream */
void JAudioPlayerStop( JAudioPlayer *audioPlayer );

/** @brief Used to destroy JAudioPlayer initialized with JAudioPlayerCreate
  * @param audioPlayer Pointer to a pointer to a JAudioPlayer structure. Pointer to
  * the JAudioPlayer will be set to NULL after being destroyed.
  */
void JAudioPlayerDestroy( JAudioPlayer **audioPlayer );

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

/** @brief Routine used by PortAudio for audio handling */
int paCallback( const void *input,
                void *output,
                unsigned long frameCount,
                const PaStreamCallbackTimeInfo *timeInfo,
                PaStreamCallbackFlags statusFlags,
                void *userData );

/** @brief Routine for audio producer thread */
unsigned int __stdcall audioBufferProducer( void *threadArg );

#endif // JAUDIOPLAYER_H_INCLUDED
