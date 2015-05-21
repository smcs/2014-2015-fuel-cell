#Usage info
write_netconfig_common() {

echo "# interfaces(5) file used by ifup(8) and ifdown(8)" > $ROOTPATH/etc/network/interfaces.new
echo "" >> $ROOTPATH/etc/network/interfaces.new
echo "# loopback network interface" >> $ROOTPATH/etc/network/interfaces.new
echo "auto lo" >> $ROOTPATH/etc/network/interfaces.new
echo "iface lo inet loopback" >> $ROOTPATH/etc/network/interfaces.new
echo "" >> $ROOTPATH/etc/network/interfaces.new
echo "# primary network interface" >> $ROOTPATH/etc/network/interfaces.new

}

update_netconfig_common() {
	mv $ROOTPATH/etc/network/interfaces $ROOTPATH/etc/network/interfaces.bak
	mv $ROOTPATH/etc/network/interfaces.new $ROOTPATH/etc/network/interfaces
}

show_help() {
	echo "Usage: ${0##*/} [-h] [-s SERIALNUM] [-z HZID] [-p SCRIPTPATH] [-d DEVFILE]..."
	echo "Install run environemnt"
	echo
	echo "   -h             display this help and exit"
	echo "   -s SERIALNUM   Required SN of device"
	echo "   -z HZID        Required Hotzone Id of device"
	echo "   -p SCRIPTPATH  Path for install scripts (.)"
	echo "   -d DEVFILE     Default device file (./sampledevenv)"
	echo "   -i IMAGE       File to image to /dev/mmcblk1"
	echo "   -r             Operate on /dev/mmcblk1
	echo "   -k             Do not remove source tree
	echo "   -o ROOTPATH    Place root is mounted (/ default)"
	echo "   -t PACKAGEDIR  Path to package file"
	echo "   -x             Disable IP"
	echo "   -c             Use DHCP IP configuration"
	echo "   -f             Use fixed IP configuration"
	echo "   -a <ipaddr>    Fixed IP Address (if -f used) "
	echo "   -m <ipmask>    Fixed IP subnet mask (if -f used) "
	echo "   -g <ipgw>      Fixed default gateway (if -f used) "
	echo "   -5				P250 setup "
}

set -e

# Initialize our own variables:
SERIALNUM=""
HZID="NotSet"
HOSTTAG="P200i-SN-"
UNITID=""
SCRIPTPATH="."
DEVFILE="./sampledevenv"
IPADDR=""
IPMASK=""
IPGW=""
IPMODE="none"
MODE="iponly"
ROOTPATH=""
IMAGEFILE=""
REMOTEDEVICE=0
PACKAGEDIR=""
WAITTIME=0
REMOVESOURCE=1
TARGET_P250=0

# parse options
OPTIND=1 # Reset is necessary if getopts was used previously in the script.  It is a good idea to make this local in a function.
while getopts "h?s:p:z:d:krxacf5i:m:g:o:t:w:" opt; do
    case "$opt" in
        h)
            show_help
            exit 0
            ;;
        s)  SERIALNUM=$OPTARG
            ;;
        p)  SCRIPTPATH=$OPTARG
            ;;
        z)  HZID=$OPTARG
            ;;
        d)  DEVFILE=$OPTARG
            ;;
		x)  IPMODE="disabled"
			;;
		c)  IPMODE="dhcp"
			;;
		f)  IPMODE="fixed"
			;;
        a)  IPADDR=$OPTARG
            ;;
        m)  IPMASK=$OPTARG
            ;;
        g)  IPGW=$OPTARG
            ;;
		o)  ROOTPATH=$OPTARG
			;;
		r)  REMOTEDEVICE=1
			;;
		k)  REMOVESOURCE=0
			;;
		i)  IMAGEFILE=$OPTARG
			;;
		t)  PACKAGEDIR=$OPTARG
			;;
		w)  WAITTIME=$OPTARG
			;;
		5)  HOSTTAG="P250-SN-"
			TARGET_P250=1
			;;
        '?')
            show_help >&2
            exit 1
            ;;
    esac
done
shift "$((OPTIND-1))" # Shift off the options and optional --.

