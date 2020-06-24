/*
 vx8nmea sketch - translates GPS output to GPS input for radio.  Save about $400.

 This sketch is designed to run on a Trinket M0.  The GPS TX output is connected to the Trinket's RX port 
 and the Trinket's TX port is connected to the radio.  We ignore the radio's TX.

 GPS Module is: MTK3339 G.top
 
 */


#include <Adafruit_DotStar.h>

enum
{
    kMessageID,
    kUTCTime,
    kLatitude,
    kNSIndicator,
    kLongitude,
    kEWIndicator,
    kPositionFix,
    kSatellitesUsed,
    kHDOP,
    kAltiude,
    kAltUnit,
    kGeoidalSeperation,
    kGSUnit,
    kAgeOfDiffCorrection,
    kChecksum
};


enum
{
    kRMCMessageID,
    kRMCUTCTime,
    kRMCStatus,
    kRMCLatitude,
    kRMCNSIndicator,
    kRMCLongitude,
    kRMCEWIndicator,
    kRMCSpeed,
    kRMCCourse,
    kRMCDate,
    kRMCMagenticVariation,
    kRMCMode,
    kRMCChecksum
};

enum
{
    kRMCMessageID_width         = 6,
    kRMCUTCTime_width           = 10,
    kRMCStatus_width            = 1,
    kRMCLatitude_width          = 9,
    kRMCNSIndicator_width       = 1,
    kRMCLongitude_width         = 10,
    kRMCEWIndicator_width       = 1,
    kRMCSpeed_width             = 7,
    kRMCCourse_width            = 6,
    kRMCDate_width              = 6,
    kRMCMagenticVariation_width = 0,
    kRMCMode_width              = 0,
    kRMCChecksum_width          = 2
};

// these line up with the RMC message enums, these are the widths of each field
const int kPaddingArray[] = 
{
    kRMCMessageID_width,
    kRMCUTCTime_width,
    kRMCStatus_width,
    kRMCLatitude_width,
    kRMCNSIndicator_width,
    kRMCLongitude_width,
    kRMCEWIndicator_width,
    kRMCSpeed_width,
    kRMCCourse_width,
    kRMCDate_width,
    kRMCMagenticVariation_width,
    kRMCMode_width,
    kRMCChecksum_width
};

// When we are in production mode, we shut down the USB, and output
// on the same serial port we receive data on. GPS TX->Trinket RX, Trinket TX->Radio RX.
// When not in production mode, we output on the built-in USB for debugging. Trinket TX->USB serial monitor.
//#define PRODUCTION



#ifndef PRODUCTION
// for test
#define kInput  Serial1
#define kOutput Serial

#else

// for production
#define kInput  Serial1
#define kOutput Serial1
#endif

#define kSerialInputPin  3
#define kDotStarDataPin  7
#define kDotStarClockPin 8
Adafruit_DotStar strip( 1, kDotStarDataPin, kDotStarClockPin, DOTSTAR_BRG );

static char s_buffer[1024] = {};

static int  s_writeIndex = 0;
static int  s_comma      = 0;
static bool s_latch      = false;
static bool s_mute       = false;

static bool s_processingGGA = false;
static bool s_processingRMC = false;

static const char* kGGAPrefix = "$GPGGA";
static const char* kRMCPrefix = "$GPRMC";



bool checkForGPGGA( const char* check )
{
    return strncmp( check, kGGAPrefix, strlen( kGGAPrefix ) ) == 0;
}


bool checkForGPRMC( const char* check )
{
    return strncmp( check, kRMCPrefix, strlen( kRMCPrefix ) ) == 0;
}



void outputPaddedString( const char* string, int padded_size )
{
    if( !string )
        return;

    int input_size = strlen( string );
    int pad_amount = padded_size - input_size;

    if( pad_amount > 0 )
    {
        for( int i = 0; i < pad_amount; i++ )
            kOutput.write( '0' );
    }
    for( int i = 0; i < input_size; i++ )
        kOutput.write( string[i] );
}


