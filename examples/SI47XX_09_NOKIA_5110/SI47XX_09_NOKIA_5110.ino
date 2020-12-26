/*

  Under construction... 

  This sketch uses an Arduino Pro Mini, 3.3V (8MZ) with a display NOKIA.
  To control the Nokia display this sketch uses the libraries Adafruit_GFX and Adafruit_PCD8544.
  Plese, install these libraries before compiling this sketch. 

  The  purpose  of  this  example  is  to  demonstrate a prototype  receiver based  on  the  SI4735-D60 or Si4732-A10  and  the 
  "PU2CLR SI4735 Arduino Library" working with NOKIA Diaplsy. It is not the purpose of this prototype 
  to provide you a beautiful interface. I think you can do it better than me. 

  It is  a  complete  radio  capable  to  tune  LW,  MW,  SW  on  AM  and  SSB  mode  and  also  receive  the
  regular  comercial  stations. 

  Features:   AM; SSB; LW/MW/SW; two super band (from 150kHz to 30 MHz); external mute circuit control; Seek (Automatic tuning)
              AGC; Attenuation gain control; SSB filter; CW; AM filter; 1, 5, 10, 50 and 500kHz step on AM and 10Hhz sep on SSB

  Wire up on Arduino UNO, Pro mini and SI4735-D60

  | Device name               | Device Pin / Description      |  Arduino Pin  |
  | ----------------          | ----------------------------- | ------------  |
  | Display NOKIA             |                               |               |
  |                           | (1) RST (RESET)               |     8         |
  |                           | (2) CE or CS                  |     9         |
  |                           | (3) DC or DO                  |    10         |
  |                           | (4) DIN or DI or MOSI         |    11         |
  |                           | (5) CLK                       |    13         |
  |                           | (6) VCC  (3V-5V)              |    +VCC       |  
  |                           | (7) BL/DL/LIGHT               |    +VCC       |
  |                           | (8) GND                       |    GND        |
  |     Si4735                |                               |               |
  |                           | (*3) RESET (pin 15)           |     12        |
  |                           | (*3) SDIO (pin 18)            |     A4        |
  |                           | (*3) SCLK (pin 17)            |     A5        |
  |                           | (*4) SEN (pin 16)             |    GND        | 
  |     Buttons               |                               |               |
  |                           | (*1)Switch MODE (AM/LSB/AM)   |      4        |
  |                           | (*1)Banddwith                 |      5        |
  |                           | (*1)BAND                      |      6        |
  |                           | (*2)SEEK                      |      7        |
  |                           | (*1)AGC/Attenuation           |     14 / A0   |
  |                           | (*1)STEP                      |     15 / A1   | 
  |                           | VFO/VFO Switch (Encoder)      |     16 / A2   |
  |    Encoder                |                               |               |
  |                           | A                             |       2       |
  |                           | B                             |       3       |

  (*1) You have to press the push button and after, rotate the encoder to select the parameter.
       After you activate a command by pressing a push button, it will keep active for 2,5 seconds. 
  (*2) The SEEK direction is based on the last movement of the encoder. If the last movement of 
       the encoder was clockwise, the SEEK will be towards the upper limit. If the last movement of 
       the encoder was counterclockwise, the SEEK direction will be towards the lower limit.  
  (*3) - If you are using the SI4732-A10, check the corresponding pin numbers.  
  (*4) - If you are using the SI4735-D60, connect the SEN pin to the ground; 
         If you are using the SI4732-A10, connect the SEN pin to the +Vcc. 

  Prototype documentation: https://pu2clr.github.io/SI4735/
  PU2CLR Si47XX API documentation: https://pu2clr.github.io/SI4735/extras/apidoc/html/
  Jim Reagan's schematic: https://github.com/JimReagans/Si4735-radio-PCB-s-and-bandpass-filter

  By PU2CLR, Ricardo; and W09CHL, Jim Reagan;  Sep  2020.

*/

#include <SI4735.h>

#include <Adafruit_GFX.h>    //Downlaod it here : https://www.electronoobs.com/eng_arduino_Adafruit_GFX.php
#include <Adafruit_PCD8544.h> //Download it here: https://www.electronoobs.com/eng_arduino_Adafruit_PCD8544.php
#include <SPI.h>
#include "Rotary.h"

