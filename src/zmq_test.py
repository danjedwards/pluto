import zmq
import time
import numpy as np

def pub(addr):
    ctx = zmq.Context()
    socket = ctx.socket(zmq.PUB)
    socket.bind(addr)

    while True:
        socket.send(np.random.rand(100).tobytes())
        time.sleep(.1)

def sub(addr):
    ctx = zmq.Context()
    socket = ctx.socket(zmq.SUB)
    socket.connect(addr)
    socket.setsockopt(zmq.SUBSCRIBE, b"")

    while True:
        print(socket.recv())

if __name__ == "__main__":    
    addr = "tcp://localhost:5555"
    # pub(addr)
    sub(addr)