#include "tmac_ds.h"

void TmacDS::begin(uint8_t cs, uint32_t hz)
{
    _cs  = cs;
    _spi = SPISettings(hz, MSBFIRST, SPI_MODE0);

    pinMode(_cs, OUTPUT);
    digitalWrite(_cs, HIGH);
    SPI.begin();
}

#if defined(ESP32) || defined(ESP8266) || defined(ARDUINO_ARCH_RP2040)
void TmacDS::begin(uint8_t cs, int8_t sck, int8_t miso, int8_t mosi,
                   uint32_t hz)
{
    _cs  = cs;
    _spi = SPISettings(hz, MSBFIRST, SPI_MODE0);

    pinMode(_cs, OUTPUT);
    digitalWrite(_cs, HIGH);
    // The -1 keeps the library from touching a chip select of its own: CS is
    // driven by hand so each transaction gets exactly one pulse.
    SPI.begin(sck, miso, mosi, -1);
}
#endif

void TmacDS::sendWord(uint8_t channel, uint8_t level, TmacOut out,
                      TmacReset reset)
{
    uint16_t word = (1u << 9)
                  | ((uint16_t)reset << 8)
                  | ((uint16_t)out   << 7)
                  | ((uint16_t)level << 4)
                  |  (uint16_t)channel;

    // Left align the 10 bits inside the two bytes. The six padding bits must
    // be zero, otherwise the FPGA reads one of them as the start bit of
    // another word.
    uint16_t packed = word << 6;

    SPI.beginTransaction(_spi);
    digitalWrite(_cs, LOW);
    delayMicroseconds(10);
    SPI.transfer((uint8_t)(packed >> 8));
    SPI.transfer((uint8_t)(packed & 0xFF));
    delayMicroseconds(10);
    digitalWrite(_cs, HIGH);
    SPI.endTransaction();
    delayMicroseconds(20);          // the FPGA applies the reset on this edge
}

void TmacDS::config(uint8_t channel, uint8_t level, TmacOut out,
                    TmacReset reset)
{
    sendWord(channel, level, out, reset);

    if (reset == TMAC_RESET) {
        // Clear bit 8 in the word the FPGA keeps. Without this the reset
        // request stays armed, and a later transaction that starts a reception
        // by accident would fire a second reset. No reset fires on this word:
        // the FPGA samples the bit when the reception ends, and by then this
        // word has already replaced the previous one.
        sendWord(channel, level, out, TMAC_NO_RESET);
    }
}

void TmacDS::reset(uint8_t channel, uint8_t level)
{
    config(channel, level, TMAC_MAJORITY, TMAC_RESET);
}

void TmacDS::readRaw(uint8_t *rx)
{
    SPI.beginTransaction(_spi);
    digitalWrite(_cs, LOW);
    delayMicroseconds(10);
    // The 0x00 holds MOSI low: a one there would be taken as the start of a
    // configuration word.
    for (uint8_t i = 0; i < TMAC_FRAME_LEN; i++) rx[i] = SPI.transfer(0x00);
    delayMicroseconds(10);
    digitalWrite(_cs, HIGH);
    SPI.endTransaction();
    delayMicroseconds(10);
}

static uint32_t u32be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

void TmacDS::readCounters(TmacCounters &c)
{
    uint8_t rx[TMAC_FRAME_LEN];
    readRaw(rx);

    uint16_t f = ((uint16_t)rx[16] << 8) | rx[17];

    c.trigger1 = u32be(rx + 0);
    c.trigger2 = u32be(rx + 4);
    c.channel  = u32be(rx + 8);
    c.ms       = u32be(rx + 12);
    c.ovfT1    = (f >> 15) & 1;
    c.ovfT2    = (f >> 14) & 1;
    c.ovfCh    = (f >>  2) & 0x0FFF;
    c.ovfTime  = (f >>  1) & 1;
    c.valid    =  f        & 1;
}

// The Arduino Print class cannot print 64 bits on 8-bit boards, so the number
// is formatted by hand.
static void printU64(uint64_t v)
{
    char  buf[21];
    char *p = buf + 20;

    *p = '\0';
    do { *--p = (char)('0' + (v % 10)); v /= 10; } while (v);
    Serial.print(p);
}

void TmacDS::startAquisition(uint8_t level, TmacOut out, uint16_t freqHz)
{
    const uint32_t periodMs = 1000UL / (freqHz > 0 ? freqHz : 1);
    uint32_t       wraps[TMAC_CHANNELS] = {0};
    uint64_t       wrapsT1 = 0, wrapsT2 = 0;
    TmacCounters   c;

    Serial.println(F("ms,ch,count,trigger1,trigger2"));
    config(0, level, out, TMAC_RESET);

    for (;;) {
        uint32_t start = millis();

        for (uint8_t ch = 0; ch < TMAC_CHANNELS; ch++) {
            config(ch, level, out, TMAC_NO_RESET);
            readCounters(c);

            // The FPGA clears a channel flag only on the readout of the
            // selected channel, so look at this channel's bit alone. The
            // trigger flags are cleared on every readout, so they have to be
            // accumulated on each channel.
            if ((c.ovfCh >> ch) & 1) wraps[ch]++;
            wrapsT1 += c.ovfT1;
            wrapsT2 += c.ovfT2;

            Serial.print(c.ms);   Serial.print(',');
            Serial.print(ch);     Serial.print(',');
            printU64(((uint64_t)wraps[ch] << 32) + c.channel); Serial.print(',');
            printU64((wrapsT1 << 32) + c.trigger1);            Serial.print(',');
            printU64((wrapsT2 << 32) + c.trigger2);            Serial.println();
        }

        uint32_t elapsed = millis() - start;
        if (elapsed < periodMs) delay(periodMs - elapsed);
    }
}
