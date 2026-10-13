import time
from tmac_ds import TmacDS

# Pinos por defeito: SPI0, SCK=2, MOSI=3, MISO=4, CS=5
tmac = TmacDS()

tmac.config(channel=0, level=3, out_trigger=1, reset=1)

while True:
    c = tmac.read_counters()
    if c["valid"]:
        print(c["trigger1"], c["trigger2"], c["channel"], c["ms"])
    time.sleep(1)
