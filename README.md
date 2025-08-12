# ESP8266
- Arduino IDE 1.8.19
- ESP8266 LittleFS 2.6.0
- TFT_eSPI > User_Setup.h mod

# PC (Python)
- UV
```
uv run pyinstaller --onefile --windowed --icon=bongo_middle.ico --add-data "bongo_middle.png;." .\bongo_serial.py

uv run pyinstaller --onefile --windowed --add-data "bongo_middle.png;." .\bongo_serial.py
```