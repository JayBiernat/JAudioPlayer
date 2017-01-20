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

#include "portaudio.h"
#include "JAudioPlayer.h"

int main( void )
{
    JAudioPlayer myAudioPlayer;

    JAudioPlayerCreate( &myAudioPlayer );
    if( myAudioPlayer.state == JPLAYER_FAILED_INITIALIZATION )
        return 1;
    printf( "Audio Player Created\n" );

    JAudioPlayerPlay( &myAudioPlayer );
    printf( "Audio Player Playing\n" );

    Sleep( 5000 );

    printf( "Stopping Audio Player\n" );
    JAudioPlayerStop( &myAudioPlayer );
    printf( "Audio Player Stopped\n" );

    JAudioPlayerDestroy( &myAudioPlayer );
    printf( "Audio Player Destroyed\n" );
    printf( "Test finished.\n" );

    return 0;
}

