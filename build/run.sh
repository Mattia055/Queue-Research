#!/bin/bash

# Run "padding" executable and record its output to padding_output.txt
nohup ./padding > ../padding_output.txt 2>&1 &

# Wait for "padding" to finish before running "no_padding"
wait $!

# Run "no_padding" executable and record its output to no_padding_output.txt
nohup ./no_padding > ../no_padding_output.txt 2>&1 &

# Optional: Print a message indicating that the programs are running
echo "Both programs are running sequentially in the background. You can safely disconnect."
