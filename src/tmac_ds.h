/*
 * startAquisition is a convenience for bring-up only: it never returns and it
 * prints CSV text to Serial. For anything real, use config and readCounters
 * and write your own loop.
 */

#ifndef TMAC_DS_H
#define TMAC_DS_H

#include <Arduino.h>
#include <SPI.h>

#define TMAC_FRAME_LEN 18
#define TMAC_CHANNELS  12

/* The FPGA ceiling: half an SCK period must cover two cycles of the 100 MHz
   clki. */
#define TMAC_SCK_MAX_HZ 25000000UL

/* out: what reaches the FPGA's out pin. Both trigger counters always run,
   whatever this is set to. */
enum TmacOut : uint8_t {
    TMAC_MAJORITY  = 0,
    TMAC_ADJACENCY = 1
};

/* reset: zeroes the 12 channel counters, the 2 trigger counters and the time
   base. */
enum TmacReset : uint8_t {
    TMAC_NO_RESET = 0,
    TMAC_RESET    = 1
};

struct TmacCounters {
    uint32_t trigger1;
    uint32_t trigger2;
    uint32_t channel;
    uint32_t ms;
    uint16_t ovfCh;         // 12 bits, one per channel
    bool     ovfT1;
    bool     ovfT2;
    bool     ovfTime;
    bool     valid;         // end of frame marker; false means the master lost
                            // byte alignment and the rest is junk
};

class TmacDS {
public:
    // The board's default SPI pins. Works on any board.
    void begin(uint8_t cs, uint32_t hz = 1000000);

#if defined(ESP32) || defined(ESP8266) || defined(ARDUINO_ARCH_RP2040)
    // Chosen SPI pins. Only on boards whose core allows it.
    void begin(uint8_t cs, int8_t sck, int8_t miso, int8_t mosi,
               uint32_t hz = 1000000);
#endif

    // One word with all three fields. level is 0 to 7: the majority trigger
    // fires with level+1 or more active channels. channel selects which of the
    // 12 per channel counters the readout reports.
    void config(uint8_t channel, uint8_t level, TmacOut out = TMAC_MAJORITY,
                TmacReset reset = TMAC_NO_RESET);

    // Zero every counter and the time base. Takes the configuration to leave
    // in place, because the word carries all three fields together.
    void reset(uint8_t channel = 0, uint8_t level = 0,
               TmacOut out = TMAC_MAJORITY);

    // The 18 frame bytes as they arrive.
    void readRaw(uint8_t *rx);

    // The same frame, decoded.
    void readCounters(TmacCounters &c);

    // Bring-up only. See the warning at the top of this file.
    void startAquisition(uint8_t level, TmacOut out, uint16_t freqHz);

private:
    uint8_t     _cs;
    SPISettings _spi;

    void sendWord(uint8_t channel, uint8_t level, TmacOut out, TmacReset reset);
};

#endif
