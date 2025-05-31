/*
Foam Force Gauge v 0.5 
based on Adafruit NAU7802 load cell interpreter board 
calibrated to "50kg" load cells affixed vertically with 
38g target plate  and hardware attached 

*/
#include <Adafruit_NAU7802.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Adafruit_NAU7802 nau;

void setup() {
  Serial.begin(115200); // can use various baud rates with this i2c board but this is the most stable 
  Serial.println("NAU7802");
  if (! nau.begin()) {
    Serial.println("Failed to find NAU7802");
  }
  Serial.println("Found NAU7802");

  nau.setLDO(NAU7802_3V0);  //The NAU7802 can be set to various voltages between 2.4 and 4.5 v. Since the 32u4 is 3v logic already this worked well
  Serial.print("LDO voltage set to 3.0V");

  nau.setGain(NAU7802_GAIN_128); //The NAU7802 has an in built amplifier. set to max for the moment. 
  Serial.print("Gain set to 128");

  /* Set to 80 samples per second as a reliable, high speed, poling rate for the adc. 
  Alternative set to 10 samples per second if you need to manually read every value as it comes through. 
   Can do any of the following rates 10,20,40,80,320(Although 320sps appears to be unstable) */
  nau.setRate(NAU7802_RATE_80SPS); 
  Serial.print("Conversion rate set to 80 SPS");
  
  // Take 10 readings to flush out readings
  for (uint8_t i = 0; i < 10; i++) {
    while (! nau.available()) delay(1);
    nau.read();
  }

  //set 0 point calibration at statup. Can do multi point but the below conversion rate assumes linear approxmiation for the load cell
  while (! nau.calibrate(NAU7802_CALMOD_INTERNAL)) { 
    Serial.println("Failed to calibrate internal offset, retrying!");
    delay(1000);
  }
  Serial.println("Calibrated internal offset");

  while (! nau.calibrate(NAU7802_CALMOD_OFFSET)) {
    Serial.println("Failed to calibrate system offset, retrying!");
    delay(1000);
  }
  Serial.println("Calibrated system offset");  

  //Set up the OLED Display
   // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3c)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000); // Pause for 2 seconds
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();        
  drawdisplay(0,0);
  display.display();
}

void loop() {

    static int32_t PeakValue = 0;
    static unsigned long LastValidReadingTime = 0;
    static bool ReadingIsNoise = false;
    static int64_t Impulse = 0;
    static unsigned long LastMeasureTime = 0;
    
    static int32_t LoadCell = 50; //set the type of load cell used. Theoretical full range for a 50kg beam strain gauge load cell

  while (! nau.available()) {
    delay(1);
  }
  // read value currently ready by the force gauge 
  int32_t val = nau.read();
  /* Convert read value to mN (2 * Load Cell Range * 9.80665 N/kg * 1000 mN/N) = 19,613.3*LoadCell
  24 bit analouge to digital converter range (23 bit number with ack bit) = 8388608   */
  int32_t ReadValue = abs(val)*LoadCell*19613.3/8388608;  
  unsigned long CurrentTime = millis();
  //if the value has fallen below 80 stop taking readings because the curve is complete
  if (ReadValue < 80) {
    ReadingIsNoise = true;
  }

  /* Calculate impulse (area under the curve)  
  Note: boards without oscilator chips may have less accurate impulse values 
  (although their relative values should be equivalent) */
  if (LastMeasureTime != 0 && !ReadingIsNoise) {
      Impulse += ReadValue * (CurrentTime - LastMeasureTime);
    }

  //Record the current measurment time
  LastMeasureTime = CurrentTime;

  //set the peak value read by the force gauge if higher than current value 
  PeakValue = max(PeakValue, ReadValue);
  
  //any valid reading time could mark the end of a peak. (Timer should start from the end of the most recent peak)
  if(!ReadingIsNoise) {
    LastValidReadingTime = CurrentTime;
  }

  //hold onto the most recent values for 10 seconds of "no reading" (below 50mN)
  if (ReadingIsNoise && (CurrentTime - LastValidReadingTime > 10000)) { 
    PeakValue = 0;
    Impulse = 0;
  }

  if(ReadingIsNoise){
  printreadingsdisplay(PeakValue,Impulse);
  }

  ReadingIsNoise = false;

  printreadingsserial(ReadValue,PeakValue,Impulse);

  

}

void printreadingsserial(int32_t ReadValue,int32_t PeakValue,int64_t Impulse) {
  //print current force, peak force, and impulse (for that peak) value measurments 
  Serial.println("---------------------------------");
  Serial.print("Force measured (mN)"); 
  Serial.println(ReadValue); 
  Serial.print("Peak Force (mN)");
  Serial.println(PeakValue);
  Serial.print("Impulse (mN*S)"); // or N*mS
  Serial.println((long)(Impulse/1000)); 
}

void printreadingsdisplay(int32_t PeakValue,int64_t Impulse) {
display.clearDisplay();
drawdisplay(PeakValue,Impulse);
display.display();
}

void drawdisplay(int32_t PeakValue,int64_t Impulse) {
  display.setCursor(0,0); // Start at top-left corner
  display.setTextSize(1);  
  display.println(F("        Force        "));
  display.setTextSize(3);  
  display.print(PeakValue);
  display.setCursor(72,8);
  display.setTextSize(2);
  display.println(F("mN"));
  display.setTextSize(1);  
  display.setCursor(0,32);
  display.println(F("       Impulse       "));
  display.setTextSize(3);  
  display.print((long)(Impulse/1000));
  display.setCursor(72,40);
  display.setTextSize(2);
  display.cp437(true); // full character set 
  display.print("mN");
  display.write(0x07); // • symbol
  display.println("S");
}
