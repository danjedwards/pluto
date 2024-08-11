import pyqtgraph as pg
import zmq
import numpy as np

from PyQt5.QtCore import (
    QObject,
    QThread,
    Qt,
    pyqtSignal
)

from PyQt5.QtWidgets import ( 
    QApplication,
    QMainWindow,
    QWidget,
    QVBoxLayout,
    QHBoxLayout,
    QPushButton
)

class ZmqWorker(QObject):
    data = pyqtSignal(np.ndarray)
    
    def __init__(self, addr):
        super().__init__()
        self.ctx    = zmq.Context()
        self.socket = self.ctx.socket(zmq.SUB)
        self.socket.connect(addr)
        self.socket.setsockopt(zmq.SUBSCRIBE, b"")

    def run(self):
        while True:
            buf = self.socket.recv()
            self.data.emit(np.frombuffer(buf, dtype=np.float64))

class ZmqWorkerWrapper(QObject):
    data = pyqtSignal(np.ndarray)

    def __init__(self, worker):
        super().__init__()
        self.worker = worker

    def start(self):
        self.worker_thread = QThread()
        self.worker.moveToThread(self.worker_thread)
        
        self.worker_thread.started.connect(self.worker.run)
        self.worker.data.connect(self.data.emit)

        self.worker_thread.start()

class TimeDomainPlot(QWidget):
    def __init__(self):
        super().__init__()

        # Vars
        self.plot = None

        # Widgets
        self.line_graph = pg.PlotWidget()

        # Layout
        layout = QVBoxLayout()
        layout.addWidget(self.line_graph)
        self.setLayout(layout)

    def set_data(self, y):
        t = np.linspace(0, y.shape[0], y.shape[0])
        if self.plot is None:
            self.plot = self.line_graph.plotItem.plot(t, y)
        else:
            self.plot.setData(t, y)


class PlutoGui(QMainWindow):
    def __init__(self):
        super().__init__()
        
        # Config
        self.setWindowTitle("Pluto Gui")
        
        # Workers
        self.recv_worker = ZmqWorkerWrapper(ZmqWorker("tcp://localhost:5555"))
        self.recv_worker.start()

        # Widgets
        graph = TimeDomainPlot()
        btn   = QPushButton("Dummy Button")

        # Signals and Slots
        self.recv_worker.data.connect(graph.set_data)

        # Layout
        layout = QVBoxLayout()
        layout.addWidget(graph)
        layout.addWidget(btn)
        
        layout_widget = QWidget()
        layout_widget.setLayout(layout)
        self.setCentralWidget(layout_widget)

if __name__ == "__main__":
    import sys
    
    app = QApplication(sys.argv)
    gui = PlutoGui()
    gui.show()
    app.exec()