sleep $WAITTIME

if [ -n "$IMAGEFILE" ]; then
	if [ -f /dev/mmcblk1p1 ]; then
		echo "No device at /dev/mmcblk1p2."
		exit 1
	fi

	if [ -n "$ROOTPATH" ]; then
		echo "Cannot specify a ROOTDIR when imaging device."
		echo "ROOTDIR is mapped to /dev/mmcblk1p2 automatically."
		exit 1
	fi

	echo "Imaging $IMAGEFILE to /dev/mmcblk1."
	sudo xz -cd $IMAGEFILE > /dev/mmcblk1

	exit 0
	REMOTEDEVICE=1
fi

if [ $REMOTEDEVICE = 1 ]; then
	if [ -f /dev/mmcblk1p1 ]; then
		echo "No device at /dev/mmcblk1p2."
		exit 1
	fi

	sudo mkdir -p /media/BootPart || true
	sudo mount /dev/mmcblk1p1 /media/BootPart || true
	sudo sed -i '2s/.*/optargs=fixrtc\ capemgr.disable_partno=BB-BONELT-HDMI,BB-BONELT-HDMIN/' /media/BootPart/uEnv.txt

if [ $TARGET_P250 = 1 ]; then
	sudo cp /opt/protonex/builds/MLO.P250 /media/BootPart/MLO
	sudo cp /opt/protonex/builds/u-boot.img.P250 /media/BootPart/u-boot.img
else
	sudo cp /opt/protonex/builds/MLO.P200I /media/BootPart/MLO
	sudo cp /opt/protonex/builds/u-boot.img.P200I /media/BootPart/u-boot.img
fi
	sudo mkdir -p /media/OsPart || true
	sudo mount /dev/mmcblk1p2 /media/OsPart || true

	ROOTPATH=/media/OsPart
fi

# update ip config
case $IPMODE in
none)		echo "IP configuruation not updated"
			;;
dhcp)		echo "IP configuruation set to DHCP"
			write_netconfig_common;
			echo "#auto eth0 -- not started automatically" >> $ROOTPATH/etc/network/interfaces.new ;
			echo "iface eth0 inet dhcp" >> $ROOTPATH/etc/network/interfaces.new ;
			update_netconfig_common ;
			;;
fixed)		echo "IP configuruation set to static: $IPADDR"
			write_netconfig_common;
			echo "#auto eth0 -- not started automatically" >> $ROOTPATH/etc/network/interfaces.new ;
			echo "iface eth0 inet static" >> $ROOTPATH/etc/network/interfaces.new ;
			echo "    address $IPADDR" >> $ROOTPATH/etc/network/interfaces.new ;
			echo "    netmask $IPMASK" >> $ROOTPATH/etc/network/interfaces.new ;
			echo "    gateway $IPGW" >> $ROOTPATH/etc/network/interfaces.new ;
			update_netconfig_common ;
			;;
disabled)	echo "IP configuruation disabled"
			write_netconfig_common;
			echo "# Disabled" >> $ROOTPATH/etc/network/interfaces.new ;
			update_netconfig_common ;
			;;
esac

# Make the $ROOTPATH/opt/protonex format
echo "Creating $ROOTPATH/opt/protonex framework"
sudo mkdir -p $ROOTPATH/opt/protonex/Calibration
sudo mkdir -p $ROOTPATH/opt/protonex/upgrades
sudo mkdir -p $ROOTPATH/opt/protonex/log
sudo mkdir -p $ROOTPATH/opt/protonex/builds
sudo mkdir -p $ROOTPATH/opt/protonex/cores
sudo mkdir -p $ROOTPATH/opt/protonex/cores/backup
sudo chown ubuntu -hR $ROOTPATH/opt/protonex
sudo chown updater -h $ROOTPATH/opt/protonex/upgrades

