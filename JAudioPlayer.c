/* JAudioPlayer.c
 * Author: Jay Biernat
 *
 * Contains routines specific to JAudioPlayer
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <process.h>

#include <portaudio.h>
#include "sndfile.h"
#include "JAudioPlayer.h"

JAudioPlayer* JAudioPlayerCreate( const char *filePath )
{
    JAudioPlayer *audioPlayer = NULL;
    PaError err;

    audioPlayer = (JAudioPlayer*)malloc( sizeof(JAudioPlayer) );
    if( audioPlayer == NULL )
        return NULL;

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
    audioPlayer->audioBuffer.length_in_frames = FRAMES_PER_BLOCK * CIRCULAR_BUFFER_LENGTH_SCALING;
    audioPlayer->audioBuffer.head = 0;
    audioPlayer->audioBuffer.tail = 0;
    audioPlayer->audioBuffer.availableFrames = 0;
    audioPlayer->audioBuffer.bufferPtr = (float*)malloc( sizeof(float) * audioPlayer->audioBuffer.length_in_frames * audioPlayer->sfInfo.channels );
    if( audioPlayer->audioBuffer.bufferPtr == NULL )
    {
        printf( "  Error using malloc\n" );
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
        free( audioPlayer->audioBuffer.bufferPtr );
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
        free( audioPlayer->audioBuffer.bufferPtr );
        sf_close( audioPlayer->sfPtr );
        free( audioPlayer );
        return NULL;
    }
    audioPlayer->state = JPLAYER_STOPPED;

    /* Start producer thread */
    audioPlayer->handle_Producer = (HANDLE)_beginthreadex( NULL,
                                                           0,
                                                           audioBufferProducer,
                                                           audioPlayer,
                                                           CREATE_SUSPENDED,
                                                           &audioPlayer->threadID_Producer );
    if( audioPlayer->handle_Producer == 0 )
    {
        printf( "  Error creating producer thread\n" );
        Pa_CloseStream( audioPlayer->stream );
        Pa_Terminate();
        free( audioPlayer->audioBuffer.bufferPtr );
        sf_close( audioPlayer->sfPtr );
        free( audioPlayer );
        return NULL;
    }

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
            while( ResumeThread( audioPlayer->handle_Producer ) > 1 );
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
            while( ResumeThread( audioPlayer->handle_Producer ) > 1 );
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
            SuspendThread( audioPlayer->handle_Producer );  /* Don't need producer thread running */
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
            SuspendThread( audioPlayer->handle_Producer );
            break;
    }
    return;
}


inline void JAudioPlayerSeek( JAudioPlayer *audioPlayer, sf_count_t frames, int whence )
{
    audioPlayer->seekerInfo.frames = frames;
    audioPlayer->seekerInfo.whence = whence;

    /* Wait for producer thread to signal seek cursor has been changed in audio
     * file by setting changeSeek back to FALSE */
    audioPlayer->seekerInfo.bChangeSeek = TRUE;

    /* Must wake producer thread to process seek change in file if player is paused
     * or stopped */
    if( ( audioPlayer->state == JPLAYER_STOPPED ) ||
        ( audioPlayer->state == JPLAYER_PAUSED ) )
        while( ResumeThread( audioPlayer->handle_Producer ) > 1 );

    while( audioPlayer->seekerInfo.bChangeSeek );

    SuspendThread( audioPlayer->handle_Producer );

    return;
}


