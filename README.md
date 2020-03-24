# TinySolder - T12 Soldering Station based on ATtiny13a

Simple T12 Quick Heating Soldering Station featuring (in 700 bytes of code):
- Temperature measurement of the tip
- Direct control of the heater
- Temperature control via potentiometer
- Handle movement detection (by checking ball switch)
- Time driven sleep/power off mode if iron is unused (movement detection)

Indicator LEDs:
- steady blue          - station is powered
- steady red           - tip temperature has not reached setpoint yet
- steady green         - tip temperature is at setpoint - iron is worky
- blinking red/green   - iron is in sleep mode - move handle to wake up
- steady red and green - iron is in off mode - move handle to start again

Power supply should be in the range of 16V/2A to 24V/3A and well
stabilized.

For calibration you need a soldering iron tips thermometer. For best results
wait at least three minutes after switching on the soldering station before 
you start the calibration. Calibrate at your prefered temperature using the
trimpot.

Project at EasyEDA: https://easyeda.com/wagiminator/y-attiny13-soldering-station-smd


![IMG_20200323_171422_x.jpg](https://image.easyeda.com/pullimage/6zlz1T0bS0xivDuH5bGyqxX1nQG80YPWzZsWQyaF.jpeg)
