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
#include <SDL.h>
#include "sndfile.h"
#include "JAudioPlayer.h"

const SDL_Rect ButtonUnpressed = { 0, 0, 50, 50 };
const SDL_Rect ButtonPressed = { 0, 50, 50, 50 };
const SDL_Rect StopButtonPos = { 250, 125, 50, 50 };
const SDL_Rect PlayButtonPos = { 100, 125, 50, 50 };
const SDL_Rect PauseButtonPos = { 175, 125, 50, 50 };

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
    audioPlayer->changeSeek = FALSE;
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
                                                           0,
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
        case JPLAYER_PLAYING:   /* Fall through all cases */
        case JPLAYER_PAUSED:
            JAudioPlayerStop( audioPlayer );
        case JPLAYER_STOPPED:
            Pa_CloseStream( audioPlayer->stream );
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


JPlayerGUI* JPlayerGUICreate( void )
{
    JPlayerGUI  *playerGUI = NULL;
    const char  backgroundPath[] = "assets\\PlayerBackground.bmp";
    const char  playButtonPath[] = "assets\\PlayButton.bmp";
    const char  stopButtonPath[] = "assets\\StopButton.bmp";
    const char  pauseButtonPath[] = "assets\\PauseButton.bmp";
    const char  timeTrackerPath[] = "assets\\TimeTracker.bmp";

    SDL_Surface *BMPSurface = NULL;        /* Loaded BMP image */
    SDL_Rect    trackerPos = { 44, 94, 13, 13 };

    playerGUI = (JPlayerGUI*)malloc( sizeof(JPlayerGUI) );
    if( playerGUI == NULL )
        return NULL;

    if( SDL_Init( SDL_INIT_VIDEO ) < 0 )    /* Initialize SDL library */
    {
        printf( "  ERROR: SDL could not initialize! SDL Error: %s\n", SDL_GetError() );
        free( playerGUI );
        return NULL;
    }

    playerGUI->buttonState = NO_BUTTON_PRESSED;

    /* Create window */
    playerGUI->window = SDL_CreateWindow( "J Audio Player",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          WINDOW_WIDTH,
                                          WINDOW_HEIGHT,
                                          SDL_WINDOW_SHOWN );
    if( playerGUI->window == NULL )
    {
        printf( "  ERROR: Window could not be created! SDL Error: %s\n", SDL_GetError() );
        SDL_Quit();
        free( playerGUI );
        return NULL;
    }

    /* Create renderer for window */
    playerGUI->renderer = SDL_CreateRenderer( playerGUI->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC );
    if( playerGUI->renderer == NULL )
    {
        printf( "  ERROR: Renderer could not be created! SDL Error: %s\n", SDL_GetError() );
        SDL_DestroyWindow( playerGUI->window );
        SDL_Quit();
        free( playerGUI );
        return NULL;
    }

    /* Load background texture */
    BMPSurface = SDL_LoadBMP( backgroundPath );
    if( BMPSurface == NULL )
    {
        printf( "  Warning: Unable to load background image!\n  SDL_LoadBMP Error: %s\n", SDL_GetError() );
        playerGUI->texture_background = NULL;
    }
    else
    {
        playerGUI->texture_background = SDL_CreateTextureFromSurface( playerGUI->renderer, BMPSurface );
        if( playerGUI->texture_background == NULL )
            printf( "  Warning: Unable to create background texture! SDL Error: %s\n", SDL_GetError() );
    }

    /* Load three buttons and tracker textures */
    /* Play Button */
    SDL_FreeSurface( BMPSurface );
    BMPSurface = SDL_LoadBMP( playButtonPath );
    if( BMPSurface == NULL )
    {
        printf( "  Warning: Unable to load play button image!\n  SDL_LoadBMP Error: %s\n", SDL_GetError() );
        playerGUI->texture_play = NULL;
    }
    else
    {
        playerGUI->texture_play = SDL_CreateTextureFromSurface( playerGUI->renderer, BMPSurface );
        if( playerGUI->texture_play == NULL )
            printf( "  Warning: Unable to create play button texture! SDL Error: %s\n", SDL_GetError() );
    }

    /* Stop Button */
    SDL_FreeSurface( BMPSurface );
    BMPSurface = SDL_LoadBMP( stopButtonPath );
    if( BMPSurface == NULL )
    {
        printf( "  Warning: Unable to load stop button image!\n  SDL_LoadBMP Error: %s\n", SDL_GetError() );
        playerGUI->texture_stop = NULL;
    }
    else
    {
        playerGUI->texture_stop = SDL_CreateTextureFromSurface( playerGUI->renderer, BMPSurface );
        if( playerGUI->texture_stop == NULL )
            printf( "  Warning: Unable to create stop button texture! SDL Error: %s\n", SDL_GetError() );
    }

    /* Pause Button */
    SDL_FreeSurface( BMPSurface );
    BMPSurface = SDL_LoadBMP( pauseButtonPath );
    if( BMPSurface == NULL )
    {
        printf( "  Warning: Unable to load pause button image!\n  SDL_LoadBMP Error: %s\n", SDL_GetError() );
        playerGUI->texture_pause = NULL;
    }
    else
    {
        playerGUI->texture_pause = SDL_CreateTextureFromSurface( playerGUI->renderer, BMPSurface );
        if( playerGUI->texture_pause == NULL )
            printf( "  Warning: Unable to create pause button texture! SDL Error: %s\n", SDL_GetError() );
    }

    /* Time tracker */
    SDL_FreeSurface( BMPSurface );
    BMPSurface = SDL_LoadBMP( timeTrackerPath );
    if( BMPSurface == NULL )
    {
        printf( "  Warning: Unable to load time tracker image!\n  SDL_LoadBMP Error: %s\n", SDL_GetError() );
        playerGUI->texture_tracker = NULL;
    }
    else
    {
        playerGUI->texture_tracker = SDL_CreateTextureFromSurface( playerGUI->renderer, BMPSurface );
        if( playerGUI->texture_tracker == NULL )
            printf( "  Warning: Unable to create time tracker texture! SDL Error: %s\n", SDL_GetError() );
    }
    SDL_FreeSurface( BMPSurface );

    /* Draw audio player GUI */
    SDL_SetRenderDrawColor( playerGUI->renderer, 0xFF, 0xFF, 0xFF, 0xFF );
    SDL_RenderClear( playerGUI->renderer );

    if( SDL_RenderCopy( playerGUI->renderer, playerGUI->texture_background, NULL, NULL ) < 0 )   /* background */
        printf( "  ERRROR: There was an error copying background texture! SDL Error: %s\n", SDL_GetError() );
    if( SDL_RenderCopy( playerGUI->renderer, playerGUI->texture_play, &ButtonUnpressed, &PlayButtonPos ) < 0 )   /* play button */
        printf( "  ERRROR: There was an error copying play button texture! SDL Error: %s\n", SDL_GetError() );
    if( SDL_RenderCopy( playerGUI->renderer, playerGUI->texture_stop, &ButtonUnpressed, &StopButtonPos ) < 0 )   /* stop button */
        printf( "  ERRROR: There was an error copying stop button texture! SDL Error: %s\n", SDL_GetError() );
    if( SDL_RenderCopy( playerGUI->renderer, playerGUI->texture_pause, &ButtonUnpressed, &PauseButtonPos ) < 0 )   /* pause button */
        printf( "  ERRROR: There was an error copying pause button texture! SDL Error: %s\n", SDL_GetError() );
    if( SDL_RenderCopy( playerGUI->renderer, playerGUI->texture_tracker, NULL, &trackerPos ) < 0 )   /* time tracker */
        printf( "  ERRROR: There was an error copying time tracker texture! SDL Error: %s\n", SDL_GetError() );

    SDL_RenderPresent( playerGUI->renderer );

    return playerGUI;
}


