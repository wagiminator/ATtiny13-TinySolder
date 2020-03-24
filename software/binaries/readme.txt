avrdude -c usbtiny -p t13 -U flash:w:SolderingStation_Tiny13_v1.0.hex
avrdude -c usbtiny -p t13 -U lfuse:w:0x7a:m -U hfuse:w:0xff:m
