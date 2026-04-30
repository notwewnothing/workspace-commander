import serial
import serial.tools.list_ports
import time
import subprocess
import threading
from datetime import datetime

BAUD_RATE = 115200

def run_cmd(cmd):
    """Executes a shell command asynchronously."""
    subprocess.Popen(cmd, shell=True)

def find_esp_port():
    """Auto-detects the ESP32 serial port."""
    ports = serial.tools.list_ports.comports()
    for port in ports:
        # Common identifiers for ESP32/CP2102/CH340/CH9102
        if "CP210" in port.description or "CH340" in port.description or "USB" in port.device:
            return port.device
    
    # Fallback default
    return "/dev/ttyUSB0"

def send_time(ser):
    """Sends the current system time to the ESP32 periodically."""
    while True:
        now = datetime.now()
        # Format HHMM (e.g., 2302)
        time_str = now.strftime("%H%M")
        try:
            ser.write(f"TIME:{time_str}\n".encode('utf-8'))
        except Exception:
            break # Exit thread on write failure so it restarts upon reconnect
        
        # Send time every 10 seconds to keep the 7-segment perfectly synced
        time.sleep(10)

def main():
    while True:
        port = find_esp_port()
        try:
            print(f"Connecting to ESP32 on {port}...")
            ser = serial.Serial(port, BAUD_RATE, timeout=1)
            print("Connected successfully!")
            
            # Start background thread to send the time continuously
            t = threading.Thread(target=send_time, args=(ser,), daemon=True)
            t.start()

            while True:
                if ser.in_waiting > 0:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if not line: 
                        continue
                        
                    print(f"Received Payload: {line}")
                    
                    # --- ACTION MAPPINGS ---
                    
                    if line == "MEDIA_SPOTIFY":
                        # Your keybinds show Super+S opens the 'special' workspace (scratchpad)
                        # which is standard for Spotify in advanced Hyprland configs!
                        run_cmd("hyprctl dispatch togglespecialworkspace")
                        
                    elif line == "MEDIA_PLAY_PAUSE":
                        run_cmd("playerctl play-pause")
                        
                    elif line == "MEDIA_NEXT":
                        run_cmd("playerctl next")
                        
                    elif line == "MEDIA_PREVIOUS":
                        run_cmd("playerctl previous")
                        
                    elif line == "KEY_RIGHT":
                        # Simulates Right Arrow for skipping forward in Movies
                        run_cmd("wtype -k Right")
                        
                    elif line == "KEY_LEFT":
                        # Simulates Left Arrow for skipping backward in Movies
                        run_cmd("wtype -k Left")
                        
                    elif line == "KEY_DEBUGGER":
                        # Standard F5 mapping for Debugging (VS Code / IDEs)
                        run_cmd("wtype -k F5")
                        
                    elif line == "MODE_STUDY":
                        # On Entry: Open Browser (Tab 3), Open Spotify (Scratchpad)
                        run_cmd("hyprctl dispatch togglespecialworkspace")
                        time.sleep(0.5)
                        # Hyprland directly launches your defined $browser variable
                        run_cmd("hyprctl dispatch exec $browser")
                        time.sleep(1)
                        # Switch to Tab 3 (Ctrl + 3)
                        run_cmd("wtype -M ctrl -k 3 -m ctrl")
                        
                    elif line == "MODE_CODING":
                        # Future-proofing: Insert any environment setup here (like launching your IDE)
                        pass
                        
                    elif line == "MODE_MOVIES":
                        # Future-proofing: Insert fullscreen toggles or media player launches here
                        pass
                        
                    elif line == "NOTIFY":
                        # The ESP32 triggers this when the Pomodoro timer hits zero
                        run_cmd("notify-send 'Macro Pad' 'Pomodoro phase complete!' -u critical -i timer")

        except serial.SerialException:
            # If device gets disconnected, wait and retry silently
            time.sleep(3)
        except Exception as e:
            print(f"Unexpected error: {e}")
            time.sleep(3)

if __name__ == "__main__":
    main()
