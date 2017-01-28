/* main.c
 * Author: Jay Biernat
 *
 * Contains main routine of JAudioPlayer
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <process.h>
#include <math.h>

#include <portaudio.h>
#include <SDL.h>

#include "JAudioPlayer.h"

int main( int argc, char* argv[] )
{
    JAudioPlayer    *myAudioPlayer;
    JPlayerGUI      *myPlayerGUI;
    SDL_Event       event;
    int             bQuit = FALSE;

    if( argc != 2 )
    {
        printf( "ERROR: Not enough input arguments\n"
                "Usage: %s audio_file\n", argv[0] );
        return 1;
    }

    printf( "Creating audio player...\n" );
    myAudioPlayer = JAudioPlayerCreate( argv[1] );
    if( myAudioPlayer == NULL )
    {
        printf( "Failed to create audio player!\n" );
        return 1;
    }

    printf( "Creating audio player GUI...\n" );
    myPlayerGUI = JPlayerGUICreate();
    if( myPlayerGUI == NULL )
    {
        printf( "Failed to create audio player GUI!\n" );
        JAudioPlayerDestroy( &myAudioPlayer );
        return 1;
    }

    JAudioPlayerPlay( myAudioPlayer );
    printf( "Audio Player Playing\n" );
    printf( "  To quit, exit out of the J Audio Player window\n" );

    while( !bQuit )
    {
        while( SDL_PollEvent( &event) != 0 )
        {
            JPlayerGUICursorSate cursorState = JPlayerGUIGetCursorState();
            if( cursorState == CURSOR_ON_BACKGROUND )
                myPlayerGUI->buttonState = NO_BUTTON_PRESSED;

            if( ( event.type == SDL_MOUSEBUTTONDOWN ) &&
                ( event.button.button == SDL_BUTTON_LEFT ) )
            {
                switch( cursorState )
                {
                    case CURSOR_ON_PLAY_BUTTON:
                        myPlayerGUI->buttonState = PLAY_BUTTON_PRESSED;
                        break;
                    case CURSOR_ON_PAUSE_BUTTON:
                        myPlayerGUI->buttonState = PAUSE_BUTTON_PRESSED;
                        break;
                    case CURSOR_ON_STOP_BUTTON:
                        myPlayerGUI->buttonState = STOP_BUTTON_PRESSED;
                        break;
                    case CURSOR_ON_TRACK:
                        myPlayerGUI->seekerEngaged = TRUE;
                        break;
                    case CURSOR_ON_BACKGROUND:
                        break;
                }
            }
            else if( ( event.type == SDL_MOUSEBUTTONUP ) &&
                     ( event.button.button == SDL_BUTTON_LEFT ) )
            {
                if( myPlayerGUI->buttonState == PLAY_BUTTON_PRESSED )
                    JAudioPlayerPlay( myAudioPlayer );
                else if( myPlayerGUI->buttonState == STOP_BUTTON_PRESSED )
                    JAudioPlayerStop( myAudioPlayer );
                else if( myPlayerGUI->buttonState == PAUSE_BUTTON_PRESSED )
                    JAudioPlayerPause( myAudioPlayer );

                myPlayerGUI->buttonState = NO_BUTTON_PRESSED;

                if( myPlayerGUI->seekerEngaged )
                {
                    int x;
                    sf_count_t frameOffset;

                    SDL_GetMouseState( &x, NULL );
                    x = ( x > 349 ? 349 : x );
                    x = ( x < 49 ? 49 : x );

                    frameOffset = (sf_count_t)( ((float)(x - 49) / 300.0) * (float)myAudioPlayer->sfInfo.frames );
                    JAudioPlayerSeek( myAudioPlayer, frameOffset, SEEK_SET );
                    myPlayerGUI->seekerEngaged = FALSE;
                }
            }
            else if( event.type == SDL_QUIT )
                bQuit = TRUE;
        }
        if( myPlayerGUI->seekerEngaged )
        {
            int x;
            SDL_GetMouseState( &x, NULL );
            x = ( x > 349 ? 349 : x );
            x = ( x < 49 ? 49 : x );
            JPlayerGUIDraw( myPlayerGUI, (float)(x - 49) / 300.0 );
        }
        else
            JPlayerGUIDraw( myPlayerGUI, (float)myAudioPlayer->seekFrames / (float)myAudioPlayer->sfInfo.frames );
    }

    JPlayerGUIDestroy( &myPlayerGUI );
    printf("Audio Player GUI Destroyed\n" );
    JAudioPlayerDestroy( &myAudioPlayer );
    printf( "Audio Player Destroyed\n" );
    printf( "Test finished.\n" );

    return 0;
}
