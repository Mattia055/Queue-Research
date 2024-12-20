import pandas as pd
import matplotlib.pyplot as plt

# Load the CSV file
data = pd.read_csv('MemoryBenchmark.csv')

# Extract the columns (time, rssSize, vmSize)
time = data['time']
rssSize = data['rssSize']
vmSize = data['vmSize']

# Plotting the data
plt.figure(figsize=(10, 6))
plt.plot(time, rssSize, label='RSS Size', color='blue', marker='o')
plt.plot(time, vmSize, label='VM Size', color='red', marker='x')

# Adding labels and title
plt.xlabel('Time (s)')
plt.ylabel('Size (KB)')

#list = [10000,20000] #fino a 360000
list = [i*20000 for i in range(0,18)]
#xticks labels
labels = [str(int(i/1000)) for i in list]

plt.xticks(list,labels)
plt.title('LPRQ [1:1] 10 Threads - 10 sec intervals')

# Adding a legend
plt.legend()

# Display the plot
plt.grid(True)
# Save the plot to file
plt.tight_layout()
plt.savefig("LPRQProdCons[1:1].png")
plt.show()
