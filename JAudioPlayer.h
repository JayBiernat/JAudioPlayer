/* JAudioPlayer.h Header file for audio player object
 * Copyright (c) 2017 Jay Biernat
 *
 * This file is part of J Audio Player
 *
 * J Audio Player is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * J Audio Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with J Audio Player.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef JAUDIOPLAYER_H_INCLUDED
#define JAUDIOPLAYER_H_INCLUDED

#ifdef WIN32
#include <Windows.h>
#else
#include <pthread.h>
#include <semaphore.h>
#endif

#include "portaudio.h"
#include "sndfile.h"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define FRAMES_PER_BLOCK 256
#define MAX_BLOCKS 4

#ifdef WIN32
#define THREAD_ROUTINE_SIGNATURE unsigned int __stdcall
#else
#define THREAD_ROUTINE_SIGNATURE void*
#endif

/** State of the audio player - specifically what the state of the PaStream is */
typedef enum
{
    JPLAYER_STOPPED,    /* Producer thread put to sleep - not yet implemented */
    JPLAYER_PAUSED,     /* Producer thread in a busy wait state */
    JPLAYER_PLAYING
}
JPlayerState;

/** Buffer used to transfer audio from file to output stream */
typedef struct
{
    float       *blockPtrs[MAX_BLOCKS];
    unsigned    head;       /* Track position of head and tail in blocks */
    unsigned    tail;
    unsigned    num_blocks_in_buffer;
    volatile unsigned availableBlocks;  /* Blocks available for output */

#ifdef WIN32
    HANDLE      producerThreadEvent;    /* Event to signal that there is something
                                             * to do in the producer thread */
#else
    sem_t       producerThreadSemaphore;
#endif
}
JCircularBuffer;

/** Contains information needed to synchronize the seek cursor position
  * across threads
  * @see JAudioPlayerSeek
  */
typedef struct
{
    volatile int        bChangeSeek;
    volatile sf_count_t frames;
    volatile int        whence;
}
JChangeSeekInfo;

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

    /* sndfile API variables */
    SF_INFO          sfInfo;
    SNDFILE         *sfPtr;

    volatile sf_count_t seekFrames;
    JChangeSeekInfo     seekerInfo;

    /* Buffer producer thread variables */
#ifdef WIN32
    HANDLE          handle_Producer;
    unsigned        threadID_Producer;
#else
    pthread_t       threadID_Producer;
#endif

    volatile int    bTimeToQuit;        /* Flag signal time for thread shutdown */

    JCircularBuffer audioBuffer;
    JPlayerState    state;
}
JAudioPlayer;

/** @brief Initializes JAudioPlayer.  JAudioPlayerDestroy must be called to free
  * resources allocated by JAudioPlayerCreate.
  * @return Pointer to an initialized JAudioPlayer object, returns NULL on failure
  */
JAudioPlayer* JAudioPlayerCreate( const char *filePath );

/** @brief Starts the playing the audio stream */
void JAudioPlayerPlay( JAudioPlayer *audioPlayer );

/** @brief Pauses playback by outputting zeros while stream remains open */
void JAudioPlayerPause( JAudioPlayer *audioPlayer );

/** @brief Stops the audio stream */
void JAudioPlayerStop( JAudioPlayer *audioPlayer );

/** @brief Signals to set the cursor within the data section of the opened audio file.
  * Only returns when the producer thread has seen the request to change the seek cursor
  * position and changes it.
  * @param frames Offset of frames the cursor will be set to from the whence parameter
  * @param whence One of the values SEEK_SET (from beginning of data) SEEK_CUR (from
  * current location SEEK_END (fromt end of data)
  */
inline void JAudioPlayerSeek( JAudioPlayer *audioPlayer, sf_count_t frames, int whence );

/** @brief Used to destroy JAudioPlayer initialized with JAudioPlayerCreate
  * @param audioPlayer Pointer to a pointer to a JAudioPlayer structure. Pointer to
  * the JAudioPlayer will be set to NULL after being destroyed.
  */
void JAudioPlayerDestroy( JAudioPlayer **audioPlayer );

/** @brief Routine used by PortAudio for audio handling */
int paCallback( const void *input,
                void *output,
                unsigned long frameCount,
                const PaStreamCallbackTimeInfo *timeInfo,
                PaStreamCallbackFlags statusFlags,
                void *userData );

/** @brief Routine for audio producer thread */
THREAD_ROUTINE_SIGNATURE audioBufferProducer( void *threadArg );

#endif // JAUDIOPLAYER_H_INCLUDED