# add some dev files so that system is more ready for building images
sudo mkdir -p $ROOTPATH/usr/include/pruss
sudo cp /usr/include/pruss/* $ROOTPATH/usr/include/pruss
sudo cp /usr/lib/libpruss* $ROOTPATH/usr/lib
sudo cp /usr/bin/pasm $ROOTPATH/usr/bin
sudo chmod 755 $ROOTPATH/usr/bin/pasm

sudo cp /usr/local/bin/dtc $ROOTPATH/usr/local/bin
sudo chmod 755 $ROOTPATH/usr/local/bin/dtc

if [ $REMOVESOURCE = 1 ]; then
	sudo rm -rf $ROOTPATH/home/ubuntu/src
fi

# copy up application package file

if [ -n "$PACKAGEDIR" ]; then
	echo "Copying package into install"
	cp $PACKAGEDIR/package_TB*.zip $ROOTPATH/opt/protonex/upgrades
fi

# Delete old configuration
echo "Delete old upstart config files"
rm -f $ROOTPATH/etc/init/ProtonexInit.conf
rm -f $ROOTPATH/etc/init/ProtonexMain.conf
rm -f $ROOTPATH/etc/init/ProtonexSub1.conf
rm -f $ROOTPATH/etc/init/ProtonexSub2.conf
rm -f $ROOTPATH/etc/init/ProtonexNode.conf
rm -f $ROOTPATH/etc/init/UpStartEth0Script.conf
#rm -f $ROOTPATH/etc/init/smb.conf
rm -f $ROOTPATH/etc/samba/smb.conf

# install samba config file
if [ -d $ROOTPATH/etc/samba ]; then
	echo "Copying Samba config to $ROOTPATH/etc/samba"
	tr -d '\r' < $SCRIPTPATH/smb.conf > $ROOTPATH/etc/samba/smb.conf
else
	echo "Samba not configured at $ROOTPATH/etc/samba"
fi

# Copy each process configuration file up to $ROOTPATH/etc/init
echo "Copy new upstart configuration"
# Make sure there are no Windows' style CRLF
tr -d '\r' < $SCRIPTPATH/UpStartInitScript.conf > $ROOTPATH/etc/init/ProtonexInit.conf
tr -d '\r' < $SCRIPTPATH/UpStartMainScript.conf > $ROOTPATH/etc/init/ProtonexMain.conf
tr -d '\r' < $SCRIPTPATH/UpStartEth0Script.conf > $ROOTPATH/etc/init/ProtonexEth0Up.conf
tr -d '\r' < $SCRIPTPATH/UpStartInitSubScript.conf | sed s/ProtonexSub/ProtonexSub1/g > $ROOTPATH/etc/init/ProtonexSub1.conf

# Copy the main script which checks for upgrades and starts application
# Again make sure there are no Windows' style CRLF
tr -d '\r' < $SCRIPTPATH/checkforupgrade.sh > $ROOTPATH/opt/protonex/checkforupgrade.sh
chmod 700 $ROOTPATH/opt/protonex/checkforupgrade.sh

if [ $REMOTEDEVICE = 1 ]; then
	echo "Copy PRU libraries"
	cp /usr/lib/libpruss* $ROOTPATH/usr/lib
	echo "Copy setup file"
	tr -d '\r' < /home/ubuntu/src/setupBBB.sh > $ROOTPATH/home/ubuntu/setupBBB.sh
fi

if [ -n "$SERIALNUM" ]; then
	# install device info file with info unique to unit
	echo "Creating device information file"
	tr -d '\r' < $DEVFILE | sed s/ReplaceHotZoneId/$HZID/g  | sed s/ReplaceSN/$SERIALNUM/g | sed s/ReplaceUnitId/$UNITID/g > $ROOTPATH/opt/protonex/devenv

	HOSTNAME=$HOSTTAG$SERIALNUM
	echo "Host name = $HOSTNAME"
	echo "$HOSTNAME" > $ROOTPATH/etc/hostname
else
	echo "Missing serial number [-s SERIALNUM]"
	echo "Device configuation not updated"
fi

# try to force files onto disk
sync

#(echo p; echo d; echo 2; echo p; echo n; echo p; echo 2; echo ; echo ; echo p; echo w) | fdisk /dev/mmcblk1
#partprobe /dev/mmcblk1
#resize2fs /dev/mmcblk1p2
# prussdrv.h
# pruss_intc_mapping.h
# pasm
# dtc in /usr/local/bin