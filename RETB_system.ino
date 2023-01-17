#include "src/date.h"
#include "src/version.h"
#include "src/macros.h"
#include "src/debounceClass.h"

#include <painlessMesh.h>

#define   MESH_PREFIX     "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555



painlessMesh mesh ;
//Scheduler userScheduler; // to control your personal task

/* README
    if new node is connection, atleast 1 broadcast of token states is to be issued
    this means that every node will transmitt this

    if a node broadcasts new token info, all nodes must update LEDs

*/

// #define wrong_led

bool  connected             =    0 ;
const uint32_t R1           = 1000 ;
const uint32_t R2           = 4700 ;
const uint32_t threshold[]  =
{   
     /*4 * 1023 * R1 / ( R2 + ( 4 * R1) ) , // 470 ->*/ 682,
     /*3 * 1023 * R1 / ( R2 + ( 3 * R1) ) , // 398 ->*/ 579,
     /*2 * 1023 * R1 / ( R2 + ( 2 * R1) ) , // 305 ->*/ 446,
     /*1 * 1023 * R1 / ( R2 + ( 1 * R1) ) , // 179 ->*/ 262,
     /*0 * 1023 * R1 / ( R2 + ( 0 * R1) ) , //   0   */   0,
} ;

const int nSections       = 5 ;

#ifdef wrong_led
const int   red[]         = { D0, D2, D4, D6, 3 } ;
const int green[]         = { D1, D3, D5, D7, 1 } ;
#else
const int green[]         = { D0, D2, D4, D6, 3 } ;
const int   red[]         = { D1, D3, D5, D7, 1 } ;
#endif

const int switchesPin     = A0 ;
uint32_t timeOut[nSections] = {0,0,0,0} ;  
const int debugPin = 2 ;  // DEBUG TEST ME
const uint32_t timeOutInterval =  3000 ;
const uint32_t sendInterval    =  2000 ;

const uint32_t connectionTimeout = 10000 ;

enum tokenStates
{
    AVAILABLE,
    IN_POSSESSION,
    TAKEN,  
} ;
uint8_t token[ nSections ] ;

Debounce button[] =
{
    Debounce ( 255 ),
    Debounce ( 255 ),
    Debounce ( 255 ),
    Debounce ( 255 ),
    Debounce ( 255 )
} ;

/************** FUNCTIONS **************/

void updateLEDs()
{
    if( !connected )
    {
        REPEAT_MS( 500 )
        {
            for (int i = 0; i < nSections ; i++) digitalWrite( red[i], !digitalRead(red[i] )) ; // toggle all red lights during connecting
        }
        END_REPEAT
    }
    else for (int i = 0; i < nSections ; i++)
    {
        switch (token[i])
        {
        case     AVAILABLE: analogWrite(  green[i],   32 ) ;         // green
                            digitalWrite(   red[i],  LOW ) ; break;

        case IN_POSSESSION:analogWrite(    green[i],  32 ) ;         // yellow
                            digitalWrite(   red[i], HIGH ) ; break;
                           
        case         TAKEN: digitalWrite( green[i],  LOW ) ;         // red
                            digitalWrite(   red[i], HIGH ) ; break;

        }
    }
} ;

void newConnection(uint32_t nodeId)
{
    connected = 1 ;
}


void debounceInputs()
{
    REPEAT_MS( 50 )
    {
        int sample = analogRead( switchesPin ) ;

        for (int i = 0; i < nSections ; i++)
        {
            uint16_t ref ;
            if( threshold[i] >= 35 ) ref = threshold[i] ;
            else                     ref = 35 ; 
            if( sample >= ref - 35 
            &&  sample <= ref + 35 ) button[i].debounceInputs( 1 ) ;
            else                     button[i].debounceInputs( 0 ) ;
        }
    } END_REPEAT
}

void processInputs(  ) 
{
    for (int i = 0; i < nSections ; i++ )
    {
        String message = "" ;
        message +=  i ;
        message += ',' ;

        if( button[i].readInput() == FALLING )
        {
            if( token[i] == TAKEN ) { continue ; }  // token is claimed by another discard button press

            else if( token[i] == AVAILABLE )        // if the token is available.....
            {
                token[i] =  IN_POSSESSION ;         // claim the token
                message += TAKEN ;
            }
            else if( token[i] == IN_POSSESSION )    // if the token is in possession
            {
                token[i] =  AVAILABLE ;             // free up the token
                message += AVAILABLE ;     
            }
            mesh.sendBroadcast( message ) ; 
        }
    }
}

void transceiveTokens()
{
    static uint8_t index = 0 ;

    REPEAT_MS( sendInterval / nSections )                  // if we claimed atleast 1 token, transmitt this once every second
    {
        if( token[index] == IN_POSSESSION )
        {
            String message = "" ;
            message +=  index ;
            message += ',' ;
            message += TAKEN ;
            mesh.sendBroadcast( message ) ;   
        }
        if( ++ index == nSections ) index = 0 ;
    }
    END_REPEAT

    for (int i = 0; i < nSections ; i++ )
    {
        if( token[i] != IN_POSSESSION                   // if a node which claimed a token is turned off while still possessing the token
        &&  millis() - timeOut[i] >= timeOutInterval )  // the token becomes available again after a timeout
        {
            token[i] = AVAILABLE ;
        }
    }
}


void incommingMessage( uint32 from, String msg )
{
    uint32_t tokenState ;
    uint32_t tokenID ;
    char char_array[32];

    strcpy(char_array, msg.c_str());

    sscanf( char_array, "%d,%d", &tokenID, &tokenState ) ;

    if( token[ tokenID ] == IN_POSSESSION && tokenState == TAKEN ) // if we have the token and an other also claims the token...
    {                                                              // .. free the token again, and transmitt it. 
        token[ tokenID ] = AVAILABLE ;
        String message = "" ;
        message +=  tokenID ;
        message += ',' ;
        message += AVAILABLE ;
        mesh.sendBroadcast( message ) ;
    }
    if( tokenState == AVAILABLE )
    {
        token[ tokenID ] = AVAILABLE ;
    }

    token[tokenID] = tokenState ; // update token with state 
    if( token[tokenID] == TAKEN )
    {
        timeOut[tokenID] = millis() ; // set timeout
    }
}

void setup()
{
    debounceInputs() ; // to be sure
    
    mesh.init( MESH_PREFIX, MESH_PASSWORD, MESH_PORT );
    mesh.onReceive(&incommingMessage );
    mesh.onNewConnection( &newConnection );
    
    for( int i = 0 ; i < nSections ; i ++ )
    {
        pinMode( green[i], OUTPUT ) ;
        pinMode(   red[i], OUTPUT ) ;
        digitalWrite( green[i], LOW ) ;
        digitalWrite(   red[i], LOW ) ;
    }   
}

void loop()
{
    debounceInputs() ;
    processInputs( ) ;
    updateLEDs() ;
    transceiveTokens() ;
    mesh.update() ;

    if( millis() > connectionTimeout )
    {
        connected = 1 ;
    }
}

