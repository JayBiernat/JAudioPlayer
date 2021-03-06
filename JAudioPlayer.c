/* JAudioPlayer.c Contains routines pertaining to audio playback and audio
 * player object
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

#include <stdlib.h>
#include <stdio.h>

#ifdef WIN32
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#include <semaphore.h>
#include <time.h>   // timespec
#endif

#include "JAudioPlayer.h"

#ifdef WIN32
#define CLOSE_SYNCHRONIZATION_OBJECT CloseHandle( audioPlayer->audioBuffer.producerThreadEvent );
#define SIGNAL_SYNCHRONIZATION_OBJECT SetEvent( audioPlayer->audioBuffer.producerThreadEvent );
#else
#define CLOSE_SYNCHRONIZATION_OBJECT sem_destroy( &audioPlayer->audioBuffer.producerThreadSemaphore );
#define SIGNAL_SYNCHRONIZATION_OBJECT sem_post( &audioPlayer->audioBuffer.producerThreadSemaphore );
#endif

JAudioPlayer* JAudioPlayerCreate( const char *filePath )
{
    JAudioPlayer *audioPlayer = NULL;
    PaError err;
    int i;

    audioPlayer = (JAudioPlayer*)malloc( sizeof(JAudioPlayer) );
    if( audioPlayer == NULL )
        return NULL;
#ifdef WIN32
    audioPlayer->audioBuffer.producerThreadEvent = NULL;
#endif

    audioPlayer->bTimeToQuit = FALSE;

    /* Open soundfile and fill in sfInfo */
    audioPlayer->sfInfo.format = 0;     /* sndfile API requires format be set to zero before calling sf_open */
    audioPlayer->sfPtr = sf_open( filePath, SFM_READ, &audioPlayer->sfInfo );
    if( audioPlayer->sfPtr == NULL )
    {
        printf( "  Error: Could not open soundfile: %s\n", filePath );
        free( audioPlayer );
        return NULL;
    }
    audioPlayer->seekerInfo.bChangeSeek = FALSE;
    audioPlayer->seekFrames = 0;

    /* Set up audioBuffer */
    audioPlayer->audioBuffer.head = 0;
    audioPlayer->audioBuffer.tail = 0;
    audioPlayer->audioBuffer.availableBlocks = 0;
    audioPlayer->audioBuffer.num_blocks_in_buffer = MAX_BLOCKS;
    for( i=0; i<MAX_BLOCKS; i++ )
        audioPlayer->audioBuffer.blockPtrs[i] = NULL;

    for( i=0; i<MAX_BLOCKS; i++ )
    {
        audioPlayer->audioBuffer.blockPtrs[i] = (float*)malloc( sizeof(float) * FRAMES_PER_BLOCK * audioPlayer->sfInfo.channels );
        if( audioPlayer->audioBuffer.blockPtrs[i] == NULL )
        {
            printf( "  Error using malloc\n" );
            for( i=0; i<MAX_BLOCKS; i++ )
                free( audioPlayer->audioBuffer.blockPtrs[i] );
            sf_close( audioPlayer->sfPtr );
            free( audioPlayer );
            return NULL;
        }
    }

    /* Set up signaling object */
#ifdef WIN32
    audioPlayer->audioBuffer.producerThreadEvent = CreateEvent( NULL, /* bManualReset = */ FALSE, /* bInitialState = */ TRUE, NULL );
    if( audioPlayer->audioBuffer.producerThreadEvent == NULL )
#else
    if( sem_init( &audioPlayer->audioBuffer.producerThreadSemaphore, /* pshared = */ 0, /* value = */ 1 ) < 0 )
#endif
    {
        printf( "  Error: Cannot create synchronization object\n" );
        for( i=0; i<MAX_BLOCKS; i++ )
            free( audioPlayer->audioBuffer.blockPtrs[i] );
        sf_close( audioPlayer->sfPtr );
        free( audioPlayer );
        return NULL;
    }

    /* Initialize PortAudio and set up output stream */
    err = Pa_Initialize();
    if( err != paNoError )
    {
        printf( "  Error: Pa_Initialize\n" );
        printf( "  Error number: %d\n", err );
        printf( "  Error message: %s\n", Pa_GetErrorText( err ) );
        CLOSE_SYNCHRONIZATION_OBJECT
        for( i=0; i<MAX_BLOCKS; i++ )
            free( audioPlayer->audioBuffer.blockPtrs[i] );
        sf_close( audioPlayer->sfPtr );
        free( audioPlayer );
        return NULL;
    }

    audioPlayer->outputParameters.device = Pa_GetDefaultOutputDevice();
    audioPlayer->outputParameters.channelCount = audioPlayer->sfInfo.channels;
    audioPlayer->outputParameters.sampleFormat = paFloat32;          /* 32 bit floating point output */
    audioPlayer->outputParameters.suggestedLatency = Pa_GetDeviceInfo( audioPlayer->outputParameters.device )->defaultLowOutputLatency;
    audioPlayer->outputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(
              &audioPlayer->stream,
              NULL, /* no input */
              &audioPlayer->outputParameters,
              audioPlayer->sfInfo.samplerate,
              FRAMES_PER_BLOCK,
              paNoFlag,
              paCallback,
              audioPlayer );
    if( err != paNoError )
    {
        printf( "  Error: Pa_OpenStream\n" );
        printf( "  Error number: %d\n", err );
        printf( "  Error message: %s\n", Pa_GetErrorText( err ) );
        Pa_Terminate();
        CLOSE_SYNCHRONIZATION_OBJECT
        for( i=0; i<MAX_BLOCKS; i++ )
            free( audioPlayer->audioBuffer.blockPtrs[i] );
        sf_close( audioPlayer->sfPtr );
        free( audioPlayer );
        return NULL;
    }
    audioPlayer->state = JPLAYER_STOPPED;

    /* Start producer thread */