void JPlayerGUIDraw( JPlayerGUI *playerGUI, float audioCompletion )
{
    SDL_Rect TrackerPos = { 44 + (int)(300.0 * audioCompletion), 94, 13, 13 };

    SDL_RenderClear( playerGUI->renderer );
    SDL_RenderCopy( playerGUI->renderer, playerGUI->texture_background, NULL, NULL );
    SDL_RenderCopy( playerGUI->renderer, playerGUI->texture_tracker, NULL, &TrackerPos );

    switch( playerGUI->buttonState )
    {
        case NO_BUTTON_PRESSED:
            SDL_RenderCopy( playerGUI->renderer, playerGUI->texture_play, &ButtonUnpressed, &PlayButtonPos );
            SDL_RenderCopy( playerGUI->renderer, playerGUI->texture_stop, &ButtonUnpressed, &StopButtonPos );
            SDL_RenderCopy( playerGUI->renderer, playerGUI->texture_pause, &ButtonUnpressed, &PauseButtonPos );
            break;
        case PLAY_BUTTON_PRESSED:
            SDL_RenderCopy( playerGUI->renderer, playerGUI->texture_play, &ButtonPressed, &PlayButtonPos );
            SDL_RenderCopy( playerGUI->renderer, playerGUI->texture_stop, &ButtonUnpressed, &StopButtonPos );
            SDL_RenderCopy( playerGUI->renderer, playerGUI->texture_pause, &ButtonUnpressed, &PauseButtonPos );
            break;
        case STOP_BUTTON_PRESSED:
            SDL_RenderCopy( playerGUI->renderer, playerGUI->texture_play, &ButtonUnpressed, &PlayButtonPos );
            SDL_RenderCopy( playerGUI->renderer, playerGUI->texture_stop, &ButtonPressed, &StopButtonPos );
            SDL_RenderCopy( playerGUI->renderer, playerGUI->texture_pause, &ButtonUnpressed, &PauseButtonPos );
            break;
        case PAUSE_BUTTON_PRESSED:
            SDL_RenderCopy( playerGUI->renderer, playerGUI->texture_play, &ButtonUnpressed, &PlayButtonPos );
            SDL_RenderCopy( playerGUI->renderer, playerGUI->texture_stop, &ButtonUnpressed, &StopButtonPos );
            SDL_RenderCopy( playerGUI->renderer, playerGUI->texture_pause, &ButtonPressed, &PauseButtonPos );
            break;
    }

    SDL_RenderPresent( playerGUI->renderer );

    return;
}


