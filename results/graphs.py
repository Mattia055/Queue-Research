import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter

XTICKS_POSITION = [1,2,4,8,16,32,64,128,256,512,1024]

# Data Preparation
def MetricsGraph(filename: str, output_file: str, title, y_format, xticks: list = [1, 2, 4, 8, 16, 32, 64, 128], show: bool = False):
    # Load data from CSV file
    csv_file_path = filename
    df = pd.read_csv(csv_file_path)

    # Clean the "queueType" column
    df["Param: queueType"] = df["Param: queueType"].astype(str).str.replace("/padded", "")

    # Debugging prints
   

    # Plotting the data
    plt.figure(figsize=(10, 6))

    # Set the x-ticks and log scale
    plt.xscale('log')
    plt.xticks(xticks)
    print("xticks:", xticks)

    # Set x-axis limits
    plt.xlim(min(xticks)-(1/3)*min(xticks), max(xticks)+(1/3)*max(xticks))
    # Set a custom formatter for x-ticks to avoid scientific notation
    plt.gca().get_xaxis().set_major_formatter(FuncFormatter(lambda x, _: f'{int(x)}'))

    # Loop to plot each queueType with error bars
    for queue in df["Param: queueType"].unique():
        queue_data = df[df["Param: queueType"] == queue]
        plt.errorbar(queue_data["Threads"], queue_data["Score"],
                     yerr=queue_data["Score Error"],
                     label=queue, capsize=3, fmt='-o', markeredgewidth=2, linewidth=2)

    # Labels and Title
    plt.xlabel("Threads", fontsize=12)
    plt.ylabel(y_format, fontsize=12)
    plt.title(title, fontsize=14)
    plt.legend(title="Queue Type", bbox_to_anchor=(1.05, 1), loc='upper left', fontsize=10)
    plt.grid(True)

    # Save the plot to file
    plt.tight_layout()
    plt.savefig(output_file)
    if show:
        plt.show()


# Calling the function
# MetricsGraph('EnqDeq.csv', 'EnqDeqGraph.png', "Enqueue/Dequeue", "Throughput (ops/s)", show=False)
# MetricsGraph('EnqDeqAdditional.csv', 'EnqDeqAdditionGraph.png', "Enqueue/Dequeue (Additional Work)", "Throughput (ops/s)", show=False)
MetricsGraph('ProdCons[1:1].csv', 'ProdCons[1:1]Graph.png', "Producer-Consumer (1:1)","Throughput (transf/s)", xticks=[2, 4, 8, 16, 32, 64, 128], show=False)
MetricsGraph('ProdCons[1:1]Additional.csv', 'ProdCons[1:1]AdditionalGraph.png', "Producer-Consumer (1:1) Additional","Throughput (transf/s)", xticks=[2, 4, 8, 16, 32, 64, 128], show=False)
MetricsGraph('ProdCons[1:2].csv', 'ProdCons[1:2]Graph.png', "Producer-Consumer (1:2)","Throughput (transf/s)", xticks=[2, 4, 8, 16, 32, 64, 128], show=False)
MetricsGraph('ProdCons[1:2]Additional.csv', 'ProdCons[1:2]AdditionalGraph.png', "Producer-Consumer (1:2) Additional","Throughput (transf/s)", xticks=[2, 4, 8, 16, 32, 64, 128], show=False)
MetricsGraph('ProdCons[2:1].csv', 'ProdCons[2:1]Graph.png', "Producer-Consumer (2:1)","Throughput (transf/s)", xticks=[2, 4, 8, 16, 32, 64, 128], show=False)
MetricsGraph('ProdCons[2:1]Additional.csv', 'ProdCons[2:1]AdditionalGraph.png', "Producer-Consumer (2:1) Additional","Throughput (transf/s)", xticks=[2, 4, 8, 16, 32, 64, 128], show=False)
