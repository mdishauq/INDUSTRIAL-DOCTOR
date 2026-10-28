import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import sounddevice as sd

# --- CONFIGURATION ---
SAMPLE_RATE = 44100
BLOCK_SIZE = 4096
FREQ_LIMIT = 10000 

# --- AI ZONES (Tweaked) ---
ZONES = [
    # Started at 60Hz to ignore electrical hum/room noise
    (60, 300, "LOW FREQ: UNBALANCE / LOOSENESS"),  
    (300, 2000, "MID FREQ: GEAR MESH"),
    (2000, 10000, "HIGH FREQ: BEARING FAULT")
]

# --- SETUP PLOT ---
fig, ax = plt.subplots(figsize=(12, 7))

x_axis = np.linspace(0, SAMPLE_RATE / 2, (BLOCK_SIZE // 2) + 1)
line, = ax.plot([], [], color='#00ff00', linewidth=1.5)

ax.set_facecolor('black')
fig.patch.set_facecolor('#1c1c1c')
ax.set_ylim(0, 80)
ax.set_xlim(0, FREQ_LIMIT)
ax.set_title("AI MECHANIC: TUNED SENSITIVITY", color='white', fontsize=16, fontweight='bold')
ax.set_xlabel("Frequency (Hz)", color='white')
ax.set_ylabel("Amplitude (dB)", color='white')
ax.grid(True, color='gray', linestyle='--', alpha=0.3)
ax.tick_params(colors='white')

# Zone Highlights
ax.axvspan(60, 300, color='red', alpha=0.1)     
ax.axvspan(300, 2000, color='yellow', alpha=0.1) 
ax.axvspan(2000, 10000, color='blue', alpha=0.1) 

# Text Elements
diagnosis_text = ax.text(0.5, 0.5, 'READY', transform=ax.transAxes, 
                         color='white', fontsize=20, fontweight='bold', 
                         ha='center', va='center', bbox=dict(facecolor='black', alpha=0.8))

# --- PROCESSING ---
def analyze_audio(frame):
    try:
        data, _ = stream.read(BLOCK_SIZE)
        
        # FFT Math
        windowed_data = data[:, 0] * np.hanning(len(data))
        fft_data = np.abs(np.fft.rfft(windowed_data)) / BLOCK_SIZE
        magnitude_db = 20 * np.log10(fft_data + 1e-10) + 80
        magnitude_db[magnitude_db < 0] = 0

        # *** CRITICAL FIX: Kill the first 50Hz (Index 0 to 10) ***
        # This removes the "false positive" looseness from room noise
        magnitude_db[:10] = 0 
        
        # Update Graph
        line.set_data(x_axis, magnitude_db)
        
        # Find Peak
        peak_index = np.argmax(magnitude_db)
        peak_freq = x_axis[peak_index]
        peak_amp = magnitude_db[peak_index]

        # Decision Logic
        diagnosis = "SYSTEM HEALTHY"
        color_alert = "#00ff00" # Green

        # Threshold is 30dB. If peak is quieter, ignore it.
        if peak_amp > 30:
            for (low, high, label) in ZONES:
                if low <= peak_freq <= high:
                    diagnosis = f"⚠ {label}"
                    if low == 60: color_alert = "red"
                    if low == 300: color_alert = "yellow"
                    if low == 2000: color_alert = "cyan"
        else:
             diagnosis = "LISTENING..."
             color_alert = "gray"

        diagnosis_text.set_text(diagnosis)
        diagnosis_text.set_color(color_alert)
        diagnosis_text.set_bbox(dict(facecolor='black', alpha=0.8, edgecolor=color_alert))

        return line, diagnosis_text
    
    except Exception as e:
        print(f"Error: {e}")
        return line, diagnosis_text

# --- START ---
stream = sd.InputStream(channels=1, samplerate=SAMPLE_RATE, blocksize=BLOCK_SIZE)
stream.start()
ani = FuncAnimation(fig, analyze_audio, blit=False, interval=50, cache_frame_data=False)
plt.show()
stream.stop()
stream.close()