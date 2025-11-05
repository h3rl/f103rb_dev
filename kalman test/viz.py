import os
import sys
import serial
import serial.tools.list_ports
import struct
import numpy as np
from collections import deque

# --- QT environment fix
os.environ["QT_QPA_PLATFORM_PLUGIN_PATH"] = os.path.join(
    os.path.dirname(sys.executable), "Library", "plugins", "platforms"
)
os.environ["QT_QPA_PLATFORM"] = "windows"

import pyqtgraph as pg
from pyqtgraph.Qt import QtCore, QtWidgets
import pyqtgraph.opengl as gl

# --- Find virtual COM port
def find_virtual_com():
    ports = serial.tools.list_ports.comports()
    for port, desc, hwid in sorted(ports):
        if "Virtual COM Port" in desc:
            return serial.Serial(port, 115200, timeout=0.01)
    return None

# --- 3D Plane Visualizer
class PlaneVisualizer:
    def __init__(self, plane_size=1.0):
        self.ser = find_virtual_com()
        if not self.ser:
            print("No Virtual COM Port found.")
            sys.exit(1)
        print(f"Opened port: {self.ser.port}")

        self.plane_size = plane_size
        self.pitch = 0.0
        self.roll = 0.0

        # PyQtGraph window
        self.app = QtWidgets.QApplication(sys.argv)
        self.win = gl.GLViewWidget()
        self.win.setWindowTitle("3D Plane Visualizer")
        self.win.setCameraPosition(distance=5)
        self.win.show()

        # Cube/plane vertices
        self.verts = np.array([
            [-0.5, -0.5, -0.05],
            [ 0.5, -0.5, -0.05],
            [ 0.5,  0.5, -0.05],
            [-0.5,  0.5, -0.05],
            [-0.5, -0.5,  0.05],
            [ 0.5, -0.5,  0.05],
            [ 0.5,  0.5,  0.05],
            [-0.5,  0.5,  0.05],
        ]) * self.plane_size

        # Cube faces
        self.faces = np.array([
            [0,1,2], [0,2,3],   # bottom
            [4,5,6], [4,6,7],   # top
            [0,1,5], [0,5,4],   # front
            [1,2,6], [1,6,5],   # right
            [2,3,7], [2,7,6],   # back
            [3,0,4], [3,4,7],   # left
        ])

        # Colors per face (RGBA)
        self.colors = np.array([[1,0,0,1]]*len(self.faces))

        # Create GLMeshItem
        self.mesh = gl.GLMeshItem(vertexes=self.verts, faces=self.faces,
                                  faceColors=self.colors, smooth=False,
                                  drawEdges=True, edgeColor=(1,1,1,1))
        self.win.addItem(self.mesh)

        # Timer for updates (50 Hz)
        self.timer = QtCore.QTimer()
        self.timer.timeout.connect(self.update)
        self.timer.start(20)

    def set_orientation(self, pitch_deg, roll_deg):
        pitch = np.radians(pitch_deg)
        roll = np.radians(roll_deg)

        Rx = np.array([
            [1,0,0],
            [0,np.cos(pitch), -np.sin(pitch)],
            [0,np.sin(pitch),  np.cos(pitch)]
        ])
        Ry = np.array([
            [np.cos(roll),0,np.sin(roll)],
            [0,1,0],
            [-np.sin(roll),0,np.cos(roll)]
        ])

        R = Ry @ Rx

        rotated = np.dot(self.verts, R.T)

        if np.all(np.isfinite(rotated)):
            self.mesh.setMeshData(vertexes=rotated, faces=self.faces,
                                  faceColors=self.colors, resetNormals=True)

    def update(self):
        while self.ser.in_waiting >= 17:  # header + 4 floats (1 + 16 bytes)
            header = self.ser.read(1)
            if header != b'\xAA':
                continue
            raw = self.ser.read(16)
            if len(raw) != 16:
                continue
            try:
                pitch, roll, bpitch, broll = struct.unpack('<4f', raw)
                pitch, roll = np.rad2deg(pitch), np.rad2deg(roll)
            except struct.error:
                continue

            # Update 3D plane
            self.set_orientation(pitch, roll)

    def run(self):
        QtWidgets.QApplication.instance().exec_()

# --- Main
if __name__ == "__main__":
    viz = PlaneVisualizer(plane_size=2.0)
    viz.run()
