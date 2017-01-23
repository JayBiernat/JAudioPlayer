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

    printf( "Creating audio player...\n" );
    myAudioPlayer = JAudioPlayerCreate();
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
                if( cursorState == CURSOR_ON_PLAY_BUTTON )
                    myPlayerGUI->buttonState = PLAY_BUTTON_PRESSED;
                else if( cursorState == CURSOR_ON_PAUSE_BUTTON )
                    myPlayerGUI->buttonState = PAUSE_BUTTON_PRESSED;
                else if( cursorState == CURSOR_ON_STOP_BUTTON )
                    myPlayerGUI->buttonState = STOP_BUTTON_PRESSED;
            }
            else if( ( event.type == SDL_MOUSEBUTTONUP ) &&
                     ( event.button.button == SDL_BUTTON_LEFT ) )
            {
                if( myPlayerGUI->buttonState == PLAY_BUTTON_PRESSED )
                    JAudioPlayerPlay( myAudioPlayer );
                else if( myPlayerGUI->buttonState == STOP_BUTTON_PRESSED )
                    JAudioPlayerStop( myAudioPlayer );
                else if( myPlayerGUI->buttonState == PAUSE_BUTTON_PRESSED )     /* Add pause routine later */
                    JAudioPlayerStop( myAudioPlayer );

                myPlayerGUI->buttonState = NO_BUTTON_PRESSED;
            }
            else if( event.type == SDL_QUIT )
                bQuit = TRUE;

            JPlayerGUIDraw( myPlayerGUI, 0.0 );
        }
    }

    JPlayerGUIDestroy( &myPlayerGUI );
    printf("Audio Player GUI Destroyed\n" );
    JAudioPlayerDestroy( &myAudioPlayer );
    printf( "Audio Player Destroyed\n" );
    printf( "Test finished.\n" );

    return 0;
}
