"""
tmac_ds - MicroPython driver for the TMAC counter over SPI.

start_aquisition is a convenience for bring-up only: it never returns on its
own and it writes to a file. For anything real, use config and read_counters
and write your own loop.
"""

from machine import Pin, SPI
import time

__version__ = "0.2.0"

FRAME_LEN = 18
CHANNELS = 12

# The FPGA ceiling: half an SCK period must cover two cycles of the 100 MHz
# clki.
SCK_MAX_HZ = 25_000_000

# out_trigger: what reaches the out pin. Both trigger counters always run,
# whatever this is set to.
MAJORITY = 0
ADJACENCY = 1

# reset: zeroes the 12 channel counters, the 2 trigger counters and the time
# base.
NO_RESET = 0
RESET = 1


class TmacDS:
    def __init__(self, spi=None, cs=5, spi_id=0, baudrate=1_000_000,
                 sck=2, mosi=3, miso=4):
        if spi is None:
            spi = SPI(spi_id, baudrate=baudrate, polarity=0, phase=0,
                      sck=Pin(sck), mosi=Pin(mosi), miso=Pin(miso))
        self.spi = spi
        self.cs = cs if isinstance(cs, Pin) else Pin(cs, Pin.OUT, value=1)
        self.cs.value(1)

    def config(self, channel, level, out_trigger=MAJORITY, reset=NO_RESET):
        """Send all three fields at once, in a single word."""
        if not 0 <= channel <= 15:
            raise ValueError("channel must be 0 to 15")
        if not 0 <= level <= 7:
            raise ValueError("level must be 0 to 7")

        word = ((1 << 9) | (bool(reset) << 8) | (bool(out_trigger) << 7)
                | (level << 4) | channel)
        self.cs.value(0)
        try:
            # Left align the 10 bits inside the two bytes. The six padding bits
            # must be zero, otherwise the FPGA reads one of them as the start
            # bit of another word.
            self.spi.write(bytes([(word >> 2) & 0xFF, (word << 6) & 0xFF]))
        finally:
            self.cs.value(1)

    def reset(self, channel=0, level=0, out_trigger=MAJORITY):
        """Zero every counter and the time base.

        Takes the configuration to leave in place, because the word carries all
        three fields together.
        """
        # One word is enough. The FPGA fires the reset on the falling edge of
        # its internal reciving flag, which only rises when MOSI is high on an
        # SCK edge, so a readout (MOSI held low throughout) never produces
        # another one. The reset bit staying set in the word the FPGA keeps is
        # harmless: exactly one reset per configuration transaction.
        self.config(channel, level, out_trigger, RESET)

    def read_raw(self):
        """The 18 frame bytes as they arrive."""
        self.cs.value(0)
        try:
            # The 0x00 holds MOSI low: a one there would be taken as the start
            # of a configuration word.
            return self.spi.read(FRAME_LEN, 0x00)
        finally:
            self.cs.value(1)

    def read_counters(self):
        """The same frame, decoded into a dict.

        A false "valid" means the master lost byte alignment and the rest of
        the fields are junk.
        """
        d = self.read_raw()
        f = (d[16] << 8) | d[17]
        return {
            "trigger1": int.from_bytes(d[0:4], "big"),
            "trigger2": int.from_bytes(d[4:8], "big"),
            "channel":  int.from_bytes(d[8:12], "big"),
            "ms":       int.from_bytes(d[12:16], "big"),
            "ovf_t1":   (f >> 15) & 1,
            "ovf_t2":   (f >> 14) & 1,
            "ovf_ch":   (f >> 2) & 0xFFF,
            "ovf_time": (f >> 1) & 1,
            "valid":    f & 1,
        }

    def start_aquisition(self, level, out_trigger=MAJORITY, freq_aquisition=1,
                         filename="tmac.csv"):
        """Bring-up convenience. Reset, then scan the 12 channels at
        freq_aquisition, writing to filename. Stops on Ctrl-C.

        One line per reading: ms,ch,count,trigger1,trigger2. Every count is
        continuous, with 2^32 added for each wrap of the counter.
        """
        period_ms = int(1000 / freq_aquisition)
        wraps = [0] * CHANNELS     # 2^32 wraps seen on each channel
        wraps_t1 = 0
        wraps_t2 = 0

        f = open(filename, "w")
        f.write("ms,ch,count,trigger1,trigger2\n")
        self.reset(0, level, out_trigger)

        try:
            while True:
                start = time.ticks_ms()

                for ch in range(CHANNELS):
                    self.config(ch, level, out_trigger, NO_RESET)
                    d = self.read_counters()
                    # The FPGA clears a channel flag only on the readout of the
                    # selected channel, so look at this channel's bit alone.
                    # The trigger flags are cleared on every readout, so they
                    # have to be accumulated on each channel.
                    if (d["ovf_ch"] >> ch) & 1:
                        wraps[ch] += 1
                    wraps_t1 += d["ovf_t1"]
                    wraps_t2 += d["ovf_t2"]
                    f.write("{},{},{},{},{}\n".format(
                        d["ms"], ch,
                        (wraps[ch] << 32) + d["channel"],
                        (wraps_t1 << 32) + d["trigger1"],
                        (wraps_t2 << 32) + d["trigger2"]))

                f.flush()

                elapsed = time.ticks_diff(time.ticks_ms(), start)
                if elapsed < period_ms:
                    time.sleep_ms(period_ms - elapsed)
        except KeyboardInterrupt:
            print("stopped")
        finally:
            f.close()
