import math

def calculate_stddev(numbers):
    # Calculate the mean
    mean = sum(numbers) / (len(numbers)-1)
    print(mean)
    
    # Calculate the squared differences from the mean
    squared_diffs = [(x - mean) ** 2 for x in numbers]
    
    # Calculate the variance (mean of squared differences)
    variance = sum(squared_diffs) / (len(numbers) - 1)  # sample standard deviation uses N-1
    
    # Return the square root of the variance (standard deviation)
    return math.sqrt(variance)

# Example usage
numbers = [1056,1696,1696,1696,1696,1696,1696,1696,1696,1696]
stddev = calculate_stddev(numbers)
print("Standard Deviation:", stddev)