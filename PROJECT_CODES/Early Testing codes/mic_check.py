import sounddevice as sd
import numpy as np

def callback(indata, frames, time, status):
    # Calculate volume (Root Mean Square)
    volume = np.linalg.norm(indata) * 10
    # Print a bar graph in the terminal
    print(f"Volume: {volume:.2f} | " + "|" * int(volume))

print("Speak into the mic! Press Ctrl+C to stop.")
with sd.InputStream(callback=callback):
    sd.sleep(100000)