// Test it with patch_init.h 
#include "patch_init.h" // SSB patch for whole SSBRX initialization string

const uint16_t size_content = sizeof ssb_patch_content; // see ssb_patch_content in patch_full.h or patch_init.h

// NOKIA Display pin setup
#define NOKIA_RST  8  // RESET
#define NOKIA_CE   9  // Some NOKIA devices show CS
#define NOKIA_DC  10  // 
#define NOKIA_DIN 11  // MOSI
#define NOKIA_CLK 13  // SCK
#define NOKIA_LED  0  // 0 if wired to +3.3V directly

#define FM_BAND_TYPE 0
#define MW_BAND_TYPE 1
#define SW_BAND_TYPE 2
#define LW_BAND_TYPE 3

#define RESET_PIN 12

// Enconder PINs
#define ENCODER_PIN_A 2
#define ENCODER_PIN_B 3

// Buttons controllers
#define MODE_SWITCH 4         // Switch MODE (Am/LSB/USB)
#define BANDWIDTH_BUTTON 5    // Used to select the banddwith. Values: 1.2, 2.2, 3.0, 4.0, 0.5, 1.0 kHz
#define BAND_BUTTON 6         // Band switch button
#define SEEK_BUTTON 7         // Previous band
#define AGC_SWITCH 14         // Pin A0 - Switch AGC ON/OF
#define STEP_SWITCH 15        // Pin A1 - Used to select the increment or decrement frequency step (1, 5 or 10 kHz)
#define BFO_SWITCH 16         // Pin A2 - Used to select the enconder control (BFO or VFO)
#define AUDIO_MUTE 1          // External AUDIO MUTE circuit control

#define MIN_ELAPSED_TIME 300
#define MIN_ELAPSED_RSSI_TIME 150
#define ELAPSED_COMMAND 2500  // time to turn off the last command controlled by encoder
#define DEFAULT_VOLUME 50     // change it for your favorite sound volume

#define FM 0
#define LSB 1
#define USB 2
#define AM 3
#define LW 4

#define SSB 1
#define CLEAR_BUFFER(x)  (x[0] = '\0');

bool bfoOn = false;
bool ssbLoaded = false;

// AGC and attenuation control
int8_t agcIdx = 0;
uint8_t disableAgc = 0;
int8_t agcNdx = 0;

bool cmdBand = false;
bool cmdVolume = false;
bool cmdAgc = false;
bool cmdBandwidth = false;
bool cmdStep = false;
bool cmdMode = false;

int currentBFO = 0;
uint8_t seekDirection = 1;   // Tells the SEEK direction (botton or upper limit)

long elapsedRSSI = millis();
long elapsedButton = millis();
long elapsedCommand = millis();

// Encoder control variables
volatile int encoderCount = 0;

// Some variables to check the SI4735 status
uint16_t currentFrequency;

uint8_t currentBFOStep = 10;

typedef struct 
{
  uint8_t idx;      // SI473X device bandwitdth index
  const char *desc; // bandwitdth description
} Bandwitdth;

int8_t bwIdxSSB = 4;
Bandwitdth bandwitdthSSB[] = {{4, "0.5"},   // 0
                              {5, "1.0"},   // 1
                              {0, "1.2"},   // 2
                              {1, "2.2"},   // 3
                              {2, "3.0"},   // 4
                              {3, "4.0"}};  // 5

int8_t bwIdxAM = 4;
Bandwitdth bandwitdthAM[] = {{4, "1.0"},
                             {5, "1.8"},
                             {3, "2.0"},
                             {6, "2.5"},
                             {2, "3.0"},
                             {1, "4.0"},
                             {0, "6.0"}};

const char * bandModeDesc[] = {"   ", "LSB", "USB", "AM "};
uint8_t currentMode = FM;

uint16_t currentStep = 1;

