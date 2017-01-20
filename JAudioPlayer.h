/* JAudioPlayer.h
 * Author: Jay Biernat
 *
 * Header file for structures and routines specific to JAudioPlayer
 *
 */

#ifndef JAUDIOPLAYER_H_INCLUDED
#define JAUDIOPLAYER_H_INCLUDED

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
    JPLAYER_FAILED_INITIALIZATION,
    JPLAYER_STOPPED,    /* Producer thread put to sleep */
    JPLAYER_PAUSED,     /* Producer thread in a busy wait state */
    JPLAYER_PLAYING,
    JPLAYER_DESTROYED
}
JPlayerState;

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

/** @brief Initializes JAudioPlayer
  * @param audioPlayer Pointer to a JAudioPlayer variable that is to be initialized
  * @return 0 if JAudioPlayer is initialized correctly, 1 if an error occured
  */
int JAudioPlayerCreate( JAudioPlayer *audioPlayer );

/** @brief Starts the playing the audio stream */
void JAudioPlayerPlay( JAudioPlayer *audioPlayer );

/** @brief Stops the audio stream */
void JAudioPlayerStop( JAudioPlayer *audioPlayer );

/** @brief Used to destroy JAudioPlayer initialized with JAudioPlayerCreate */
void JAudioPlayerDestroy( JAudioPlayer *audioPlayer );

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
