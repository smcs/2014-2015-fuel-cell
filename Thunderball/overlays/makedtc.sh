dtc -@ -O dtb -o /home/ubuntu/src/Linux/Applications/Thunderball/overlays/BB-BONE-PRU-PINS-00A0.dtbo /home/ubuntu/src/Linux/Applications/Thunderball/overlays/BB-BONE-PRU-PINS-00A3.dts
dtc -@ -O dtb -o /home/ubuntu/src/Linux/Applications/Thunderball/overlays/BB-BONE-PWM-PINS-00A0.dtbo /home/ubuntu/src/Linux/Applications/Thunderball/overlays/BB-BONE-PWM-PINS-00A0.dts
dtc -@ -O dtb -o /home/ubuntu/src/Linux/Applications/Thunderball/overlays/enable-uart5-00A0.dtbo /home/ubuntu/src/Linux/Applications/Thunderball/overlays/enable-uart5.dts
dtc -@ -O dtb -o /home/ubuntu/src/Linux/Applications/Thunderball/overlays/BB-SPI0-00A0.dtbo /home/ubuntu/src/Linux/Applications/Thunderball/overlays/BB-SPI0-00A0.dts
dtc -@ -O dtb -o /home/ubuntu/src/Linux/Applications/Thunderball/overlays/BB-SPI1-00A0.dtbo /home/ubuntu/src/Linux/Applications/Thunderball/overlays/BB-SPI1-00A0.dts
dtc -@ -O dtb -o /home/ubuntu/src/Linux/Applications/Thunderball/overlays/BB-GPIOMAP-00A0.dtbo /home/ubuntu/src/Linux/Applications/Thunderball/overlays/BB-GPIOMAP-00A0.dts

echo "Attempting overlay install..."
if [ $# = 1 ]; then
if [ $1 = "install" ]; then
	echo "Installing device overlays"
	cp /home/ubuntu/src/Linux/Applications/Thunderball/overlays/BB-BONE-PRU-PINS-00A0.dtbo /lib/firmware
	cp /home/ubuntu/src/Linux/Applications/Thunderball/overlays/BB-BONE-PWM-PINS-00A0.dtbo /lib/firmware
	cp /home/ubuntu/src/Linux/Applications/Thunderball/overlays/enable-uart5-00A0.dtbo /lib/firmware
	cp /home/ubuntu/src/Linux/Applications/Thunderball/overlays/BB-SPI0-00A0.dtbo /lib/firmware
	cp /home/ubuntu/src/Linux/Applications/Thunderball/overlays/BB-SPI1-00A0.dtbo /lib/firmware
	cp /home/ubuntu/src/Linux/Applications/Thunderball/overlays/BB-GPIOMAP-00A0.dtbo /lib/firmware

	echo BB-BONE-PRU-PINS > /sys/devices/bone_capemgr.9/slots
	echo BB-BONE-PWM-PINS > /sys/devices/bone_capemgr.9/slots
	echo enable-uart5 > /sys/devices/bone_capemgr.9/slots
	echo BB-SPI0 > /sys/devices/bone_capemgr.9/slots
	echo BB-SPI1 > /sys/devices/bone_capemgr.9/slots
	echo BB-GPIOMAP > /sys/devices/bone_capemgr.9/slots

	route add -net 224.0.0.0 netmask 240.0.0.0 dev eth0
else
    echo "Install not performed, check arguments"
fi
fi