/*
   Band data structure
*/
typedef struct
{
  const char *bandName; // Band description
  uint8_t bandType;     // Band type (FM, MW or SW)
  uint16_t minimumFreq; // Minimum frequency of the band
  uint16_t maximumFreq; // maximum frequency of the band
  uint16_t currentFreq; // Default frequency or current frequency
  uint16_t currentStep; // Defeult step (increment and decrement)
} Band;

/*
   Band table
   Actually, except FM (VHF), the other bands cover the entire LW / MW and SW spectrum.
   Only the default frequency and step is changed. You can change this setup. 
*/
Band band[] = {
    {"FM ", FM_BAND_TYPE, 6400, 10800, 10390, 10},
    {"MW ", MW_BAND_TYPE, 150, 1720, 810, 10},
    {"SW1", SW_BAND_TYPE, 150, 30000, 7100, 1}, // Here and below: 150kHz to 30MHz
    {"SW2", SW_BAND_TYPE, 150, 30000, 9600, 5},
    {"SW3", SW_BAND_TYPE, 150, 30000, 11940, 5},
    {"SW4", SW_BAND_TYPE, 150, 30000, 13600, 5},
    {"SW5", SW_BAND_TYPE, 150, 30000, 14200, 1},
    {"SW5", SW_BAND_TYPE, 150, 30000, 15300, 5},
    {"SW6", SW_BAND_TYPE, 150, 30000, 17600, 5},
    {"SW7", SW_BAND_TYPE, 150, 30000, 21100, 1},
    {"SW8", SW_BAND_TYPE, 150, 30000, 28400, 1}};

const int lastBand = (sizeof band / sizeof(Band)) - 1;
int bandIdx = 1;


int tabStep[] = {1, 5, 10, 50, 100, 500, 1000};
const int lastStep = (sizeof tabStep / sizeof(int)) - 1;
int idxStep = 0;


uint8_t rssi = 0;
uint8_t snr = 0;
uint8_t stereo = 1;
uint8_t volume = DEFAULT_VOLUME;

// Devices class declarations
Rotary encoder = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);
Adafruit_PCD8544 display = Adafruit_PCD8544(NOKIA_DC, NOKIA_CE, NOKIA_RST);

SI4735 rx;

void setup()
{
  // Encoder pins
  pinMode(ENCODER_PIN_A, INPUT_PULLUP);
  pinMode(ENCODER_PIN_B, INPUT_PULLUP);
  pinMode(BANDWIDTH_BUTTON, INPUT_PULLUP);
  pinMode(BAND_BUTTON, INPUT_PULLUP);
  pinMode(SEEK_BUTTON, INPUT_PULLUP);
  pinMode(BFO_SWITCH, INPUT_PULLUP);
  pinMode(AGC_SWITCH, INPUT_PULLUP);
  pinMode(STEP_SWITCH, INPUT_PULLUP);
  pinMode(MODE_SWITCH, INPUT_PULLUP);

  // uncomment the line below if you have external audio mute circuit
  // rx.setAudioMuteMcuPin(AUDIO_MUTE);

  // Start the Nokia display device
  display.begin();
  display.setContrast(80);
  // Splash
  splash();
  // Encoder interrupt
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), rotaryEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), rotaryEncoder, CHANGE);

  // rx.setup(RESET_PIN, 1); // Starts FM mode and ANALOG audio mode
  rx.setup(RESET_PIN, -1, 1, SI473X_ANALOG_AUDIO); // Starts FM mode and ANALOG audio mode.
  // rx.setup(RESET_PIN, -1, 1, SI473X_ANALOG_DIGITAL_AUDIO); // Starts FM mode and ANALOG and DIGITAL audio mode.

  // Set up the radio for the current band (see index table variable bandIdx )
  useBand();
  rx.setVolume(volume);
  showStatus();
}

/**
 * Shows splash message
 */
void splash() {
  display.clearDisplay();
  display.display();
  display.setTextColor(BLACK);
  // Splash - Change it for your introduction text.
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.print("SI4735");
  display.setCursor(0, 15);
  display.print("Arduino");
  display.setCursor(0, 30);
  display.print("Library");
  display.display();
  delay(3000);
  display.clearDisplay();
  display.setCursor(30, 0);
  display.print("BY");
  display.setCursor(10, 20);
  display.print("PU2CLR");
  display.display();
  delay(2000);
  display.clearDisplay();
}

