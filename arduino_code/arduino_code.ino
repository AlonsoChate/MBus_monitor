#include <LiquidCrystal.h>
#include "Adafruit_FONA.h"

// lcd init
const int rs = 12, en = 11, d4 = 5, d5 = 6, d6 = 7, d7 = 8;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// gsm part
#define FONA_RX 2
#define FONA_TX 3
#define FONA_RST 4

char replybuffer[255];

#include <SoftwareSerial.h>
SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
SoftwareSerial* fonaSerial = &fonaSS;

Adafruit_FONA_3G fona = Adafruit_FONA_3G(FONA_RST);
char imei[16] = {0};  // MUST use a 16 character buffer for IMEI!

// print long sentence on lcd screen, delay 2s each screen
void printLong(char* data) {
    int lines = ceil((double)strlen(data) / 16.0);
    char line[16];
    for (int i = 0; i < lines; i++) {
        if (i % 2 == 0)
            lcd.clear();
        strncpy(line, data + i * 16, 16);
        lcd.setCursor(0, i % 2);
        lcd.print(line);
        if (i % 2 == 1)
            delay(2000);
    }
}

uint16_t readBatteryPercent() {
    // read the battery voltage
    uint16_t vbat;
    // if (!fona.getBattVoltage(&vbat)) {
    //     Serial.println(F("Failed to read Batt"));
    // } else {
    //     Serial.print(F("VBat = "));
    //     Serial.print(vbat);
    //     Serial.println(F(" mV"));
    // }

    // read battery percentage
    if (!fona.getBattPercent(&vbat)) {
        printLong("Failed to read Batt");
        return 0;
    } else {
        return vbat;
    }
}

void readRSSI() {
    // read the RSSI
    uint8_t n = fona.getRSSI();
    int8_t r;

    Serial.print(F("RSSI = "));
    Serial.print(n);
    Serial.print(": ");
    if (n == 0)
        r = -115;
    if (n == 1)
        r = -111;
    if (n == 31)
        r = -52;
    if ((n >= 2) && (n <= 30)) {
        r = map(n, 2, 30, -110, -54);
    }
    Serial.print(r);
    Serial.println(F(" dBm"));
}

void readNetworkStat() {
    // read the network/cellular status
    uint8_t n = fona.getNetworkStatus();
    Serial.print(F("Network status "));
    Serial.print(n);
    Serial.print(F(": "));
    if (n == 0)
        Serial.println(F("Not registered"));
    if (n == 1)
        Serial.println(F("Registered (home)"));
    if (n == 2)
        Serial.println(F("Not registered (searching)"));
    if (n == 3)
        Serial.println(F("Denied"));
    if (n == 4)
        Serial.println(F("Unknown"));
    if (n == 5)
        Serial.println(F("Registered roaming"));
}

void turnGPRS() {
    // try to turn off first
    fona.enableGPRS(false);
    if (fona.enableGPRS(true)) {
        printLong("GPRS Enabled");
    } else {
        printLong("Failed to enable GPRS, please retry");
    }
    delay(1000);
}

void postData() {
    // Post data to website via 3G
    float temperature = 12;  // Change this to suit your needs
    uint16_t battLevel =
        45;  // Just for testing. Use the read battery function instead

    // Construct URL and post the data to the web API
    // Create char buffers for the floating point numbers for sprintf
    char URL[150];  // Make sure this buffer is long enough for your
                    // request URL
    char tempBuff[16];
    char battLevelBuff[16];

    // Format the floating point numbers as needed
    dtostrf(temperature, 1, 2,
            tempBuff);  // float_val, min_width, digits_after_decimal,
                        // char_buffer
    dtostrf(battLevel, 1, 0, battLevelBuff);

    // Use IMEI as device ID in this example:
    sprintf(URL,
            "GET /dweet/for/gsm_mod?people_count=%s&battery_percent=%s "
            "HTTP/1.1\r\nHost: dweet.io\r\nContent-Length: 10\r\n\r\n",
            tempBuff, battLevelBuff);

    if (!fona.postData3G(URL))  // Uncomment this if you have FONA 3G!
        Serial.println(F("Failed to post data to website..."));
}

void setup() {
    // set up the LCD's number of columns and rows:
    lcd.begin(16, 2);

    Serial.begin(115200);
    lcd.clear();
    lcd.print("Initializing....");
    delay(500);

    fonaSerial->begin(4800);
    while (!fona.begin(*fonaSerial)) {
        lcd.clear();
        printLong("Couldn't find FONA, retry in 2s");
        fonaSerial->begin(4800);
    }

    uint8_t type = fona.type();
    lcd.clear();
    lcd.print("FONA is OK");
    delay(1000);
    lcd.clear();

    switch (type) {
        case FONA3G_A:
            lcd.print("FONA 3G American");
            break;
        default:
            lcd.print("???");
            break;
    }

    // Print module IMEI number.
    uint8_t imeiLen = fona.getIMEI(imei);
    if (imeiLen > 0) {
        lcd.setCursor(0, 1);
        lcd.print(imei);
        delay(2000);
    }

    fona.setGPRSNetworkSettings(F("wireless.dish.com"));

    // Optionally configure HTTP gets to follow redirects over SSL.
    // Default is not to follow SSL redirects, however if you uncomment
    // the following line then redirects over SSL will be followed.
    // fona.setHTTPSRedirect(true);

    // enable GPRS
    //turnGPRS();
}

void loop() {
    lcd.clear();
    lcd.print(readBatteryPercent());
    delay(2000);

    // get current timestamp from rtc

    // send request to pi to count people

    // pack data

    // send data through gsm

    // store data in sd card
}
