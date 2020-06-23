# vx8-nmea

vx8nmea sketch - translates GPS output to GPS input for Yaesu VX-8 radio.  Save about $400.

This sketch is designed to run on a Trinket M0.  The GPS TX output is connected to the Trinket's RX port 
and the Trinket's TX port is connected to the radio.  We ignore the radio's TX.

GPS Module is: MTK3339 G.top

Trinket M0: https://www.adafruit.com/product/3500
MTK3339:  https://www.adafruit.com/product/790

I didn't use 330 Ohm resistors between the GPS and the Trinket as the datasheet advises.  
This seems fine in testing but you may want to add.

You might want to pick up this cable to interface this to your VX-8:
https://www.amazon.com/gp/product/B00AIT49ZK

![](https://twitter.com/crazypill/status/1275241680447262720/photo/1)