/**
 *  Set all command flags to false
 *  When all flags are disabled (false), the encoder controls the frequency
 */
void disableCommands() {
  cmdBand = false;
  bfoOn = false;
  cmdVolume = false;
  cmdAgc = false;
  cmdBandwidth = false;
  cmdStep = false;
  cmdMode = false;
}

/**
 *  Shows the static content on  display
 */
void showTemplate()
{

}


/**
 *   Reads encoder via interrupt
 *   Use Rotary.h and  Rotary.cpp implementation to process encoder via interrupt
 */
void rotaryEncoder()
{ // rotary encoder events
  uint8_t encoderStatus = encoder.process();

  if (encoderStatus)
    encoderCount = (encoderStatus == DIR_CW) ? 1 : -1;
}

void show(uint8_t col, uint8_t lin, uint8_t textSize, const char *content) {
  display.setCursor(col, lin);
  display.setTextSize(textSize);
  display.print(content);
  display.display();
}

    /**
 *  Shows frequency information on Display
 */
    void showFrequency()
{
  char tmp[10];
  char bufferDisplay[10];
  
  // It is better than use dtostrf or String to save space.
  sprintf(tmp, "%5.5u", currentFrequency);

  bufferDisplay[0] = (tmp[0] == '0') ? ' ' : tmp[0];
  bufferDisplay[1] = tmp[1];
  if (rx.isCurrentTuneFM())
  {
    bufferDisplay[2] = tmp[2];
    bufferDisplay[3] = '.';
    bufferDisplay[4] = tmp[3];
  }
  else
  {
    if ( currentFrequency  < 1000 ) {
      bufferDisplay[1] = ' ';
      bufferDisplay[2] = tmp[2] ;
      bufferDisplay[3] = tmp[3];
      bufferDisplay[4] = tmp[4];
    } else {
      bufferDisplay[2] = tmp[2];
      bufferDisplay[3] = tmp[3];
      bufferDisplay[4] = tmp[4];
    }
  }
  bufferDisplay[5] = '\0';

  show(0,0,2,bufferDisplay);
}


/**
 *  This function is called by the seek function process.
 */
void showFrequencySeek(uint16_t freq)
{
  currentFrequency = freq;
  showFrequency();
}

/**
 *   Show some basic information on display
 */
void showStatus()
{
  char *unt;
  char bufferDisplay[20];

  sprintf(bufferDisplay, "%s %s", band[bandIdx].bandName, bandModeDesc[currentMode]);
  show(0,15,1,bufferDisplay);

  if (rx.isCurrentTuneFM()) {
    unt = (char *) "MHz";
  } else
  {
    unt = (char *) "kHz";
    showStep();
    showAgcAtt();
    if (ssbLoaded)  showBFO();
    showBandwitdth();
    showAgcAtt();
  }
  show(65,8,1,unt);

  showRSSI();
  showFrequency();
}

/**
 * Shows the current Bandwitdth status
 */
void showBandwitdth() {
    char bufferDisplay[15];
    // Bandwidth
    if (currentMode == LSB || currentMode == USB || currentMode == AM) {
      char * bw;

      if (currentMode == AM) 
        bw = (char *) bandwitdthAM[bwIdxAM].desc;
      else 
        bw = (char *) bandwitdthSSB[bwIdxSSB].desc;
      sprintf(bufferDisplay, "BW: %s kHz", bw);
    } 
    else {
      bufferDisplay[0] = '\0';
    }

}

/**
 *  Shows the current RSSI and SNR status
 */
void showRSSI()
{
    int rssiLevel;
    uint8_t rssiAux;
    char sRssi[7];
    // int maxAux = tft.width() - 10;

    display.setTextSize(1);

    if (currentMode == FM)
    {
      display.setCursor(70,0);
      display.print((rx.getCurrentPilot())? "ST" : "MO");
    }

    if (rssi < 2) 
       rssiAux = 4;
    else if ( rssi < 4)
       rssiAux = 5;
    else if ( rssi < 12 ) 
       rssiAux = 6;
    else if (rssi < 25) 
       rssiAux = 7;
    else if ( rssi < 50 )
       rssiAux = 8;
    else if ( rssi >= 50 )
       rssiAux = 9;

    sprintf(sRssi,"S%u%c",rssiAux,(rssiAux == 9)? '+': ' ');
    show(15,70,1,sRssi);
}

