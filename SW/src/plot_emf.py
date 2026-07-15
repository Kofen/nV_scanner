import serial
import time
import csv
import random
from datetime import datetime
import pyvisa

# --- Config ---
SCANNER_PORT = "/dev/cu.usbserial-21230"  # Serial port for scanner
SCANNER_BAUD = 115200

GPIB_GATEWAY_IP = "192.168.30.73"          # Agilent GPIB-LAN bridge IP
GPIB_ADDRESS = f"TCPIP0::{GPIB_GATEWAY_IP}::gpib0,22::INSTR"

NUM_CHANNELS = 16
DELAY = 2.0               # Delay in seconds between switching and reading
NUM_CYCLES = 1            # Number of full scan cycles
RANDOMIZE = True          # Randomize channel order each cycle
CSV_FILENAME = "temf_scan_pyvisa.csv"

# --- Setup VISA / 34420A ---
rm = pyvisa.ResourceManager()
meter = rm.open_resource(GPIB_ADDRESS)
meter.timeout = 10000  # ms

idn = meter.query("*IDN?")
print(f"Connected to: {idn.strip()}")
# Full configuration of 34420A for front terminals + slow high-res read
meter.write("*RST")
meter.write("ROUT:TERM FRONT")           # Use front terminals
meter.write("INP:IMP:AUTO ON")           # Enable auto impedance switching
meter.write("CONF:VOLT:DC 0.1,MAX")      # 100 mV range, auto-res
meter.write("VOLT:NPLC 20")              # 20 power line cycles = slowest, most accurate
meter.write("TRIG:SOUR IMM")             # Immediate trigger
meter.write("DISP:TEXT 'TEMF scan'")     # Optional display text

# --- Setup scanner over serial ---
scanner = serial.Serial(SCANNER_PORT, SCANNER_BAUD, timeout=1)
time.sleep(1)  # Let the scanner settle

# --- Start CSV Logging ---
with open(CSV_FILENAME, mode="w", newline="") as csvfile:
    writer = csv.writer(csvfile)
    writer.writerow(["Timestamp", "Cycle", "Channel", "Relay A", "Relay B", "CH1 (V)", "CH2 (V)"])

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

            meter.write("VOLT:CHAN 1")
            ch1 = float(meter.query("MEAS:VOLT:DC? 0.1,MAX"))

            meter.write("VOLT:CHAN 2")
            ch2 = float(meter.query("MEAS:VOLT:DC? 0.1,MAX"))

            timestamp = datetime.now().isoformat(timespec='seconds')
            print(f"{timestamp} | Cycle {cycle} | A[{ch:02d}] → CH1 = {ch1:.6e}, CH2 = {ch2:.6e}")
            writer.writerow([timestamp, cycle, ch, f"setrelaya {ch}", "reset", f"{ch1:.6e}", f"{ch2:.6e}"])

        if RANDOMIZE:
            random.shuffle(channels)

        # --- Relay B pass ---
        for ch in channels:
            scanner.write(f"setrelayb {ch}\n".encode())
            scanner.write(f"resetrelay {ch}\n".encode())
            time.sleep(DELAY)

            meter.write("VOLT:CHAN 1")
            ch1 = float(meter.query("MEAS:VOLT:DC? 0.1,MAX"))

            meter.write("VOLT:CHAN 2")
            ch2 = float(meter.query("MEAS:VOLT:DC? 0.1,MAX"))

            timestamp = datetime.now().isoformat(timespec='seconds')
            print(f"{timestamp} | Cycle {cycle} | B[{ch:02d}] → CH1 = {ch1:.6e}, CH2 = {ch2:.6e}")
            writer.writerow([timestamp, cycle, ch, "reset", f"setrelayb {ch}", f"{ch1:.6e}", f"{ch2:.6e}"])

# --- Cleanup ---
meter.write("DISP:TEXT 'Done'")
meter.close()
scanner.close()
print(f"\n✅ Scan complete. Results saved to: {CSV_FILENAME}")
