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
    SDL_Event       event;
    int             bQuit = FALSE;

    const char defaultPath[] = "assets\\GUI_Placeholder.bmp";
    SDL_Window          *window = NULL;            /* The window to be rendered to */
    SDL_Surface         *convertedSurface = NULL;  /* The surface converted to the window's pixel format */
    SDL_Surface         *BMPSurface = NULL;        /* Loaded BMP image */
    SDL_Renderer        *renderer = NULL;          /* Texture Renderer */
    SDL_Texture         *texture;

    myAudioPlayer = JAudioPlayerCreate();
    if( myAudioPlayer == NULL )
    {
        printf( "Failed to create audio player\n" );
        return 1;
    }
    printf( "Audio Player Created\n" );
    printf( "Creating GUI\n" );

    if( SDL_Init( SDL_INIT_VIDEO ) < 0 )
    {
        printf( "SDL could not initialize! SDL Error: %s\n", SDL_GetError() );
        JAudioPlayerDestroy( &myAudioPlayer );
        return 1;
    }

    /* Load BMP image for Audio Player GUI */
    BMPSurface = SDL_LoadBMP( defaultPath );
    if( BMPSurface == NULL )
    {
        printf( "\n  Unable to load image default image!\n  SDL_LoadBMP Error: %s\n", SDL_GetError() );
        SDL_Quit();
        JAudioPlayerDestroy( &myAudioPlayer );
        return 1;
    }

    /* Create window */
    window = SDL_CreateWindow( "Image Processing",
                               SDL_WINDOWPOS_UNDEFINED,
                               SDL_WINDOWPOS_UNDEFINED,
                               BMPSurface->w,
                               BMPSurface->h,
                               SDL_WINDOW_SHOWN );
    if( window == NULL )
    {
        printf( "ERROR: Window could not be created! SDL Error: %s\n", SDL_GetError() );
        SDL_FreeSurface( BMPSurface );
        SDL_Quit();
        JAudioPlayerDestroy( &myAudioPlayer );
        return 1;
    }

    /* Create renderer for window */
    renderer = SDL_CreateRenderer( window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC );
    if( renderer == NULL )
    {
        printf( "ERROR: Renderer could not be created! SDL Error: %s\n", SDL_GetError() );
        SDL_DestroyWindow( window );
        SDL_FreeSurface( BMPSurface );
        SDL_Quit();
        JAudioPlayerDestroy( &myAudioPlayer );
        return 1;
    }

    /* Create surface formatted for window from loaded BMP surface */
    convertedSurface = SDL_ConvertSurface( BMPSurface, SDL_GetWindowSurface( window )->format, 0 );
    if( convertedSurface == NULL )
    {
        printf( "ERROR: Unable to convert surface to window format! SDL Error: %s\n", SDL_GetError() );
        SDL_DestroyRenderer( renderer );
        SDL_DestroyWindow( window );
        SDL_FreeSurface( BMPSurface );
        SDL_Quit();
        JAudioPlayerDestroy( &myAudioPlayer );
        return 1;
    }

    /* Creating streaming texture */
    texture = SDL_CreateTextureFromSurface( renderer, convertedSurface );
    if( texture == NULL )
    {
        printf( "ERROR: Unable to create texture from surface! SDL Error: %s\n", SDL_GetError() );
        SDL_FreeSurface( convertedSurface );
        SDL_DestroyRenderer( renderer );
        SDL_DestroyWindow( window );
        SDL_FreeSurface( BMPSurface );
        SDL_Quit();
        JAudioPlayerDestroy( &myAudioPlayer );
        return 1;
    }

    /* Free surfaces because we no longer need them */
    SDL_FreeSurface( convertedSurface );
    SDL_FreeSurface( BMPSurface );

    /* Clear screen */
    SDL_SetRenderDrawColor( renderer, 0xFF, 0xFF, 0xFF, 0xFF );
    SDL_RenderClear( renderer );

    /* Render updated texture */
    if( SDL_RenderCopy( renderer,
                        texture,
                        NULL,
                        NULL ) < 0 )
        printf( "ERRROR: There was an error copying texture! SDL Error: %s\n", SDL_GetError() );

    /* Update screen */
    SDL_RenderPresent( renderer );

    JAudioPlayerPlay( myAudioPlayer );
    printf( "Audio Player Playing\n" );
    printf( "  To play, press 'p'\n"
            "  To stop, press 's'\n"
            "  To quit, press 'q'\n" );

    while( !bQuit )
    {
        while( SDL_PollEvent( &event) != 0 )
        {
            printf( "Got Event\n" );
            if( event.type == SDL_KEYDOWN )
            {
                printf( "Got Keyboard event\n" );
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
        }
    }

    /* Clean up */
    SDL_DestroyTexture( texture );
    SDL_DestroyRenderer( renderer );
    SDL_DestroyWindow( window );
    SDL_Quit();

    JAudioPlayerDestroy( &myAudioPlayer );
    printf( "Audio Player Destroyed\n" );
    printf( "Test finished.\n" );

    return 0;
}