/**
 *  Shows the current AGC and Attenuation status
 */
void showAgcAtt() {
    char sAgc[10];
    rx.getAutomaticGainControl();
    if (agcNdx == 0 && agcIdx == 0)
      strcpy(sAgc, "AGC ON");
    else
      sprintf(sAgc, "ATT: %2d", agcNdx);

    show(0,20,1,sAgc);   
}

/**
 *  Shows the current step
 */
void showStep() {
  char sStep[15];
  sprintf(sStep, "Stp:%3d", currentStep);
  show(60,20,1,sStep); 
}

/**
 * Shows the current BFO value
 */
void showBFO()
{
    char bufferDisplay[10];
    sprintf(bufferDisplay, "%+4d", currentBFO);
    show(0,40,1,bufferDisplay);
    elapsedCommand = millis();
}


/**
 *  Sets Band up (1) or down (!1)
 */
void setBand(int8_t up_down) {
  band[bandIdx].currentFreq = currentFrequency;
  band[bandIdx].currentStep = currentStep;
  if ( up_down == 1)
    bandIdx = (bandIdx < lastBand) ? (bandIdx + 1) : 0;
  else
    bandIdx = (bandIdx > 0) ? (bandIdx - 1) : lastBand;
  useBand();
  delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
  elapsedCommand = millis();
}

/**
 *  This function loads the contents of the ssb_patch_content array into the CI (Si4735) and starts the radio on
 *  SSB mode.
 *  See also loadPatch implementation in the SI4735 Arduino Library (SI4735.h/SI4735.cpp) 
 */
void loadSSB()
{
  rx.reset();
  rx.queryLibraryId(); // Is it really necessary here? I will check it.
  rx.patchPowerUp();
  delay(50);
  rx.setI2CFastMode(); // Recommended
  // rx.setI2CFastModeCustom(500000); // It is a test and may crash.
  rx.downloadPatch(ssb_patch_content, size_content);
  rx.setI2CStandardMode(); // goes back to default (100kHz)

  // Parameters
  // AUDIOBW - SSB Audio bandwidth; 0 = 1.2kHz (default); 1=2.2kHz; 2=3kHz; 3=4kHz; 4=500Hz; 5=1kHz;
  // SBCUTFLT SSB - side band cutoff filter for band passand low pass filter ( 0 or 1)
  // AVC_DIVIDER  - set 0 for SSB mode; set 3 for SYNC mode.
  // AVCEN - SSB Automatic Volume Control (AVC) enable; 0=disable; 1=enable (default).
  // SMUTESEL - SSB Soft-mute Based on RSSI or SNR (0 or 1).
  // DSP_AFCDIS - DSP AFC Disable or enable; 0=SYNC MODE, AFC enable; 1=SSB MODE, AFC disable.
  rx.setSSBConfig(bandwitdthSSB[bwIdxSSB].idx, 1, 0, 0, 0, 1);
  delay(25);
  ssbLoaded = true;
}

/**
 *  Switch the radio to current band
 */
