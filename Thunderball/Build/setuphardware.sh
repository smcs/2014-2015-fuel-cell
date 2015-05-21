# overlay hardware setup
#
#dtc -@ -O dtb -o /home/ubuntu/src/Linux/Applications/MicroGrid3/overlays/BB-BONE-PRU-PINS-00A0.dtbo /home/ubuntu/src/Linux/Applications/MicroGrid3/overlays/BB-BONE-PRU-PINS-00A3.dts
#dtc -@ -O dtb -o /home/ubuntu/src/Linux/Applications/MicroGrid3/overlays/BB-BONE-PWM-PINS-00A0.dtbo /home/ubuntu/src/Linux/Applications/MicroGrid3/overlays/BB-BONE-PWM-PINS-00A0.dts
#dtc -@ -O dtb -o /home/ubuntu/src/Linux/Applications/MicroGrid3/overlays/enable-uart5-00A0.dtbo /home/ubuntu/src/Linux/Applications/MicroGrid3/overlays/enable-uart5.dts
#dtc -@ -O dtb -o /home/ubuntu/src/Linux/Applications/MicroGrid3/overlays/BB-SPI0-00A0.dtbo /home/ubuntu/src/Linux/Applications/MicroGrid3/overlays/BB-SPI0-00A0.dts
#dtc -@ -O dtb -o /home/ubuntu/src/Linux/Applications/MicroGrid3/overlays/BB-SPI1-00A0.dtbo /home/ubuntu/src/Linux/Applications/MicroGrid3/overlays/BB-SPI1-00A0.dts
#dtc -@ -O dtb -o /home/ubuntu/src/Linux/Applications/MicroGrid3/overlays/BB-GPIOMAP-00A0.dtbo /home/ubuntu/src/Linux/Applications/MicroGrid3/overlays/BB-GPIOMAP-00A0.dts

cp /opt/protonex/package/overlays/BB-BONE-PRU-PINS-00A0.dtbo /lib/firmware
cp /opt/protonex/package/overlays/BB-BONE-PWM-PINS-00A0.dtbo /lib/firmware
cp /opt/protonex/package/overlays/enable-uart5-00A0.dtbo /lib/firmware
cp /opt/protonex/package/overlays/BB-SPI0-00A0.dtbo /lib/firmware
cp /opt/protonex/package/overlays/BB-SPI1-00A0.dtbo /lib/firmware
cp /opt/protonex/package/overlays/BB-GPIOMAP-00A0.dtbo /lib/firmware

echo BB-BONE-PRU-PINS > /sys/devices/bone_capemgr.9/slots
echo BB-BONE-PWM-PINS > /sys/devices/bone_capemgr.9/slots
echo enable-uart5 > /sys/devices/bone_capemgr.9/slots
echo BB-SPI0 > /sys/devices/bone_capemgr.9/slots
echo BB-SPI1 > /sys/devices/bone_capemgr.9/slots
echo BB-GPIOMAP > /sys/devices/bone_capemgr.9/slots

# route for multicast group support
#route add -net 224.0.0.0 netmask 240.0.0.0 dev eth0

# call to extract time from real time clock on Thunderball/MicroGrid hardware
/opt/protonex/package/bin/SetSystemTime
