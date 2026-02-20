import socket
import ssl
import re
import os
import sys
import matplotlib.pyplot as plt
from collections import deque
from matplotlib.animation import FuncAnimation

# ============================================================
# CONFIGURATION
# ============================================================
SERVER_IP = '10.42.0.171' # Ensure this matches your QNX Pi IP
SERVER_PORT = 8080

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CERT_FILE = os.path.join(BASE_DIR, 'certs', 'client.crt') 
KEY_FILE  = os.path.join(BASE_DIR, 'certs', 'client.key')
CA_FILE   = os.path.join(BASE_DIR, 'certs', 'ca.crt')

# ============================================================
# DATA CONTAINERS
# ============================================================
HISTORY_LEN = 60
data_streams = {
    'vib': deque([0.0]*HISTORY_LEN, maxlen=HISTORY_LEN),
    'snd': deque([0.0]*HISTORY_LEN, maxlen=HISTORY_LEN),
    'tmp': deque([0.0]*HISTORY_LEN, maxlen=HISTORY_LEN),
    'cur': deque([0.0]*HISTORY_LEN, maxlen=HISTORY_LEN)
}

# ============================================================
# VISUALIZATION SETUP
# ============================================================
fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(12, 8))
plt.subplots_adjust(hspace=0.4, wspace=0.3)
fig.canvas.manager.set_window_title('IMS Sentinel-RT: Secure Telemetry')

# Setup Lines
line_vib, = ax1.plot(data_streams['vib'], color='red', label='Vibration')
line_snd, = ax2.plot(data_streams['snd'], color='blue', label='Sound')
line_tmp, = ax3.plot(data_streams['tmp'], color='orange', label='Temp')
line_cur, = ax4.plot(data_streams['cur'], color='green', label='Current')

def apply_limits():
    """Applies hard limits and redraws threshold lines."""
    # Vibration
    ax1.set_title("Vibration (Events/s)")
    ax1.set_ylim(0, 250)
    # Sound
    ax2.set_title("Sound Level (%)")
    ax2.set_ylim(0, 100)
    # Temp
    ax3.set_title("Temperature (C)")
    ax3.set_ylim(0, 100)
    # Current
    ax4.set_title("Current (Amps)")
    ax4.set_ylim(0, 20)

    for ax, w, c in [(ax1, 100, 200), (ax2, 50, 80), (ax3, 65, 80), (ax4, 12, 15)]:
        # Clear old lines to prevent stacking
        for line in ax.get_lines()[1:]: line.remove() 
        ax.axhline(y=w, color='orange', linestyle='--', alpha=0.5)
        ax.axhline(y=c, color='red', linestyle='--', alpha=0.7)
        ax.grid(True, alpha=0.2)

apply_limits()

# Status Banner
status_text = fig.text(0.5, 0.02, "Waiting for Server...", ha='center', fontsize=12, 
                       bbox=dict(facecolor='white', alpha=0.5))

# ============================================================
# HOME BUTTON FIX (Compatible with Ubuntu/Matplotlib)
# ============================================================
def on_home(event):
    apply_limits()
    fig.canvas.draw_idle()
    print("[UI] Home View Restored")

# Bind 'h' key (Home) and 'r' key (Reset) for reliability
def on_key(event):
    if event.key in ['h', 'r', 'home']:
        on_home(None)

fig.canvas.mpl_connect('key_press_event', on_key)

# ============================================================
# NETWORK LOGIC
# ============================================================
def secure_connect():
    context = ssl.create_default_context(ssl.Purpose.SERVER_AUTH, cafile=CA_FILE)
    context.load_cert_chain(certfile=CERT_FILE, keyfile=KEY_FILE)
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE
    try:
        sock = socket.create_connection((SERVER_IP, SERVER_PORT), timeout=3)
        ssock = context.wrap_socket(sock, server_hostname=SERVER_IP)
        print(f"[SUCCESS] Connected to {SERVER_IP}")
        return ssock
    except Exception as e:
        print(f"[ERROR] Connection failed: {e}")
        print("Tip: Ensure './ims_server' is running on the QNX Pi.")
        return None

conn = secure_connect()
if conn:
    conn.read(1024) 
    conn.write(b"monitor")

def update(frame):
    if not conn: return
    try:
        conn.setblocking(False)
        raw_data = conn.recv(1024).decode('utf-8', errors='ignore')
        
        match = re.search(r"Vib:\s+(\d+)\s+\|\s+Snd:\s+(\d+)%\s+\|\s+Temp:\s+([\d\.]+)C\s+\|\s+Cur:\s+([\d\.]+)A", raw_data)
        
        if match:
            v, s, t, c = map(float, match.groups())
            data_streams['vib'].append(v); data_streams['snd'].append(s)
            data_streams['tmp'].append(t); data_streams['cur'].append(c)
            
            line_vib.set_ydata(data_streams['vib'])
            line_snd.set_ydata(data_streams['snd'])
            line_tmp.set_ydata(data_streams['tmp'])
            line_cur.set_ydata(data_streams['cur'])

        if "[CRITICAL]" in raw_data:
            status_text.set_text("STATUS: CRITICAL FAULT")
            status_text.set_backgroundcolor('#ff4d4d')
        elif "[WARNING]" in raw_data:
            status_text.set_text("STATUS: SYSTEM WARNING")
            status_text.set_backgroundcolor('#ffcc00')
        elif "[OK]" in raw_data:
            status_text.set_text("STATUS: OPERATIONAL")
            status_text.set_backgroundcolor('#66ff66')

    except (BlockingIOError, ssl.SSLWantReadError):
        pass
    except Exception as e:
        pass

if conn:
    ani = FuncAnimation(fig, update, interval=1000, cache_frame_data=False)
    plt.show()
else:
    print("[EXIT] Dashboard cannot start without server connection.")