void JAudioPlayerDestroy( JAudioPlayer **audioPlayerPtr )
{
    JAudioPlayer *audioPlayer = *audioPlayerPtr;

    if( audioPlayer == NULL )
        return;

    switch( audioPlayer->state )
    {
        case JPLAYER_PLAYING:   /* Fall through all cases */
        case JPLAYER_PAUSED:
            Pa_StopStream( audioPlayer->stream );
        case JPLAYER_STOPPED:
            Pa_CloseStream( audioPlayer->stream );
            while( ResumeThread( audioPlayer->handle_Producer ) > 1 );
            audioPlayer->bTimeToQuit = TRUE;
            WaitForSingleObject( audioPlayer->handle_Producer, 10000 );
            CloseHandle( audioPlayer->handle_Producer );
            Pa_Terminate();
            free( audioPlayer->audioBuffer.bufferPtr );
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
    unsigned    framesNeeded = FRAMES_PER_BLOCK;
    unsigned    i, j;

    if( audioPlayer->state == JPLAYER_PAUSED )
    {
        for( i=0; i<FRAMES_PER_BLOCK; i++ )
        {
            for( j=0; j<channels; j++ )
                *out++ = 0;
        }
        return paContinue;
    }

    while( framesNeeded > 0 )
    {
        if( buffer->availableFrames > 0 )
        {
            unsigned framesToConsume = buffer->availableFrames;
            if( framesToConsume > framesNeeded )
                framesToConsume = framesNeeded;
            for( i=0; i<framesToConsume; i++ )
            {
                for( j=0; j<channels; j++ )
                {
                    *out++ = *(buffer->bufferPtr + (buffer->tail * channels) + j);
                }
                buffer->tail++;
                if( buffer->tail >= buffer->length_in_frames )
                    buffer->tail = 0;
                __sync_fetch_and_sub( &buffer->availableFrames, 1 );
            }
            framesNeeded -= framesToConsume;
        }
    }

    return paContinue;      /* return 0 */
}



/** For now just produce a sine wave for the audio player to play, eventually
  * will read from audio files
  */
unsigned int __stdcall audioBufferProducer( void *threadArg )
{
    JAudioPlayer    *audioPlayer = (JAudioPlayer*)threadArg;
    JCircularBuffer *buffer = &audioPlayer->audioBuffer;
    JChangeSeekInfo *seekerInfo = &audioPlayer->seekerInfo;
    const int       channels = audioPlayer->sfInfo.channels;

    const unsigned  MAX_PRODUCED_FRAMES = buffer->length_in_frames;
    sf_count_t      framesReadFromFile;

    while( !audioPlayer->bTimeToQuit )
    {
        /* Reset seek cursor in audio file if needed */
        if( seekerInfo->bChangeSeek )
        {
            sf_count_t frameOffset;
            if( ( frameOffset = sf_seek( audioPlayer->sfPtr, seekerInfo->frames, seekerInfo->whence ) ) >= 0 )
                audioPlayer->seekFrames = frameOffset;

            seekerInfo->bChangeSeek = FALSE;
        }

        if( buffer->availableFrames < MAX_PRODUCED_FRAMES )
        {
            unsigned framesToProduce = ( MAX_PRODUCED_FRAMES - buffer->availableFrames );
            if( buffer->head + framesToProduce > MAX_PRODUCED_FRAMES )
            {
                framesToProduce = MAX_PRODUCED_FRAMES - buffer->head;
                framesReadFromFile = sf_readf_float( audioPlayer->sfPtr, buffer->bufferPtr + (buffer->head * channels), (sf_count_t)framesToProduce );
                audioPlayer->seekFrames += framesReadFromFile;

                if( framesReadFromFile < framesToProduce )  /* Check frames read from file, produce silence after end of file */
                {
                    unsigned    silentFramesToProduce = framesToProduce - framesReadFromFile;
                    float       *ptr = buffer->bufferPtr + (buffer->head + framesReadFromFile) * channels;
                    unsigned    i, j;

                    for( i=0; i<silentFramesToProduce; i++ )
                    {
                        for( j=0; j<channels; j++ )
                            *ptr++ = 0;
                    }
                }
                __sync_fetch_and_add( &(buffer->availableFrames), framesToProduce );  /* Protect against race condition with atomic operation */
                buffer->head = 0;
            }
            else
            {
                framesReadFromFile = sf_readf_float( audioPlayer->sfPtr, buffer->bufferPtr + (buffer->head * channels), framesToProduce );
                audioPlayer->seekFrames += framesReadFromFile;

                if( framesReadFromFile < framesToProduce )  /* Check frames read from file, produce silence after end of file */
                {
                    unsigned    silentFramesToProduce = framesToProduce - framesReadFromFile;
                    float       *ptr = buffer->bufferPtr + (buffer->head + framesReadFromFile) * channels;
                    unsigned    i, j;

                    for( i=0; i<silentFramesToProduce; i++ )
                    {
                        for( j=0; j<channels; j++ )
                            *ptr++ = 0;
                    }
                }
                __sync_fetch_and_add( &(buffer->availableFrames), framesToProduce );  /* Protect against race condition with atomic operation */
                buffer->head += framesToProduce;
                if( buffer->head >= MAX_PRODUCED_FRAMES )
                    buffer->head = 0;
            }
        }
    }

    _endthreadex( 0 );
    return 0;
}
