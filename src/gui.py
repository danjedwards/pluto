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

FS = 30719999
FULL_SCALE = 100.

class ZmqWorker(QObject):
    data = pyqtSignal(np.ndarray)
    
    def __init__(self, addr, dtype):
        super().__init__()
        self.ctx    = zmq.Context()
        self.socket = self.ctx.socket(zmq.SUB)
        self.socket.connect(addr)
        self.socket.setsockopt(zmq.SUBSCRIBE, b"")

        self.dtype = dtype

    def run(self):
        while True:
            buf = self.socket.recv()
            self.data.emit(np.frombuffer(buf, dtype=self.dtype))

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

        # Widgets
        self.plot_widget = pg.PlotWidget()
        self.plot_widget.setTitle("Time Domain")
        self.plot_widget.setLabel("left", "Amplitude")
        self.plot_widget.setLabel("bottom", "Time (s)")
        
        self.plot_item  = None

        # Layout
        layout = QVBoxLayout()
        layout.addWidget(self.plot_widget)
        self.setLayout(layout)

    def set_data(self, iq):
        y = (iq.real + iq.imag) / 2
        t = np.linspace(0, y.shape[0] / FS, y.shape[0])
        if self.plot_item is None:
            self.plot_item = self.plot_widget.plotItem.plot(t, y)
        else:
            self.plot_item.setData(t, y)

class FreqDomainPlot(QWidget):
    def __init__(self):
        super().__init__()

        # Widgets
        self.plot_widget = pg.PlotWidget()
        self.plot_widget.setTitle("Freq Domain")
        self.plot_widget.setLabel("left", "Amplitude")
        self.plot_widget.setLabel("bottom", "Frequency")
        self.plot_widget.getPlotItem().setLogMode(y=True)
        
        self.plot_item  = None

        # Layout
        layout = QVBoxLayout()
        layout.addWidget(self.plot_widget)
        self.setLayout(layout)

    def set_data(self, pxx):
        f = np.linspace(-FS/2, FS/2, pxx.shape[0])
        if self.plot_item is None:
            self.plot_item = self.plot_widget.plotItem.plot(f, pxx)
        else:
            self.plot_item.setData(f, pxx)

class PlutoGui(QMainWindow):
    def __init__(self):
        super().__init__()
        
        # Config
        self.setWindowTitle("Pluto Gui")
        
        # Workers
        self.time_domain_worker = ZmqWorkerWrapper(ZmqWorker("tcp://localhost:5555", np.complex128))
        self.time_domain_worker.start()

        self.freq_domain_worker = ZmqWorkerWrapper(ZmqWorker("tcp://localhost:5556", np.float64))
        self.freq_domain_worker.start()

        # Widgets
        time_domain_plot = TimeDomainPlot()
        freq_domain_plot = FreqDomainPlot()

        # Signals and Slots
        self.time_domain_worker.data.connect(time_domain_plot.set_data)
        self.freq_domain_worker.data.connect(freq_domain_plot.set_data)

        # Layout
        layout = QVBoxLayout()
        layout.addWidget(time_domain_plot)
        layout.addWidget(freq_domain_plot)
        
        layout_widget = QWidget()
        layout_widget.setLayout(layout)
        self.setCentralWidget(layout_widget)

if __name__ == "__main__":
    import sys
    
    app = QApplication(sys.argv)
    gui = PlutoGui()
    gui.show()
    app.exec()
