#include <LiquidCrystal.h>
#include <Wire.h>
#include <arduino-timer.h>
#include "Adafruit_FONA.h"
#include "RTClib.h"

/*-----------------------------------------------------------*/
auto timer = timer_create_default();  // create timer with milliseconds

/*-----------------------------------------------------------*/
/* Global variables used for RPi slave */
#define WAKE_UP 8  // time to wake up pi
#define SLEEP 18   // time to halt
#define AWAKE_PIN 6
char msg[15];
int RPi_slave = 9;       // RPi I2C slave address
char count[15] = "N/A";  // People count reg
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
/* GSM setting */
#define FONA_RST 15
bool GPRS_enabled = false;

HardwareSerial* fonaSerial = &Serial;
Adafruit_FONA_3G fona = Adafruit_FONA_3G(FONA_RST);

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

// uint16_t readBatteryPercent() {
//     uint16_t vbat;

//     if (!fona.getBattPercent(&vbat)) {
//         printLong("Failed to read Batt");
//         return 0;
//     } else {
//         return vbat;
//     }
// }

// turn on GPRS first before tranmitting data
// GPRS should only be turned on once
int turnGPRS() {
    if (!check_network()) {
        GPRS_enabled = 0;
        printLong("Failed to enable GPRS");
        return 0;
    }
    // try to turn off first
    fona.enableGPRS(false);
    if (fona.enableGPRS(true)) {
        GPRS_enabled = 1;
        printLong("GPRS Enabled!");
    } else {
        GPRS_enabled = 0;
        printLong("Failed to enable GPRS");
    }
    return 1;
}

// gsm initialization
void GSM_init() {
    fonaSerial->begin(4800);
    while (!fona.begin(*fonaSerial)) {
        printLong("Couldn't find FONA, retry in 2s!\0");
        fonaSerial->begin(4800);
    }

    // uint8_t type = fona.type();
    // lcd.clear();
    // lcd.print("FONA is OK");
    // lcd.setCursor(0, 1);
    // switch (type) {
    //     case FONA3G_A:
    //         lcd.print("FONA 3G American");
    //         break;
    //     default:
    //         lcd.print("???");
    //         break;
    // }
    // delay(1000);

    fona.setGPRSNetworkSettings(F("wireless.dish.com"));

    // Optionally configure HTTP gets to follow redirects over SSL.
    // Default is not to follow SSL redirects, however if you uncomment
    // the following line then redirects over SSL will be followed.
    // fona.setHTTPSRedirect(true);

    // try to enable GPRS
    turnGPRS();
}

// post data to website
int postData() {
    // Post data to website via 3G
    char URL[150] = "";  // Make sure this buffer is long enough for your
                         // request URL

    // https://www.dweet.io/dweet/for/gsm_mod
    sprintf(URL,
            "GET /dweet/for/gsm_mod?peopleCount=%s&timeStamp=%s "
            "HTTP/1.1\r\nHost: dweet.io\r\nContent-Length: 10\r\n\r\n",
            count, timeStamp);
    return fona.postData3G(URL);
}

// check if there's signal
int check_network() {
    // read the network/cellular status
    uint8_t n = fona.getNetworkStatus();
    if (n == 1) {
        // registered
        return 1;
    }
    return 0;
}

// send count request to pi through I2C
void Send_Count() {
    sprintf(msg, "count\n");
    Wire.beginTransmission(RPi_slave);
    Wire.write(msg);
    Wire.endTransmission();

    // printLong("Send request to pi!");
}

// read timestamp from rtc
void read_time() {
    // request timeStamp from rtc
    now = rtc.now();
    uint16_t year = now.year();
    uint8_t month = now.month();
    uint8_t day = now.day();
    uint8_t hour = now.hour();
    uint8_t minute = now.minute();

    sprintf(timeStamp, "%d/%d/%d/%d/%d\0", year, month, day, hour, minute);
    sprintf(timeDisplay, "%d/%d/%d %d:%d\0", year, month, day, hour, minute);

    // printLong("Read count number!");
}

// read count from pi through I2C
void Read_Count() {
    Wire.requestFrom(RPi_slave, 2);
    int idx = 0;
    while (Wire.available()) {
        char c = Wire.read();
        count[idx++] = c;
    }
    count[idx] = '\0';
}

// main function to be called by timer
bool function_to_call(void*) {
    // check time
    now = rtc.now();
    if (now.hour() == SLEEP && RPi_on) {  // tell the RPi to shutdown
        sprintf(msg, "down\n");
        Wire.beginTransmission(RPi_slave);
        Wire.write(msg);
        Wire.endTransmission();
        RPi_on = false;
    } else if (now.hour() == WAKE_UP &&
               !RPi_on) {              // wake up the RPi by shorting
        digitalWrite(AWAKE_PIN, HIGH);  // short the GPIO3 for 0.5s
        delay(500);
        digitalWrite(AWAKE_PIN, LOW);
        RPi_on = true;
    } else if (RPi_on) {  // request number of people
        Send_Count();
        read_time();
        Read_Count();

        // display people count and timestamp
        lcd.clear();
        lcd.print("#People " + String(count));
        lcd.setCursor(0, 1);
        lcd.print(String(timeDisplay));
        delay(4000);

        if (!GPRS_enabled)
            turnGPRS();

        if (check_network() && postData()) {
            printLong("Transmitted successfully!");
        } else {
            printLong("Failed to transmit!");
        }
    }
    return true;
}

void setup() {
    // set up the LCD's number of columns and rows:
    lcd.begin(16, 2);
    printLong("Initializing....");

    // GPIO setting for wake up
    pinMode(AWAKE_PIN, OUTPUT);
    digitalWrite(AWAKE_PIN, LOW);

    // RPi Slave Init
    Wire.begin();
    Wire.requestFrom(RPi_slave,
                     2);  // Dump unreaded data on I2C line, necessary!
    while (Wire.available()) {
        char c = Wire.read();
    }
    // printLong("Raspberry Pi initialized!");

    /* RTC Init*/
    rtc.begin();
    if (!rtc.initialized() || rtc.lostPower()) {
        Serial.println("RTC is NOT initialized, let's set the time!");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    rtc.start();
    // printLong("RTC initialized!");

    /* GSM Init*/
    // printLong("Initializing FONA...");
    GSM_init();

    printLong("Finish initialization!");

    // call the function every millisc
    timer.every(20000, function_to_call);
}

void loop() 
{
    timer.tick();  // tick the timer
}
