#!/usr/bin/env python
import os
#os.system("python src/build.py")
print("UPLOADING")
os.system("arduino-cli upload -b esp8266:esp8266:d1_mini -p COM4 -i ./build/esp8266.esp8266.d1_mini/RETB_system.ino.bin")
exit