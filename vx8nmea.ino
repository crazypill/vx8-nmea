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


#define PRODUCTION

#ifndef PRODUCTION
// for test
#define kInput  Serial1
#define kOutput Serial

#else

// for production
#define kInput  Serial1
#define kOutput Serial1
#endif

#define kGPGGA_SkipParams kHDOP
#define kGPRMC_SkipParams kRMCEWIndicator

#define kSerialInputPin 3


#define DATAPIN    7
#define CLOCKPIN   8


Adafruit_DotStar strip(1, DATAPIN, CLOCKPIN, DOTSTAR_BRG);

static char s_buffer[1024];
static int  s_writeIndex = 0;
static int  s_comma      = 0;
static bool s_latch      = false;

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


void padZeros( const char* input, int full_size )
{
    if( !input )
        return;
    
    int input_size = strlen( input );
    int pad_amount = full_size - input_size;
    if( pad_amount <= 0 )
        return;
    
    for( int i = 0; i < pad_amount; i++ )
        kOutput.write( '0' );
}


void enter_Sleep( void )
{
    /* Configure low-power mode */
    SCB->SCR &= ~( SCB_SCR_SLEEPDEEP_Msk );  // low-power mode = sleep mode

    __DSB();
    __WFE();  // enter low-power mode
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
        // If anything comes in kInput (pins 0 & 1)
        s_buffer[s_writeIndex] = input;
        
        // process our buffer
        if( input == ',' || input == '*' )
        {
            if( input == ',' )
            {
                ++s_comma;
                s_writeIndex = 0;
                s_latch = false;
            }
            
            if( !s_processingGGA && checkForGPGGA( s_buffer ) )
            {
                s_processingGGA = true;
                s_buffer[s_writeIndex] = input; // put that character at the front for further processing
            }
            else if( !s_processingRMC && checkForGPRMC( s_buffer ) )
            {
                s_processingRMC = true;
                s_buffer[s_writeIndex] = input; // put that character at the front for further processing
            }
        }
        
        if( s_processingGGA )
        {
            // see if we should ignore the input or not...
            if( (input != '\n') &&  (input != ',') && (input != '*') && s_comma > kGPGGA_SkipParams )
            {
                if( s_comma == kAltiude )
                {
                    if( !s_latch )                      // needs to be rewritten from xx.y to 000xx.y (MSL Altitude)
                    {
                        s_buffer[++s_writeIndex] = '\0';
                        padZeros( s_buffer, 4 );
                        s_latch = true;
                    }
                    kOutput.write( input );
                }
                else if( s_comma >= kAltUnit )
                    kOutput.write( input );
            }
            else
            {
                if( input == '*' ) // checksum
                    s_processingGGA = false;
                kOutput.write( input );
            }
        }
        else if( s_processingRMC )
        {
            // see if we should ignore the input or not...
            if( (input != '\n') &&  (input != ',') && (input != '*') && s_comma > kGPRMC_SkipParams )
            {
                if( s_comma == kRMCSpeed )
                {
                    if( !s_latch )                      // needs to be rewritten from x.yy to 000x.yy (Speed over ground)
                    {
                        s_buffer[++s_writeIndex] = '\0';
                        padZeros( s_buffer, 5 );
                        s_latch = true;
                    }
                    kOutput.write( input );
                }
                else if( s_comma >= kRMCCourse )
                    kOutput.write( input );
            }
            else
            {
                if( input == '*' ) // checksum
                    s_processingRMC = false;
                kOutput.write( input );
            }
            
        }
        else
        {
            kOutput.write( input );
        }
        
        if( input == '\n' )
        {
            s_writeIndex = 0;
            s_comma = 0;
            s_processingRMC = false;
        }
        else
            ++s_writeIndex;
    }
    
    enter_Sleep();
}

// EOF