#ifdef WIN32
    audioPlayer->handle_Producer = (HANDLE)_beginthreadex( NULL,
                                                           0,
                                                           audioBufferProducer,
                                                           audioPlayer,
                                                           0,
                                                           &audioPlayer->threadID_Producer );
    if( audioPlayer->handle_Producer == 0 )
#else
    if( pthread_create( &audioPlayer->threadID_Producer, NULL, audioBufferProducer, audioPlayer ) )
#endif
    {
        printf( "  Error creating producer thread\n" );
        Pa_CloseStream( audioPlayer->stream );
        Pa_Terminate();
        CLOSE_SYNCHRONIZATION_OBJECT
        for( i=0; i<MAX_BLOCKS; i++ )
            free( audioPlayer->audioBuffer.blockPtrs[i] );
        sf_close( audioPlayer->sfPtr );
        free( audioPlayer );
        return NULL;
    }
#ifdef WIN32
    SetThreadPriority( audioPlayer->handle_Producer, THREAD_PRIORITY_TIME_CRITICAL );
#endif

    return audioPlayer;
}


void JAudioPlayerPlay( JAudioPlayer *audioPlayer )
{
    PaError err;

    if( audioPlayer == NULL )
        return;

    switch( audioPlayer->state )
    {
        case JPLAYER_STOPPED:
            err = Pa_StartStream( audioPlayer->stream );
            if( err != paNoError )
            {
                printf( "  Error: Pa_StartStream\n" );
                printf( "  Error number: %d\n", err );
                printf( "  Error message: %s\n", Pa_GetErrorText( err ) );
            }
            else
                audioPlayer->state = JPLAYER_PLAYING;
            break;
        case JPLAYER_PAUSED:
            audioPlayer->state = JPLAYER_PLAYING;
            break;
        case JPLAYER_PLAYING:
            break;
    }
    return;
}


void JAudioPlayerPause( JAudioPlayer *audioPlayer )
{
    if( audioPlayer == NULL )
        return;

    switch( audioPlayer->state )
    {
        case JPLAYER_STOPPED:
        case JPLAYER_PAUSED:
            break;
        case JPLAYER_PLAYING:
            audioPlayer->state = JPLAYER_PAUSED;
            break;
    }
    return;
}


void JAudioPlayerStop( JAudioPlayer *audioPlayer )
{
    PaError err;

    if( audioPlayer == NULL )
        return;

    switch( audioPlayer->state )
    {
        case JPLAYER_STOPPED:
            break;
        case JPLAYER_PAUSED:
        case JPLAYER_PLAYING:
            err = Pa_StopStream( audioPlayer->stream );
            if( err != paNoError )
            {
                printf( "  Error: Pa_StopStream\n" );
                printf( "  Error number: %d\n", err );
                printf( "  Error message: %s\n", Pa_GetErrorText( err ) );
                return;
            }
            else
                audioPlayer->state = JPLAYER_STOPPED;

            JAudioPlayerSeek( audioPlayer, 0, SEEK_SET );
            break;
    }
    return;
}


inline void JAudioPlayerSeek( JAudioPlayer *audioPlayer, sf_count_t frames, int whence )
{
    audioPlayer->seekerInfo.frames = frames;
    audioPlayer->seekerInfo.whence = whence;

    audioPlayer->seekerInfo.bChangeSeek = TRUE;
    SIGNAL_SYNCHRONIZATION_OBJECT

    /* Wait for producer thread to signal seek cursor has been changed in audio
     * file by setting changeSeek back to FALSE */
    while( audioPlayer->seekerInfo.bChangeSeek );

    return;
}


