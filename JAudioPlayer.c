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

#include "portaudio.h"
#include "JAudioPlayer.h"

JAudioPlayer* JAudioPlayerCreate( void )
{
    JAudioPlayer *audioPlayer = NULL;
    PaError err;

    audioPlayer = (JAudioPlayer*)malloc( sizeof(JAudioPlayer) );
    if( audioPlayer == NULL )
        return audioPlayer;

    audioPlayer->bTimeToQuit = FALSE;

    /* Set up audioBuffer */
    audioPlayer->audioBuffer.bufferPtr = NULL;
    audioPlayer->audioBuffer.length = NUM_CHANNELS * FRAMES_PER_BUFFER * CIRCULAR_BUFFER_LENGTH_SCALING;
    audioPlayer->audioBuffer.head = 0;
    audioPlayer->audioBuffer.tail = 0;
    audioPlayer->audioBuffer.availableFrames = 0;
    audioPlayer->audioBuffer.bufferPtr = (float*)malloc( sizeof(float) * audioPlayer->audioBuffer.length );
    if( audioPlayer->audioBuffer.bufferPtr == NULL )
    {
        printf( "Error using malloc\n" );
        free( audioPlayer );
        return NULL;
    }

    /* Initialize PortAudio and set up output stream */
    err = Pa_Initialize();
    if( err != paNoError )
    {
        printf( "Error: Pa_Initialize\n" );
        printf( "Error number: %d\n", err );
        printf( "Error message: %s\n", Pa_GetErrorText( err ) );
        free( audioPlayer->audioBuffer.bufferPtr );
        free( audioPlayer );
        return NULL;
    }

    audioPlayer->outputParameters.device = Pa_GetDefaultOutputDevice();
    audioPlayer->outputParameters.channelCount = NUM_CHANNELS;       /* STEREO output */
    audioPlayer->outputParameters.sampleFormat = paFloat32;          /* 32 bit floating point output */
    audioPlayer->outputParameters.suggestedLatency = Pa_GetDeviceInfo( audioPlayer->outputParameters.device )->defaultLowOutputLatency;
    audioPlayer->outputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(
              &audioPlayer->stream,
              NULL, /* no input */
              &audioPlayer->outputParameters,
              SAMPLE_RATE,
              FRAMES_PER_BUFFER,
              paNoFlag,
              paCallback,
              &audioPlayer->audioBuffer );
    if( err != paNoError )
    {
        printf( "Error: Pa_OpenStream\n" );
        printf( "Error number: %d\n", err );
        printf( "Error message: %s\n", Pa_GetErrorText( err ) );
        Pa_Terminate();
        free( audioPlayer->audioBuffer.bufferPtr );
        free( audioPlayer );
        return NULL;
    }
    audioPlayer->state = JPLAYER_STOPPED;

    /* Start producer thread */
    audioPlayer->handle_Producer = (HANDLE)_beginthreadex( NULL,
                                                           0,
                                                           audioBufferProducer,
                                                           audioPlayer,
                                                           0,
                                                           &audioPlayer->threadID_Producer );
    if( audioPlayer->handle_Producer == 0 )
    {
        printf( "Error creating producer thread\n" );
        Pa_CloseStream( audioPlayer->stream );
        Pa_Terminate();
        free( audioPlayer->audioBuffer.bufferPtr );
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
        case JPLAYER_PAUSED:    /* Fall through for now, will be different later */
            err = Pa_StartStream( audioPlayer->stream );
            if( err != paNoError )
            {
                printf( "Error: Pa_StartStream\n" );
                printf( "Error number: %d\n", err );
                printf( "Error message: %s\n", Pa_GetErrorText( err ) );
            }
            else
                audioPlayer->state = JPLAYER_PLAYING;
            break;
        case JPLAYER_PLAYING:
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
        case JPLAYER_PAUSED:
            break;
        case JPLAYER_PLAYING:
            err = Pa_StopStream( audioPlayer->stream );
            if( err != paNoError )
            {
                printf( "Error: Pa_StopStream\n" );
                printf( "Error number: %d\n", err );
                printf( "Error message: %s\n", Pa_GetErrorText( err ) );
            }
            else
                audioPlayer->state = JPLAYER_STOPPED;
    }
    return;
}


void JAudioPlayerDestroy( JAudioPlayer **audioPlayerPtr )
{
    JAudioPlayer *audioPlayer = *audioPlayerPtr;

    if( audioPlayer == NULL )
        return;

    switch( audioPlayer->state )
    {
        case JPLAYER_PLAYING:
            JAudioPlayerStop( audioPlayer );
        case JPLAYER_PAUSED:                        /* Fall through all cases */
        case JPLAYER_STOPPED:
            Pa_CloseStream( audioPlayer->stream );
            audioPlayer->bTimeToQuit = TRUE;
            WaitForSingleObject( audioPlayer->handle_Producer, 10000 );
            CloseHandle( audioPlayer->handle_Producer );
            Pa_Terminate();
            free( audioPlayer->audioBuffer.bufferPtr );
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
    JCircularBuffer *buffer = (JCircularBuffer*)userData;

    unsigned framesNeeded = FRAMES_PER_BUFFER;
    unsigned i;

    while( framesNeeded > 0 )
    {
        if( buffer->availableFrames > 0 )
        {
            unsigned framesToConsume = buffer->availableFrames;
            if( framesToConsume > framesNeeded )
                framesToConsume = framesNeeded;
            for( i=0; i<framesToConsume; i++ )
            {
                *out++ = *( buffer->bufferPtr + buffer->tail++ );
                *out++ = *( buffer->bufferPtr + buffer->tail++ );
                if( buffer->tail >= buffer->length )
                    buffer->tail -= buffer->length;
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
    const unsigned  MAX_PRODUCED_FRAMES = buffer->length / NUM_CHANNELS;
    waveTable       sineWave;
    unsigned        i;

    /* Initialize sine waveTable */
    for( i=0; i<TABLE_SIZE; i++ )
    {
        sineWave.wave[i] = (float) (0.8 * sin( ((float)i/(float)TABLE_SIZE) * PI * 2. ));
    }
    sineWave.phase = 0;

    while( !audioPlayer->bTimeToQuit )
    {
        if( buffer->availableFrames < MAX_PRODUCED_FRAMES )
        {
            unsigned framesToProduce = ( MAX_PRODUCED_FRAMES - buffer->availableFrames );
            for( i=0; i<framesToProduce; i++ )
            {
                *( buffer->bufferPtr + buffer->head++ ) = sineWave.wave[ sineWave.phase ];  /* Left and right channels have same phase */
                *( buffer->bufferPtr + buffer->head++ ) = sineWave.wave[ sineWave.phase++ ];
                if( buffer->head >= buffer->length )
                    buffer->head -= buffer->length;     /* Wrap index */
                if( sineWave.phase >= TABLE_SIZE )
                    sineWave.phase -= TABLE_SIZE;       /* Wrap index */
                __sync_fetch_and_add( &(buffer->availableFrames), 1 );  /* Protect against race condition with atomic operation */
            }
        }
    }

    _endthreadex( 0 );
    return 0;
}