void useBand()
{
  if (band[bandIdx].bandType == FM_BAND_TYPE)
  {
    currentMode = FM;
    rx.setTuneFrequencyAntennaCapacitor(0);
    rx.setFM(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq, band[bandIdx].currentFreq, band[bandIdx].currentStep);
    rx.setSeekFmLimits(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq);
    bfoOn = ssbLoaded = false;
  }
  else
  {
    // set the tuning capacitor for SW or MW/LW
    rx.setTuneFrequencyAntennaCapacitor( (band[bandIdx].bandType == MW_BAND_TYPE || band[bandIdx].bandType == LW_BAND_TYPE) ? 0 : 1);

    if (ssbLoaded)
    {
      rx.setSSB(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq, band[bandIdx].currentFreq, band[bandIdx].currentStep, currentMode);
      rx.setSSBAutomaticVolumeControl(1);
    }
    else
    {
      currentMode = AM;
      rx.setAM(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq, band[bandIdx].currentFreq, band[bandIdx].currentStep);
      bfoOn = false;
    }
    rx.setAmSoftMuteMaxAttenuation(0); // Disable Soft Mute for AM or SSB
    rx.setAutomaticGainControl(disableAgc, agcNdx);
    rx.setSeekAmLimits(band[bandIdx].minimumFreq, band[bandIdx].maximumFreq); // Consider the range all defined current band
    rx.setSeekAmSpacing((band[bandIdx].currentStep > 10) ? 10 : band[bandIdx].currentStep); // Max 10kHz for spacing
  }
  delay(100);
  currentFrequency = band[bandIdx].currentFreq;
  currentStep = band[bandIdx].currentStep;
  idxStep = getStepIndex(currentStep);
  rssi = 0;
  showStatus();
}


/**
 *  Switches the Bandwidth
 */
void doBandwidth(int8_t v) {
  if (currentMode == LSB || currentMode == USB)
  {
    bwIdxSSB = ( v == 1) ? bwIdxSSB + 1 : bwIdxSSB - 1;

    if (bwIdxSSB > 5)
      bwIdxSSB = 0;
    else if ( bwIdxSSB < 0 )
      bwIdxSSB = 5;

    rx.setSSBAudioBandwidth(bandwitdthSSB[bwIdxSSB].idx);
    // If audio bandwidth selected is about 2 kHz or below, it is recommended to set Sideband Cutoff Filter to 0.
    if (bandwitdthSSB[bwIdxSSB].idx == 0 || bandwitdthSSB[bwIdxSSB].idx == 4 || bandwitdthSSB[bwIdxSSB].idx == 5)
       rx.setSBBSidebandCutoffFilter(0);
    else
       rx.setSBBSidebandCutoffFilter(1);
  }
  else if (currentMode == AM)
  {
    bwIdxAM = ( v == 1) ? bwIdxAM + 1 : bwIdxAM - 1;

    if (bwIdxAM > 6)
      bwIdxAM = 0;
    else if ( bwIdxAM < 0)
      bwIdxAM = 6;

    rx.setBandwidth(bandwitdthAM[bwIdxAM].idx, 1);
  }
  showBandwitdth();
  elapsedCommand = millis();
  delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.

}

/**
 *  Deal with AGC and attenuattion
 */
void doAgc(int8_t v) {

  agcIdx = (v == 1) ? agcIdx + 1 : agcIdx - 1;
  if (agcIdx < 0 )
    agcIdx = 37;
  else if ( agcIdx > 37)
    agcIdx = 0;


  disableAgc = (agcIdx > 0); // if true, disable AGC; esle, AGC is enable 

  if (agcIdx > 1)
    agcNdx = agcIdx - 1;
  else
    agcNdx = 0;

  rx.setAutomaticGainControl(disableAgc, agcNdx); // if agcNdx = 0, no attenuation
  showAgcAtt();
  delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
  elapsedCommand = millis();
}

/**
 *  Gets the current step index.
 */
int getStepIndex(int st) {
  for (int i = 0; i < lastStep; i++) {
    if ( st == tabStep[i] ) return i;
  }
  return 0;
}

/**
 *  Switches the current step
 */
void doStep(int8_t v) {
  idxStep = ( v == 1 ) ? idxStep + 1 : idxStep - 1;
  if ( idxStep > lastStep)
     idxStep = 0;
  else if ( idxStep < 0 )
     idxStep = lastStep;

  currentStep = tabStep[idxStep];

  rx.setFrequencyStep(currentStep);
  band[bandIdx].currentStep = currentStep;
  rx.setSeekAmSpacing((currentStep > 10) ? 10 : currentStep); // Max 10kHz for spacing
  showStep();
  delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
  elapsedCommand = millis();
}

/**
 *  Switches to the AM, LSB or USB modes
 */
