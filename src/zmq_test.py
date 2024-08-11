import zmq
import time
import numpy as np

ctx = zmq.Context()
socket = ctx.socket(zmq.PUB)
socket.bind("tcp://localhost:5555")

while True:
    socket.send(np.random.rand(100).tobytes())
    time.sleep(.1)