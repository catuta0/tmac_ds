// Uma leitura por segundo do canal 0, com o trigger de maioria a disparar
// com 4 ou mais canais activos.
//
// Ligacao usada aqui, ESP32 ao header GPIO1 da iCE40HX8K-EVB:
//   SCK   GPIO18  ->  pino 11
//   MOSI  GPIO23  ->  pino 13
//   MISO  GPIO19  <-  pino 7
//   CS    GPIO5   ->  pino 9
//   GND           ->  pino 2, 4 ou 8
// Confirma contra a tua ligacao antes de correr.

#include <tmac_ds.h>

TmacDS tmac;

void setup()
{
    Serial.begin(115200);
    delay(300);

#if defined(ESP32)
    tmac.begin(5, 18, 19, 23);      // CS, SCK, MISO, MOSI
#else
    tmac.begin(10);                 // CS, SPI nos pinos por omissao da placa
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
        Serial.println(F("frame invalido"));
    }

    delay(1000);
}