void enter_Sleep( void )
{
    /* Configure low-power mode */
    SCB->SCR &= ~( SCB_SCR_SLEEPDEEP_Msk );  // low-power mode = sleep mode

    __DSB();
    __WFE();  // enter low-power mode - wait for event (not interrupt)
}



void setup() 
{
    kOutput.begin(9600);
#ifndef PRODUCTION
    kInput.begin(9600);
#endif
    s_writeIndex = 0;

    // we are turning off the dotstar here...to save power.
    strip.begin();
    strip.show();

    I2S->CTRLA.bit.ENABLE=0;
    ADC->CTRLA.bit.ENABLE=0;
    DAC->CTRLA.bit.ENABLE=0;
    AC->CTRLA.bit.ENABLE=0;

    PM->APBBMASK.reg &= ~PM_APBBMASK_PORT;    // you cannot control the built-in LED when this is off.
    PM->APBBMASK.reg &= ~PM_APBBMASK_DMAC;

    PM->APBCMASK.reg &= ~PM_APBCMASK_ADC;
    PM->APBCMASK.reg &= ~PM_APBCMASK_DAC;
    PM->APBCMASK.reg &= ~PM_APBCMASK_SERCOM3;
    PM->APBCMASK.reg &= ~PM_APBCMASK_SERCOM1;
    PM->APBCMASK.reg &= ~PM_APBCMASK_SERCOM2;

    // Disable USB port (to disconnect correctly from host)
#ifdef PRODUCTION
    USB->DEVICE.CTRLA.reg &= ~USB_CTRLA_ENABLE;
#endif

    // wake up when the serial pin does something
    EIC->WAKEUP.reg |= (1 << digitalPinToInterrupt(kSerialInputPin));
}


void loop() 
{
    while( kInput.available() )
    {
        int input = kInput.read();
        
        // process our buffer
        if( input == ',' )
        {
            s_buffer[s_writeIndex] = '\0';
            ++s_comma;
            s_writeIndex = 0;
            s_latch      = false;
            
            if( !s_processingGGA && checkForGPGGA( s_buffer ) )
            {
                s_processingGGA = true;
            }
            else if( !s_processingRMC && checkForGPRMC( s_buffer ) )
            {
                s_processingRMC = true;
                s_mute = true;
            }
        }
        else
          s_buffer[s_writeIndex++] = input;

        
        if( s_processingGGA )
        {
              if( s_comma == kAltiude )
              {
                  // do not write output until we have full field
                  s_mute = true;
              }
              else if( s_comma == kAltUnit )
              {
                  s_mute = false;

                  if( !s_latch )   
                  {
                      // we now have the full altitude field buffered up, so pad it while outputting
                      // needs to be rewritten from xx.y to 000xx.y (MSL Altitude)
                      kOutput.write( ',' );
                      outputPaddedString( s_buffer, 7 );
                      s_latch = true; // do this only once at the beginning
                  }
              }
        }

        // pad everything in between the time and magnetic variation...  Yaesu uses fixed positions for all this!
        if( s_processingRMC )
        {
            if( (s_comma > kRMCUTCTime) && (s_comma < kRMCChecksum) )
            {
                s_mute = true;
                
                if( !s_latch )   
                {
                    kOutput.write( ',' );
                    outputPaddedString( s_buffer, kPaddingArray[s_comma - 1] );
                    s_latch = true; // do this only once at the beginning
                }
            }
            else if( s_comma == kRMCChecksum )
                s_mute = false; // this way the rest of the line appears.
        }
          
        
        if( !s_mute )
            kOutput.write( input );
        
        if( input == '\n' )
        {
            s_writeIndex = 0;
            s_comma = 0;
            s_processingRMC = false;
            s_processingGGA = false;
            if( s_mute )
                kOutput.write( input ); // spit out that last newline...
            s_mute = false;
        }
    }
    
    enter_Sleep();
}

// EOF
