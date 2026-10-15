"""
tmac_ds - driver MicroPython para o contador TMAC via SPI.

Palavra de configuracao, 10 bits:
    [9]=1  [8]=reset  [7]=out  [6:4]=level  [3:0]=channel

Frame de leitura, 18 bytes:
    t1 | t2 | channel | ms | flags
    flags: [15]=ovf t1  [14]=ovf t2  [13:2]=ovf channels  [1]=ovf time  [0]=1
"""

from machine import Pin, SPI

__version__ = "0.1.0"

FRAME_LEN = 18


class TmacDS:
    def __init__(self, spi=None, cs=5, spi_id=0, baudrate=1_000_000,
                 sck=2, mosi=3, miso=4):
        if spi is None:
            spi = SPI(spi_id, baudrate=baudrate, polarity=0, phase=0,
                      sck=Pin(sck), mosi=Pin(mosi), miso=Pin(miso))
        self.spi = spi
        self.cs = cs if isinstance(cs, Pin) else Pin(cs, Pin.OUT, value=1)
        self.cs.value(1)

    def config(self, channel, level, out_trigger=0, reset=0):
        if not 0 <= channel <= 15:
            raise ValueError("channel deve estar entre 0 e 15")
        if not 0 <= level <= 7:
            raise ValueError("level deve estar entre 0 e 7")
        word = ((1 << 9) | (bool(reset) << 8) | (bool(out_trigger) << 7)
                | (level << 4) | channel)
        self.cs.value(0)
        try:
            self.spi.write(bytes([(word >> 2) & 0xFF, (word << 6) & 0xFF]))
        finally:
            self.cs.value(1)

    def reset(self, channel=0, level=0):
        self.config(channel, level, reset=1)

    def read_raw(self):
        self.cs.value(0)
        try:
            return self.spi.read(FRAME_LEN, 0x00)
        finally:
            self.cs.value(1)

    def read_counters(self):
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

    def start_aquisition(level, out_trigger=0, freq_aquisition):
        TmacDS.config(0, level, out_trigger, 1)
        TmacDS.read_counters() #aqui tenho de estrair so o counter do chanal e o respectivo tempo
        i=1
        priodo = 1/freq_aquisition
        while (True):
            tic
            for i in range[1:11]:
                TmacDS.config(i, level, out_trigger, 0)
                TmacDS.read_counters() #aqui tenho de estrair so o counter do chanal e o respectivo tempo
                i+=1
            TmacDS.read_counters() #aqui tenho de extrair os counters dos triggers e dos respectivos tempos
            tac
            tempo de ler counters 
            delay(priodo-tempo de ler counters)

        

