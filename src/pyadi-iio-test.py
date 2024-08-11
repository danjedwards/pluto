import adi
import zmq
import time
import matplotlib.pyplot as plt
import numpy as np
from scipy import signal

# Zmq
ctx = zmq.Context()
td_socket = ctx.socket(zmq.PUB)
td_socket.bind("tcp://localhost:5555")

fd_socket = ctx.socket(zmq.PUB)
fd_socket.bind("tcp://localhost:5556")

# Create radio
sdr = adi.Pluto()

# Configure properties
sdr.rx_rf_bandwidth = 4000000
sdr.rx_lo = 2000000000
sdr.tx_lo = 2000000000
sdr.tx_cyclic_buffer = True
sdr.tx_hardwaregain_chan0 = -30
sdr.gain_control_mode_chan0 = "slow_attack"

# Read properties
print("RX LO %s" % (sdr.rx_lo))

# Create a sinewave waveform
fs = int(sdr.sample_rate)
print(f"SAMPLING RATE = {fs}")
N = 1024
fc = int(5000000 / (fs / N)) * (fs / N)
# fc = 2000000
print(fc)
ts = 1 / float(fs)
t = np.arange(0, N * ts, ts)
i = np.cos(2 * np.pi * t * fc) * 2 ** 14
q = np.sin(2 * np.pi * t * fc) * 2 ** 14
iq = i + 1j * q

# Send data
sdr.tx(iq)

# # Collect data
# for r in range(100):
#     x = sdr.rx()
#     f, Pxx_den = signal.periodogram(x, fs)
#     plt.clf()
#     plt.semilogy(f, Pxx_den)
#     plt.ylim([1e-7, 1e2])
#     plt.xlabel("frequency [Hz]")
#     plt.ylabel("PSD [V**2/Hz]")
#     plt.draw()
#     plt.pause(0.05)
#     time.sleep(0.1)

# plt.show()

# Send rx over zmq socket
while True:
    x = sdr.rx()
    f, Pxx_den = signal.periodogram(x, fs)
    midpoint = Pxx_den.shape[0] // 2
    Pxx_den = np.append(Pxx_den[midpoint:], Pxx_den[:midpoint])
    td_socket.send(x.tobytes())
    fd_socket.send(Pxx_den.tobytes())
    time.sleep(0.1)