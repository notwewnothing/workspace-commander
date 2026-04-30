# Workspace Commander 🚀

A custom-built ESP32-based macro pad designed to seamlessly control PC workflows (Coding, Study, Movies) on Linux (Wayland/Hyprland). 

*This project was built for [Hack Club](https://hackclub.com/).*

## 🌟 Features

- **Tri-Mode Workflows:** Instantly switch between **Coding**, **Study**, and **Movies** modes using advanced chorded touch inputs.
- **Hardware UI Feedback:** A 0.96" OLED display provides instant pop-up feedback with procedural geometric icons for every action you take.
- **Pomodoro Built-In:** A fully hardware-backed Pomodoro timer (25m work / 5m break) outputs directly to a 7-segment display.
- **Wayland Native:** The companion Python daemon bridges the ESP32 serial output natively into `hyprctl`, `wtype`, and `playerctl`.
- **Auto-Syncing Clock:** The daemon automatically keeps the ESP32's 7-segment display synchronized with the host PC's system time.

## ⚙️ Hardware Stack
- **Microcontroller:** ESP32
- **Displays:** 0.96" OLED Display (I2C) & 4-Digit 7-Segment Display
- **Inputs:** 3 Capacitive Touch Buttons (Using ESP32 internal touch pins)

---

## 📖 The Development Journey (Devlogs)

This project evolved significantly from its initial concept. Here is a look back at the technical development process:

### Phase 1: Pin Constraints & Multiplexing
The project began with the goal of driving a 4-digit 7-segment display. However, it quickly became apparent that standard multiplexing was far too pin-intensive for the ESP32, leaving almost no GPIO pins for the actual touch buttons. After attempting to strategically share pins via complex HIGH/LOW state optimizations, the setup proved too crammed and unstable.

### Phase 2: The OLED Discovery
While searching for resistors to fix the multiplexing setup, an unused 0.96" OLED display was discovered. This was a massive hardware breakthrough: it offered exponentially higher resolution while only requiring 2 I2C pins. The hardware was completely rewired across two breadboards to cleanly mount both the 7-segment display (for the clock/timer) and the new OLED (for UI feedback), alongside the touch sensors. 

### Phase 3: Software Architecture Rework
With the hardware locked in, the software needed a concrete vision. Instead of haphazardly adding features, the logic was strictly separated into a state machine containing three distinct workflows:
1. **Coding:** Quick access to Spotify, Play/Pause, and Debugging (F5).
2. **Study:** Local Pomodoro timer control and quick web searches.
3. **Movies:** Media seeking (Skip Forward/Backward) and playback controls.

### Phase 4: Chorded Inputs & Python Daemon
The initial concept of "Long Hold" actions felt sluggish, so it was completely replaced by **Chorded Inputs** (Hold Button 1 + Press Button 2/3), mimicking snappy keyboard modifiers like `Shift` or `Ctrl`. 

Additionally, to prevent having to constantly reflash the ESP32 C++ firmware whenever a PC shortcut needed to be tweaked, the architecture was split. The ESP32 was programmed to simply act as a dumb terminal, sending semantic payloads (e.g., `MEDIA_SPOTIFY`, `KEY_DEBUGGER`, `MODE_STUDY`) over Serial. A lightweight Python daemon was then written for the Linux host to listen to these payloads and translate them into native Wayland actions!
