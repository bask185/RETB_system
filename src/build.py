#!/usr/bin/env python
import os
#print('ASSEMBLING IO FILES')
#os.system("updateIO.py")
print('ADDING TIME STAMP')
os.system("addDate.py")
print('BUILDING PROJECT')
os.system('arduino-cli compile -b esp8266:esp8266:d1_mini -e')
exit