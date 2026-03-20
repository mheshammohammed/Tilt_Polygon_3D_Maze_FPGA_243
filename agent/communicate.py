import serial
import serial.tools.list_ports
import struct
import time

# find the JTAG UART port automatically
def find_jtag_port():
    ports = serial.tools.list_ports.comports()
    for p in ports:
        # Intel/Altera USB Blaster shows up with these keywords
        if any(k in p.description for k in ['USB', 'JTAG', 'Altera', 'Intel', 'DE-SoC']):
            print(f"Found port: {p.device} — {p.description}")
            return p.device
    # if not found automatically just return None
    return None

def open_connection(port=None, baud=115200):
    if port is None:
        port = find_jtag_port()
    if port is None:
        print("Could not find JTAG port automatically.")
        print("Available ports:")
        for p in serial.tools.list_ports.comports():
            print(f"  {p.device} — {p.description}")
        port = input("Enter port manually (e.g. COM3 or /dev/ttyUSB0): ")
    conn = serial.Serial(port, baud, timeout=0.1)
    print(f"Connected to {port}")
    return conn

# read one full game state packet from Nios V
# packet format: 0xFF marker, then 5 ints (4 bytes each) = 21 bytes total
def read_game_state(conn):
    # sync to start marker
    while True:
        b = conn.read(1)
        if len(b) == 0:
            return None  # timeout, nothing received
        if b[0] == 0xFF:
            break

    # read 5 ints = 20 bytes
    data = conn.read(20)
    if len(data) < 20:
        return None  # incomplete packet

    px, py, tcol, trow, cm = struct.unpack('>iiiii', data)
    return {
        'px':   px,    # player x
        'py':   py,    # player y
        'tcol': tcol,  # target col
        'trow': trow,  # target row
        'map':  cm     # current map index
    }

# send agent move to Nios V
# 0=up 1=down 2=left 3=right 4=none
def send_move(conn, move):
    conn.write(bytes([move & 0xFF]))