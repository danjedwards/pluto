import pyqtgraph
import zmq

from PyQt5.QtCore import (
    QObject,
    QThread,
    pyqtSignal
)

from PyQt5.QtWidgets import ( 
    QApplication,
    QMainWindow,
    QWidget,
    QPushButton
)

class ZmqWorker(QObject):
    data = pyqtSignal(bytes)
    
    def __init__(self, addr):
        super().__init__()
        self.ctx    = zmq.Context()
        self.socket = self.ctx.socket(zmq.SUB)
        self.socket.connect(addr)
        self.running = False

    def run(self):
        while self.running:
            buf = self.socket.recv()
            self.data.emit(buf)

class ZmqWorkerWrapper(QObject):
    def __init__(self, worker):
        super().__init__()
        self.worker = worker

    def start(self):
        self.thread = QThread()
        self.worker.moveToThread(self.thread)
        
        self.thread.started.connect(self.worker.run)
        self.worker.data.connect(print)

        self.thread.start()

class PlutoGui(QMainWindow):
    def __init__(self):
        super().__init__()
        
        # Config
        self.setWindowTitle("Pluto Gui")
        
        # Workers
        ZmqWorkerWrapper()

        # Widgets
        btn = QPushButton("Press Me")

        # Signals and Slots


        # Layout
        self.setCentralWidget(btn)

if __name__ == "__main__":
    import sys
    
    app = QApplication(sys.argv)
    gui = PlutoGui()
    gui.show()
    app.exec()
