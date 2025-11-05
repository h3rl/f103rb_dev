import os
import sys
import serial
import serial.tools.list_ports
import struct
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
        self.win = pg.GraphicsLayoutWidget(show=True, title="Real-time Kalman Data")

        # --- Plot 1: Angles
        self.angle_plot = self.win.addPlot(title="Angles (pitch & roll)")
        self.angle_plot.showGrid(x=True, y=True)
        self.angle_plot.addLegend()
        self.pitch_curve = self.angle_plot.plot(pen=pg.mkPen('r', width=2), name='Pitch')
        self.roll_curve = self.angle_plot.plot(pen=pg.mkPen('g', width=2), name='Roll')
        self.win.nextRow()

        # --- Plot 2: Biases
        self.bias_plot = self.win.addPlot(title="Biases (pitch & roll)")
        self.bias_plot.showGrid(x=True, y=True)
        self.bias_plot.addLegend()
        self.bpitch_curve = self.bias_plot.plot(pen=pg.mkPen('r', width=2), name='BPitch')
        self.broll_curve = self.bias_plot.plot(pen=pg.mkPen('g', width=2), name='BRoll')

        # Data buffers
        self.maxlen = 500
        self.pitch_data = deque(maxlen=self.maxlen)
        self.roll_data = deque(maxlen=self.maxlen)
        self.bpitch_data = deque(maxlen=self.maxlen)
        self.broll_data = deque(maxlen=self.maxlen)

        # Timer for updates
        self.timer = QtCore.QTimer()
        self.timer.timeout.connect(self.update)
        self.timer.start(10)  # 100 Hz

    def update(self):
        while self.ser.in_waiting:
            b = self.ser.read(1)
            if b == b'\xAA':  # header byte
                raw = self.ser.read(16)  # 4 floats
                if len(raw) < 16:
                    break
                try:
                    pitch, roll, bpitch, broll = struct.unpack('<4f', raw)
                except struct.error:
                    continue

                self.pitch_data.append(pitch)
                self.roll_data.append(roll)
                self.bpitch_data.append(bpitch)
                self.broll_data.append(broll)
                break  # process one packet per timer tick

        # Update plots
        self.pitch_curve.setData(self.pitch_data)
        self.roll_curve.setData(self.roll_data)
        self.bpitch_curve.setData(self.bpitch_data)
        self.broll_curve.setData(self.broll_data)

    def run(self):
        QtWidgets.QApplication.instance().exec_()

# --- Main
if __name__ == "__main__":
    plotter = SerialPlotter()
    plotter.run()
