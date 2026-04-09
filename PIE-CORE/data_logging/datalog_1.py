import serial
import time
import os

ser = serial.Serial('COM15', 9600)

file_exists = os.path.isfile("pie_log_new1.csv")

with open("pie_log_new1.csv", "a") as f:
    if not file_exists:
        f.write("timestamp,amplitude,mean,std,z,behavior,transition,personalInsight,prediction,contextInsight,action\n")

    while True:
        line = ser.readline().decode(errors='ignore').strip()
        
        parts = line.split(",")

        if len(parts) == 10:
            try:
                amp = float(parts[0])
                mean = float(parts[1])
                std = float(parts[2])
                z = float(parts[3])
                behavior = parts[4]
                transition = parts[5]
                personalInsight = parts[6]
                prediction = parts[7]
                contextInsight = parts[8]
                action = parts[9]

                timestamp = time.time()

                f.write(f"{timestamp},{amp},{mean},{std},{z},{behavior},{transition},{personalInsight},{prediction},{contextInsight},{action}\n")
                print(timestamp, amp, mean, std, z, behavior, transition, personalInsight, prediction, contextInsight, action)

            except:
                pass
