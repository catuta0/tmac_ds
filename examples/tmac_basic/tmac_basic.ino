// One reading per second of channel 0, with the majority trigger firing on
// four or more active channels.
//
// Wiring used here, ESP32 to the GPIO1 header of the iCE40HX8K-EVB:
//   SCK   GPIO18  ->  pin 11
//   MOSI  GPIO23  ->  pin 13
//   MISO  GPIO19  <-  pin 7
//   CS    GPIO5   ->  pin 9
//   GND           ->  pin 2, 4 or 8
// Check this against your own wiring before running.

#include <tmac_ds.h>

TmacDS tmac;

void setup()
{
    Serial.begin(115200);
    delay(300);

#if defined(ESP32)
    tmac.begin(5, 18, 19, 23);      // CS, SCK, MISO, MOSI
#else
    tmac.begin(10);                 // CS, SPI on the board's default pins
#endif

    tmac.config(0, 3, TMAC_MAJORITY, TMAC_RESET);
}

void loop()
{
    TmacCounters c;

    tmac.readCounters(c);

    if (c.valid) {
        Serial.print(c.ms);       Serial.print(F(" ms  t1="));
        Serial.print(c.trigger1); Serial.print(F("  t2="));
        Serial.print(c.trigger2); Serial.print(F("  ch0="));
        Serial.println(c.channel);
    } else {
        Serial.println(F("invalid frame"));
    }

    delay(1000);
}