void JAudioPlayerDestroy( JAudioPlayer **audioPlayerPtr )
{
    JAudioPlayer *audioPlayer = *audioPlayerPtr;
    int i;

    if( audioPlayer == NULL )
        return;

    switch( audioPlayer->state )
    {
        case JPLAYER_PLAYING:   /* Fall through all cases */
        case JPLAYER_PAUSED:
            Pa_StopStream( audioPlayer->stream );
        case JPLAYER_STOPPED:
            Pa_CloseStream( audioPlayer->stream );
            audioPlayer->bTimeToQuit = TRUE;
#ifdef WIN32
            WaitForSingleObject( audioPlayer->handle_Producer, 10000 );
            CloseHandle( audioPlayer->handle_Produer );
#else
            pthread_join( audioPlayer->threadID_Producer, NULL );
#endif
            Pa_Terminate();
            CLOSE_SYNCHRONIZATION_OBJECT
            for( i=0; i<MAX_BLOCKS; i++ )
                free( audioPlayer->audioBuffer.blockPtrs[i] );
            sf_close( audioPlayer->sfPtr );
            free( audioPlayer );
            *audioPlayerPtr = NULL;
    }
    return;
}


int paCallback( const void                      *input,
                void                            *output,
                unsigned long                   frameCount,
                const PaStreamCallbackTimeInfo  *timeInfo,
                PaStreamCallbackFlags           statusFlags,
                void                            *userData )
{
    (void)input;        /* Prevent unused variable warning */
    float *out = (float*)output;
    JAudioPlayer *audioPlayer = (JAudioPlayer*)userData;

    JCircularBuffer *buffer = &audioPlayer->audioBuffer;

    const int   channels = audioPlayer->sfInfo.channels;
    unsigned    i, j;

    if( audioPlayer->state == JPLAYER_PAUSED )
    {
        for( i=0; i<FRAMES_PER_BLOCK; i++ )
        {
            for( j=0; j<channels; j++ )
                *out++ = 0;
        }
    }
    else
    {
        while( buffer->availableBlocks < 1 );

        for( i=0; i<FRAMES_PER_BLOCK; i++ )
        {
            for( j=0; j<channels; j++ )
            {
                *out++ = *( buffer->blockPtrs[buffer->tail] + (i * channels) + j );
            }
        }
        __sync_fetch_and_sub( &buffer->availableBlocks, 1 );
        if( ++(buffer->tail) >= buffer->num_blocks_in_buffer )
            buffer->tail = 0;
        SIGNAL_SYNCHRONIZATION_OBJECT
    }

    return paContinue;      /* return 0 */
}


THREAD_ROUTINE_SIGNATURE audioBufferProducer( void *threadArg )
{
    JAudioPlayer    *audioPlayer = (JAudioPlayer*)threadArg;
    JCircularBuffer *buffer = &audioPlayer->audioBuffer;
    JChangeSeekInfo *seekerInfo = &audioPlayer->seekerInfo;
    const int       channels = audioPlayer->sfInfo.channels;

    sf_count_t      framesReadFromFile;
    int             blocksNeeded, n;

    while( !audioPlayer->bTimeToQuit )
    {
#ifdef WIN32
        WaitForSingleObject( buffer->producerThreadEvent, 1000 );
#else
        static struct timespec waitTime = { 1, 0 };
        sem_timedwait( &buffer->producerThreadSemaphore, &waitTime );
#endif

        /* Reset seek cursor in audio file if needed */
        if( seekerInfo->bChangeSeek )
        {
            sf_count_t frameOffset;
            if( ( frameOffset = sf_seek( audioPlayer->sfPtr, seekerInfo->frames, seekerInfo->whence ) ) >= 0 )
                audioPlayer->seekFrames = frameOffset;

            seekerInfo->bChangeSeek = FALSE;
        }

        blocksNeeded = buffer->num_blocks_in_buffer - buffer->availableBlocks;
        for( n=0; n<blocksNeeded; n++ )
        {
            framesReadFromFile = sf_readf_float( audioPlayer->sfPtr,
                                                 buffer->blockPtrs[buffer->head],
                                                 FRAMES_PER_BLOCK );
            audioPlayer->seekFrames += framesReadFromFile;

            if( framesReadFromFile < FRAMES_PER_BLOCK )  /* Check frames read from file, produce silence after end of file */
            {
                unsigned    silentFramesToProduce = FRAMES_PER_BLOCK - framesReadFromFile;
                float       *ptr = buffer->blockPtrs[buffer->head] + ( framesReadFromFile * channels );
                unsigned    i, j;

                for( i=0; i<silentFramesToProduce; i++ )
                {
                    for( j=0; j<channels; j++ )
                        *ptr++ = 0;
                }
            }
            __sync_fetch_and_add( &(buffer->availableBlocks), 1 );  /* Protect against race condition with atomic operation */
            if( ++(buffer->head) >= buffer->num_blocks_in_buffer )
                buffer->head = 0;
        }
    }
#ifdef WIN32
    _endthreadex( 0 );
#endif
    return 0;
}
