import serial
import time
import csv
import random
from datetime import datetime
import pyvisa

# --- Config ---
SCANNER_PORT = "/dev/cu.usbserial-1230"  # Serial port for scanner
SCANNER_BAUD = 115200

GPIB_GATEWAY_IP = "192.168.30.73"  # Your GPIB-to-LAN bridge IP
GPIB_ADDRESS = f"TCPIP0::{GPIB_GATEWAY_IP}::gpib0,22::INSTR"

NUM_CHANNELS = 16
DELAY = 0.1               # Seconds between switching and reading
NUM_CYCLES = 1            # Number of full scan cycles
RANDOMIZE = False
CSV_FILENAME = "temf_scan_pyvisa.csv"

# --- Initialize GPIB/34420A ---
rm = pyvisa.ResourceManager()
meter = rm.open_resource(GPIB_ADDRESS)
meter.timeout = 10000  # ms

# Confirm connection
idn = meter.query("*IDN?")
print(f" Connected to: {idn.strip()}")

# Configure both voltage channels
meter.write("*RST")
meter.write("SENS1:FUNC 'VOLT:DC'")
meter.write("SENS2:FUNC 'VOLT:DC'")
meter.write("SENS1:VOLT:RANG 0.1")
meter.write("SENS2:VOLT:RANG 0.1")
meter.write("SENS1:VOLT:NPLC 5")
meter.write("SENS2:VOLT:NPLC 5")
meter.write("TRIG:SOUR IMM")
meter.write("DISP:TEXT 'TEMF scan'")

# --- Setup scanner over serial ---
scanner = serial.Serial(SCANNER_PORT, SCANNER_BAUD, timeout=1)
time.sleep(1)

# --- Open CSV file for output ---
with open(CSV_FILENAME, mode="w", newline="") as csvfile:
    writer = csv.writer(csvfile)
    writer.writerow([
        "Timestamp", "Cycle", "Channel", "Relay A", "Relay B",
        "CH1 (V)", "CH2 (V)"
    ])

    for cycle in range(1, NUM_CYCLES + 1):
        print(f"\n=== Cycle {cycle}/{NUM_CYCLES} ===")
        channels = list(range(NUM_CHANNELS))
        if RANDOMIZE:
            random.shuffle(channels)

        # --- Relay A pass ---
        for ch in channels:
            scanner.write(f"setrelaya {ch}\n".encode())
            scanner.write(f"resetrelay {ch}\n".encode())
            time.sleep(DELAY)

            meter.write("ROUT:TERM FRONT1")  # Select CH1
            meter.write("INIT")
            meter.write("*WAI")
            #time.sleep(1)
            ch1 = float(meter.query("FETCH?"))

            meter.write("ROUT:TERM FRONT2")  # Select CH2
            meter.write("INIT")
            meter.write("*WAI")
            #time.sleep(1)

            ch2 = float(meter.query("FETCH?"))

        
            timestamp = datetime.now().isoformat(timespec='seconds')
            print(f"{timestamp} | A[{ch:02d}] → CH1 = {ch1:.6e}, CH2 = {ch2:.6e}")
            writer.writerow([
                timestamp, cycle, ch, f"setrelaya {ch}", "reset",
                f"{ch1:.6e}", f"{ch2:.6e}"
            ])

        if RANDOMIZE:
            random.shuffle(channels)

        # --- Relay B pass ---
        for ch in channels:
            scanner.write(f"setrelayb {ch}\n".encode())
            scanner.write(f"resetrelay {ch}\n".encode())
            time.sleep(DELAY)

            meter.write("ROUT:TERM FRONT1")  # CH1 again
            meter.write("INIT")
            meter.write("*WAI")
            ch1 = float(meter.query("FETCH?"))

            meter.write("ROUT:TERM FRONT2")  # CH2
            meter.write("INIT")
            meter.write("*WAI")
            ch2 = float(meter.query("FETCH?"))

            
            timestamp = datetime.now().isoformat(timespec='seconds')
            print(f"{timestamp} | B[{ch:02d}] → CH1 = {ch1:.6e}, CH2 = {ch2:.6e}")
            writer.writerow([
                timestamp, cycle, ch, "reset", f"setrelayb {ch}",
                f"{ch1:.6e}", f"{ch2:.6e}"
            ])

# --- Cleanup ---
meter.write("DISP:TEXT 'Done'")
meter.close()
scanner.close()
print(f"\n✅ Scan complete. Data saved to: {CSV_FILENAME}")
