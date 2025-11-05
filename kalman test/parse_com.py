import os
import sys
import serial
import serial.tools.list_ports
from collections import deque

# --- QT environment fix (must be before PyQtGraph imports)
os.environ["QT_QPA_PLATFORM_PLUGIN_PATH"] = os.path.join(
    os.path.dirname(sys.executable), "Library", "plugins", "platforms"
)
os.environ["QT_QPA_PLATFORM"] = "windows"

import pyqtgraph as pg
from pyqtgraph.Qt import QtCore, QtWidgets

# --- Find the virtual COM port
def find_virtual_com():
    ports = serial.tools.list_ports.comports()
    for port, desc, hwid in sorted(ports):
        if "Virtual COM Port" in desc:
            return serial.Serial(port, 115200, timeout=0.01)
    return None

# --- Real-time plotting class
class SerialPlotter:
    def __init__(self):
        self.ser = find_virtual_com()
        if not self.ser:
            print("No Virtual COM Port found.")
            sys.exit(1)
        print(f"Opened port: {self.ser.port}")

        # PyQtGraph setup
        self.app = QtWidgets.QApplication(sys.argv)
        self.win = pg.GraphicsLayoutWidget(show=True, title="Real-time Angle Data")
        self.plot = self.win.addPlot(title="Angle (deg)")
        self.plot.showGrid(x=True, y=True)
        self.curve = self.plot.plot(pen=pg.mkPen('y', width=2))
        self.data = deque(maxlen=500)  # keep last 500 samples

        # Timer for updates (10ms = 100 Hz refresh)
        self.timer = QtCore.QTimer()
        self.timer.timeout.connect(self.update)
        self.timer.start(10)

    def update(self):
        # Read all available lines from serial
        while self.ser.in_waiting:
            line = self.ser.readline().decode('utf-8', errors='ignore').strip()
            if not line:
                continue
            try:
                angle = float(line)
                self.data.append(angle)
            except ValueError:
                continue

        if self.data:
            self.curve.setData(self.data)

    def run(self):
        QtWidgets.QApplication.instance().exec_()

# --- Main
if __name__ == "__main__":
    plotter = SerialPlotter()
    plotter.run()
