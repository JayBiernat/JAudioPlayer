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
    printf( "  To play, press 'p'\n"
            "  To stop, press 's'\n"
            "  To quit, press 'q'\n" );

    while( !bQuit )
    {
        while( SDL_PollEvent( &event) != 0 )
        {
            if( event.type == SDL_KEYDOWN )
            {
                switch( event.key.keysym.sym )
                {
                    case SDLK_p:
                        printf( "Got keyboard signal to play audio\n" );
                        JAudioPlayerPlay( myAudioPlayer );
                        break;
                    case SDLK_s:
                        printf( "Got keyboard signal to stop audio\n" );
                        JAudioPlayerStop( myAudioPlayer );
                        break;
                    case SDLK_q:
                        printf( "Got keyboard signal to quit\n" );
                        bQuit = TRUE;
                        break;
                }
            }
            else if( event.type == SDL_WINDOWEVENT )
            {
                if( event.window.event == SDL_WINDOWEVENT_RESTORED )
                {
                    /* Redraw default texture */
                    SDL_RenderClear( myPlayerGUI->renderer );
                    if( SDL_RenderCopy( myPlayerGUI->renderer,
                                        myPlayerGUI->texture_background,
                                        NULL,
                                        NULL ) < 0 )
                        printf( "ERRROR: There was an error copying texture! SDL Error: %s\n", SDL_GetError() );
                    SDL_RenderPresent( myPlayerGUI->renderer );
                }
            }
            else if( event.type == SDL_QUIT )
                bQuit = TRUE;
        }
    }

    JPlayerGUIDestroy( &myPlayerGUI );
    printf("Audio Player GUI Destroyed\n" );
    JAudioPlayerDestroy( &myAudioPlayer );
    printf( "Audio Player Destroyed\n" );
    printf( "Test finished.\n" );

    return 0;
}
