/* JPlayerGUI.c Contains routines for audio player GUI
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

#include "JPlayerGUI.h"

const SDL_Rect ButtonUnpressed = { 0, 0, 50, 50 };
const SDL_Rect ButtonPressed = { 0, 50, 50, 50 };
const SDL_Rect StopButtonPos = { 250, 125, 50, 50 };
const SDL_Rect PlayButtonPos = { 100, 125, 50, 50 };
const SDL_Rect PauseButtonPos = { 175, 125, 50, 50 };

JPlayerGUI* JPlayerGUICreate( void )
{
    JPlayerGUI  *playerGUI = NULL;
#ifdef WIN32
    const char  backgroundPath[] = "assets\\PlayerBackground.bmp";
    const char  playButtonPath[] = "assets\\PlayButton.bmp";
    const char  stopButtonPath[] = "assets\\StopButton.bmp";
    const char  pauseButtonPath[] = "assets\\PauseButton.bmp";
    const char  timeTrackerPath[] = "assets\\TimeTracker.bmp";
#else
    const char  backgroundPath[] = "assets/PlayerBackground.bmp";
    const char  playButtonPath[] = "assets/PlayButton.bmp";
    const char  stopButtonPath[] = "assets/StopButton.bmp";
    const char  pauseButtonPath[] = "assets/PauseButton.bmp";
    const char  timeTrackerPath[] = "assets/TimeTracker.bmp";
#endif

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
    playerGUI->seekerEngaged = FALSE;

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
    else if( x >= 44 && x < 356 && y >= 94 && y < 106 )
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
