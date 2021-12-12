#include <LiquidCrystal.h>
#include <Wire.h>
#include <arduino-timer.h>
#include "Adafruit_FONA.h"
#include "RTClib.h"

/*-----------------------------------------------------------*/
/* Timer with milliseconds */
auto timer = timer_create_default();

/*-----------------------------------------------------------*/
/* Global variables used for RPi slave */
#define WAKE_UP 8  // time to wake up pi
#define SLEEP 24   // time to halt
#define AWAKE_PIN 6
char msg[17];
int RPi_slave = 9;       // RPi I2C slave address
char count[17] = "N/A";  // People count reg
bool RPi_on = true;

/*-----------------------------------------------------------*/
/* Global variables used for RTC */
DateTime now;
RTC_PCF8523 rtc;               // RTC object
char timeStamp[17] = "N/A";    // Format: 2021/11/22/14/04
char timeDisplay[17] = "N/A";  // Format: 2021/11/22 14:04

/*-----------------------------------------------------------*/
/* Global variables used for LCD */
const uint8_t rs = 9, en = 8, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);  // LCD object

/*-----------------------------------------------------------*/
/* print long sentence on lcd screen, delay 2s each screen */
/* only used in setup  */
void printLong(String data) {
    int lines = ceil((double)data.length() / 16.0);
    char line[16];
    for (int i = 0; i < lines; i++) {
        if (i % 2 == 0)
            lcd.clear();
        strncpy(line, data.c_str() + i * 16, 16);
        lcd.setCursor(0, i % 2);
        lcd.print(line);
        if (i % 2 == 1 || i == lines - 1)
            delay(2000);
    }
}


/*-----------------------------------------------------------*/
/* send message to RPi to I2C */
void Send_Command(char* str) {
    sprintf(msg, str);
    Wire.beginTransmission(RPi_slave);
    Wire.write(msg);
    Wire.endTransmission();
}


/*-----------------------------------------------------------*/
/* send timeStamp from rtc */
void Read_Time() {
    now = rtc.now();
    uint16_t year = now.year();
    uint8_t month = now.month();
    uint8_t day = now.day();
    uint8_t hour = now.hour();
    uint8_t minute = now.minute();

    sprintf(timeStamp, "%d/%d/%d/%d/%d\0", year, month, day, hour, minute);
    sprintf(timeDisplay, "%d/%d/%d %d:%d\0", year, month, day, hour, minute);
}

/*-----------------------------------------------------------*/
/* read the count from RPi */
void Read_Count() {
    Wire.requestFrom(RPi_slave, 2);
    int idx = 0;
    while (Wire.available()) {
        char c = Wire.read();
        count[idx++] = c;
    }
    count[idx] = '\0';
}

/*-----------------------------------------------------------*/
/* function called by timer */
bool function_to_call(void*) {
    now = rtc.now();
    
    if (now.hour() == SLEEP && RPi_on) 
    {  // tell the RPi to shutdown
        Send_Command("down\n");
        RPi_on = false;
    } 
    else if (now.hour() == WAKE_UP && !RPi_on) 
    { // wake up the RPi by shorting
        digitalWrite(AWAKE_PIN, HIGH);  // short the GPIO3 for 0.5s
        delay(500);
        digitalWrite(AWAKE_PIN, LOW);
        RPi_on = true;
    } 
    else if (RPi_on) 
    {  // request number of people
        Send_Command("count\n");
        Read_Time();
        delay(5000);
        Read_Count();
        
        // display people count and timestamp
        lcd.clear();
        lcd.print("#People " + String(count));
        lcd.setCursor(0, 1);
        lcd.print(String(timeDisplay)); 

        Send_Command("upload\n");
        delay(500);
        Send_Command(timeStamp);
        delay(500);  
    }
    return true;
}

void setup() {
    /* LCD Init 16*2 Mode*/
    lcd.begin(16, 2);
    printLong("Initializing....");
    
    /* GPIO Init*/
    pinMode(AWAKE_PIN, OUTPUT);
    digitalWrite(AWAKE_PIN, LOW);

    /* RPi Slave Init*/
    Wire.begin();
    Wire.requestFrom(RPi_slave,2);  // Dump unreaded data on I2C line, necessary!
    while (Wire.available()) {
        char c = Wire.read();
    }

    /* RTC Init*/
    rtc.begin();
    //rtc.adjust(DateTime(2021, 12, 8, 23, 33, 0));
    if (!rtc.initialized() || rtc.lostPower()) {
        // Serial.println("RTC is NOT initialized, let's set the time!");
        // rtc.adjust(DateTime(2021, 12, 8, 5, 8, 0));
    }
    rtc.start();
    
    printLong("Finish!");

    // call the function every 40 millisc
    timer.every(20000, function_to_call);
}

void loop() {
    timer.tick();  // tick the timer
}
