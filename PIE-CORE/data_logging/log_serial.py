import serial
import time
import os

ser = serial.Serial('COM15', 9600)

file_exists = os.path.isfile("pie_log.csv")

with open("pie_log.csv", "a") as f:
    if not file_exists:
        f.write("timestamp,amplitude\n")

    while True:
        line = ser.readline().decode().strip()
        
        if line.isdigit():
            value = int(line)
            timestamp = time.time()
            
            f.write(f"{timestamp},{value}\n")
            print(timestamp, value)
