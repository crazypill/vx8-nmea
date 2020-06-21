/*
 SerialPassthrough sketch
 
 Some boards, like the Arduino 101, the MKR1000, Zero, or the Micro, have one
 hardware serial port attached to Digital pins 0-1, and a separate USB serial
 port attached to the IDE Serial Monitor. This means that the "serial
 passthrough" which is possible with the Arduino UNO (commonly used to interact
 with devices/shields that require configuration via serial AT commands) will
 not work by default.
 
 This sketch allows you to emulate the serial passthrough behaviour. Any text
 you type in the IDE Serial monitor will be written out to the serial port on
 Digital pins 0 and 1, and vice-versa.
 
 On the 101, MKR1000, Zero, and Micro, "Serial" refers to the USB Serial port
 attached to the Serial Monitor, and "kInput" refers to the hardware serial
 port attached to pins 0 and 1. This sketch will emulate Serial passthrough
 using those two Serial ports on the boards mentioned above, but you can change
 these names to connect any two serial ports on a board that has multiple ports.
 
 created 23 May 2016
 by Erik Nyquist
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
static int  s_comma = 0;
static bool s_latch = false;
static bool s_led   = false;

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



void wakeup()
{
//    strip.setPixelColor(0, 255, 127, 0);
//    strip.show();
//    blink();
//    digitalWrite(LED_BUILTIN, HIGH);
  s_led = true;
}


void blink() 
{
  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(500);                       // wait for a half-second
  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
  delay(500);                       // wait for a half-second
}


void setup() {
    kOutput.begin(9600);
#ifndef PRODUCTION
    kInput.begin(9600);
#endif
    s_writeIndex = 0;

    strip.begin();
//    strip.setPixelColor(0, 255, 127, 0);
    strip.show();

    pinMode(LED_BUILTIN, OUTPUT);

    I2S->CTRLA.bit.ENABLE=0;
    ADC->CTRLA.bit.ENABLE=0;
    DAC->CTRLA.bit.ENABLE=0;
    AC->CTRLA.bit.ENABLE=0;

    PM->APBBMASK.reg &= ~PM_APBBMASK_PORT;
    PM->APBBMASK.reg &= ~PM_APBBMASK_DMAC;

    PM->APBCMASK.reg &= ~PM_APBCMASK_ADC;
    PM->APBCMASK.reg &= ~PM_APBCMASK_DAC;
    PM->APBCMASK.reg &= ~PM_APBCMASK_SERCOM3;
    PM->APBCMASK.reg &= ~PM_APBCMASK_SERCOM1;
    PM->APBCMASK.reg &= ~PM_APBCMASK_SERCOM2;

    // Disable USB port (to disconnect correctly from host)
#ifndef PRODUCTION
    USB->DEVICE.CTRLA.reg &= ~USB_CTRLA_ENABLE;
#endif
 
//    attachInterrupt( digitalPinToInterrupt(kSerialInputPin), wakeup, CHANGE);
    EIC->WAKEUP.reg |= (1 << digitalPinToInterrupt(kSerialInputPin));
}


void loop() {
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
                    s_processingGGA = false;
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
            s_processingGGA = false;
        }
        else
            ++s_writeIndex;

    }
    
    enter_Sleep();

    if( s_led )
      digitalWrite(LED_BUILTIN, HIGH);

}



//void loop()
//{
//  if (kInput.available()) {     // If anything comes in kOutput (pins 0 & 1)
//    kOutput.write(kInput.read());   // read it and send it out Serial (USB)
//  }
//}
