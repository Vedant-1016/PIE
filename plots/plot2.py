import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("pie_log_new.csv")

plt.figure(figsize=(12,5))
plt.xlabel("Time")
plt.ylabel("Amplitude")
plt.plot(df['amplitude'], label="Amplitude")
plt.plot(df['mean'], label="Mean")
plt.plot(df['mean'] + df['std'], '--', label="+1σ")
plt.plot(df['mean'] + 2*df['std'], '--', label="+2σ")

plt.legend()
plt.title("PIE Adaptive Thresholds")
plt.show()


plt.figure(figsize=(12,4))
plt.plot(df['z'])
plt.axhline(2, linestyle='--')
plt.axhline(-2, linestyle='--')
plt.title("Z-score")
plt.show()