JPlayerGUICursorSate JPlayerGUIGetCursorState( void )
{
    int x, y;

    SDL_GetMouseState( &x, &y );

    if( x >= 100 && x < 150 && y >= 125 && y < 175 )
        return CURSOR_ON_PLAY_BUTTON;
    else if( x >= 175 && x < 225 && y >=125 && y < 175 )
        return CURSOR_ON_PAUSE_BUTTON;
    else if( x >= 250 && x < 300 && y >= 125 && y < 175 )
        return CURSOR_ON_STOP_BUTTON;
    else if( x >= 50 && x < 350 && y >= 98 && y < 102 )
        return CURSOR_ON_TRACK;
    else
        return CURSOR_ON_BACKGROUND;
}


void JPlayerGUIDestroy( JPlayerGUI **playerGUIPtr )
{
    JPlayerGUI *playerGUI = *playerGUIPtr;

    if( playerGUI == NULL )
        return;

    SDL_DestroyTexture( playerGUI->texture_tracker );
    SDL_DestroyTexture( playerGUI->texture_background );
    SDL_DestroyTexture( playerGUI->texture_stop );
    SDL_DestroyTexture( playerGUI->texture_play );
    SDL_DestroyTexture( playerGUI->texture_pause );
    SDL_DestroyRenderer( playerGUI->renderer );
    SDL_DestroyWindow( playerGUI->window );
    SDL_Quit();

    free( playerGUI );
    *playerGUIPtr = NULL;

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
    const int       channels = audioPlayer->sfInfo.channels;

    const unsigned  MAX_PRODUCED_FRAMES = buffer->length_in_frames;
    sf_count_t      framesReadFromFile;

    while( !audioPlayer->bTimeToQuit )
    {
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
