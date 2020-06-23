# vx8-nmea

vx8nmea sketch - translates GPS output to GPS input for Yaesu VX-8 radio.  Save about $400.

This sketch is designed to run on a Trinket M0.  The GPS TX output is connected to the Trinket's RX port 
and the Trinket's TX port is connected to the radio.  We ignore the radio's TX.

GPS Module is: MTK3339 G.top

Trinket M0: https://www.adafruit.com/product/3500

MTK3339:  https://www.adafruit.com/product/790

I used this coin cell holder for CR2032: 
https://www.digikey.com/product-detail/en/mpd-memory-protection-devices/BA2032/BA2032-ND/257744

I didn't use 330 Ohm resistors between the GPS and the Trinket as the datasheet advises.  
This seems fine in testing but you may want to add.

You might want to pick up this cable to interface this to your VX-8:
https://www.amazon.com/gp/product/B00AIT49ZK

The entire circuit consists of tying the ground from the trinket and radio and GPS together (I just wrapped GPS and radio's red wire together and shoved it inside the Trinket's GND pad).  
Same thing for power: take the radio's 3.3v (grey wire) and attach to Trinket and GPS (wrap the radio's grey and GPS together and insert in Trinket's 3v OUTPUT (to bypass the voltage regulator)).  
Attach the blue wire from the radio to the trinket TX port (pin4). GPS TX goes to Trinket RX (pin3).

![](https://raw.githubusercontent.com/crazypill/vx8-nmea/master/GPS-VX8.jpeg)