void doMode(int8_t v) {

  if (currentMode != FM)
  {
    if (currentMode == AM)
    {
      // If you were in AM mode, it is necessary to load SSB patch (avery time)
      loadSSB();
      currentMode = LSB;
    }
    else if (currentMode == LSB)
    {
      currentMode = USB;
    }
    else if (currentMode == USB)
    {
      currentMode = AM;
      bfoOn = ssbLoaded = false;
    }
    // Nothing to do if you are in FM mode
    band[bandIdx].currentFreq = currentFrequency;
    band[bandIdx].currentStep = currentStep;
    useBand();
  }

  delay(MIN_ELAPSED_TIME); // waits a little more for releasing the button.
  elapsedCommand = millis();
}

/**
 *  Find a station. The direction is based on the last encoder move clockwise or counterclockwise
 */
void doSeek() {
  rx.seekStationProgress(showFrequencySeek, seekDirection);
  currentFrequency = rx.getFrequency();
}

void loop()
{
  // Check if the encoder has moved.
  if (encoderCount != 0)
  {
    if (bfoOn & (currentMode == LSB || currentMode == USB))
    {
      currentBFO = (encoderCount == 1) ? (currentBFO + currentBFOStep) : (currentBFO - currentBFOStep);
      rx.setSSBBfo(currentBFO);
      showBFO();
    }
    else if (cmdMode)
      doMode(encoderCount);
    else if (cmdStep)
      doStep(encoderCount);
    else if (cmdAgc)
      doAgc(encoderCount);
    else if (cmdBandwidth)
      doBandwidth(encoderCount);
    else if (cmdBand)
      setBand(encoderCount);
    else
    {
      if (encoderCount == 1)
      {
        rx.frequencyUp();
        seekDirection = 1;
      }
      else
      {
        rx.frequencyDown();
        seekDirection = 0;
      }
      // Show the current frequency only if it has changed
      currentFrequency = rx.getFrequency();
    }
    showFrequency();
    encoderCount = 0;
  }
  else
  {
    if (digitalRead(BANDWIDTH_BUTTON) == LOW)
    {
      cmdBandwidth = !cmdBandwidth;
      delay(MIN_ELAPSED_TIME);
      elapsedCommand = millis();
    }
    else if (digitalRead(BAND_BUTTON) == LOW)
    {
      cmdBand = !cmdBand;
      delay(MIN_ELAPSED_TIME);
      elapsedCommand = millis();
    }
    else if (digitalRead(SEEK_BUTTON) == LOW)
    {
      doSeek();
    }
    else if (digitalRead(BFO_SWITCH) == LOW)
    {
      bfoOn = !bfoOn;
      if ((currentMode == LSB || currentMode == USB))
        showBFO();
        
      showFrequency();
      delay(MIN_ELAPSED_TIME);
      elapsedCommand = millis();
    }
    else if (digitalRead(AGC_SWITCH) == LOW)
    {
      cmdAgc = !cmdAgc;
      delay(MIN_ELAPSED_TIME);
      elapsedCommand = millis();
    }
    else if (digitalRead(STEP_SWITCH) == LOW)
    {
      cmdStep = !cmdStep;
      delay(MIN_ELAPSED_TIME);
      elapsedCommand = millis();
    }
    else if (digitalRead(MODE_SWITCH) == LOW)
    {
      cmdMode = !cmdMode;
      delay(MIN_ELAPSED_TIME);
      elapsedCommand = millis();
    }
  }

  // Show RSSI status only if this condition has changed
  if ((millis() - elapsedRSSI) > MIN_ELAPSED_RSSI_TIME * 6)
  {
    rx.getCurrentReceivedSignalQuality();
    int aux = rx.getCurrentRSSI();
    if (rssi != aux)
    {
      rssi = aux;
      snr = rx.getCurrentSNR();
      showRSSI();
    }
    elapsedRSSI = millis();
  }

  // Disable commands control
  if ((millis() - elapsedCommand) > ELAPSED_COMMAND)
  {
    if ((currentMode == LSB || currentMode == USB)) {
      bfoOn = false;
      showBFO();
      showFrequency();
    }
    disableCommands();
    elapsedCommand = millis();
  }
  delay(1);
}
