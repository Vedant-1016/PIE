import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv('pie_log.csv')

plt.figure()
plt.plot(df['timestamp'], df['amplitude'])
plt.xlabel('Timestamp')
plt.ylabel('Amplitude')
plt.title('PIE Log Data')
plt.show()
