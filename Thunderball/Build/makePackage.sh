#Usage info
show_help() {
	echo "Usage: ${0##*/} [-h] [-r] [-v REVISION] [-n APPNAME] [-c PATH]..."
	echo "Make an install package for thunberball"
	echo
	echo "   -h          display this help and exit"
	echo "   -v REVISION SW Revision of application"
	echo "   -c PATH     Path to copy zip file up to, deletes"
	echo "   -r          Remove all package zip files in PATH"
	echo "   -n          Do not add timerstamp to package tag"
	echo "               local copy"
	echo "   -p <path>   home path"
	echo "   -a <name>   application name"
}

##############################################3
# This is the major and minor version numbers
# They get changed in this file when needed
VERSION="1.0."
#

set -e

# Initialize our own variables:
APPNAME=""
REVISION="LOCAL"
copy=1
copy_path="."
rmpackage=0
deletezip=0
TIME=$(date +"%m-%d-%Y_%H%M%S")
usedate=1
HOMEDIR=/home/ubuntu

OPTIND=1 # Reset is necessary if getopts was used previously in the script.  It is a good idea to make this local in a function.
while getopts "h?rnc:v:p:a:" opt; do
    case "$opt" in
        h)
            show_help
            exit 0
            ;;
        v)  REVISION=$OPTARG
            ;;
        c)  copy_path=$OPTARG
		    copy=1
            ;;
		r)  rmpackage=1
		    ;;
		n)  usedate=0
			;;
		p)  HOMEDIR=$OPTARG
			;;
        a)  APPNAME=$OPTARG
            ;;
        '?')
            show_help >&2
            exit 1
            ;;
    esac
done
shift "$((OPTIND-1))" # Shift off the options and optional --.

if [ "$copy_path" = "." ]; then
	copy_path=$('pwd')
fi

APPVERSION=$VERSION$REVISION
TAG="TB_"
BREAK="_"

if [ $usedate = 1 ]; then
	NOW=$(echo "$TAG$APPVERSION$BREAK$TIME" | sed "s/ /_/g")
else
	NOW=$(echo "$TAG$APPVERSION" | sed "s/ /_/g")
fi

DIRNAME="package_$NOW"
export ZIPPATH=`mktemp -d /tmp/tbpackage.XXXXXX`
#ZIPPATH="$HOMEDIR/src/Linux/InstallUtilities"
DIRPATH="$ZIPPATH/package_$NOW"
OPTDIRPATH="/opt/protonex/package_$NOW"
ZIPNAME="/opt/protonex/upgrades/package_$NOW.zip"
FILENAME="package_$NOW.zip"
INSTALLFILENAME="install_$NOW.zip"

mkdir -p $DIRPATH
mkdir $DIRPATH/bin
mkdir $DIRPATH/overlays
mkdir $DIRPATH/Calibration

# Common (to MG and TB) application files
cp $HOMEDIR/src/Linux/Applications/SetSystemTime/BBB/Debug/SetSystemTime $DIRPATH/bin 
cp $HOMEDIR/src/Linux/PRUComps/SpiPruMain.bin $DIRPATH/bin

# Unique application files
cp $HOMEDIR/src/Linux/Applications/Thunderball/BBB/Debug/Thunderball $DIRPATH/bin 
cp $HOMEDIR/src/Linux/Applications/ThunderballUI/BBB/Debug/ThunderballUI $DIRPATH/bin 
cp $HOMEDIR/src/Linux/Applications/MicroThunder/BBB/Debug/MicroThunder $DIRPATH/bin 
cp $HOMEDIR/src/Linux/Applications/PtxConsole/BBB/Debug/PtxConsole $DIRPATH/bin 
cp $HOMEDIR/src/Linux/PRUComps/PwmPruMain.bin $DIRPATH/bin

# default calibration files
cp $HOMEDIR/src/Linux/Applications/Thunderball/Calibration/* $DIRPATH/Calibration 

# script files
#cp $HOMEDIR/src/Linux/InstallUtilities/startpackage.sh $DIRPATH/bin 
#tr -d '\r' < $HOMEDIR/src/Linux/InstallUtilities/startpackage.sh > $DIRPATH/bin/startpackage.sh
#chmod 700 $DIRPATH/bin/startpackage.sh
#cp $HOMEDIR/src/Linux/Applications/Thunderball/ProtonexMain.sh $DIRPATH/bin 
tr -d '\r' < $HOMEDIR/src/Linux/Applications/Thunderball/ProtonexMain.sh > $DIRPATH/bin/ProtonexMain.sh
chmod 700 $DIRPATH/bin/ProtonexMain.sh
#cp $HOMEDIR/src/Linux/Applications/Thunderball/ProtonexSub1.sh $DIRPATH/bin 
tr -d '\r' < $HOMEDIR/src/Linux/Applications/Thunderball/ProtonexSub1.sh > $DIRPATH/bin/ProtonexSub1.sh
chmod 700 $DIRPATH/bin/ProtonexSub1.sh
#cp $HOMEDIR/src/Linux/Applications/Thunderball/Build/setuphardware.sh $DIRPATH/bin 
tr -d '\r' < $HOMEDIR/src/Linux/Applications/Thunderball/Build/setuphardware.sh > $DIRPATH/bin/setuphardware.sh
chmod 700 $DIRPATH/bin/setuphardware.sh
#cp $HOMEDIR/src/Linux/Applications/Thunderball/testit.sh $DIRPATH/bin 
tr -d '\r' < $HOMEDIR/src/Linux/Applications/Thunderball/testit.sh > $DIRPATH/bin/testit.sh
chmod 700 $DIRPATH/bin/testit.sh

# overlay files
# Put in only binaries
dtc -@ -O dtb -o $DIRPATH/overlays/BB-BONE-PRU-PINS-00A0.dtbo $HOMEDIR/src/Linux/Applications/Thunderball/overlays/BB-BONE-PRU-PINS-00A3.dts
dtc -@ -O dtb -o $DIRPATH/overlays/BB-BONE-PWM-PINS-00A0.dtbo $HOMEDIR/src/Linux/Applications/Thunderball/overlays/BB-BONE-PWM-PINS-00A0.dts
dtc -@ -O dtb -o $DIRPATH/overlays/enable-uart5-00A0.dtbo $HOMEDIR/src/Linux/Applications/Thunderball/overlays/enable-uart5.dts
dtc -@ -O dtb -o $DIRPATH/overlays/BB-SPI0-00A0.dtbo $HOMEDIR/src/Linux/Applications/Thunderball/overlays/BB-SPI0-00A0.dts
dtc -@ -O dtb -o $DIRPATH/overlays/BB-SPI1-00A0.dtbo $HOMEDIR/src/Linux/Applications/Thunderball/overlays/BB-SPI1-00A0.dts
dtc -@ -O dtb -o $DIRPATH/overlays/BB-GPIOMAP-00A0.dtbo $HOMEDIR/src/Linux/Applications/Thunderball/overlays/BB-GPIOMAP-00A0.dts

# must run from here to make zip file easy to unzip
cd $ZIPPATH

echo
echo "Adding Enviroment settings"
echo

# make the appenv file for use in install
echo "PTX_VERSION=$OPTDIRPATH"
echo "PTX_VERSION=$OPTDIRPATH" > appenv 
echo "export PTX_VERSION" >> appenv 

echo "PTX_ZIPFILE=$ZIPNAME"
echo "PTX_ZIPFILE=$ZIPNAME" >> appenv 
echo "export PTX_ZIPFILE" >> appenv 

echo "PTX_TARGETDEVICE=\"Thunderball\""
echo "PTX_TARGETDEVICE=\"Thunderball\"" >> appenv 
echo "export PTX_TARGETDEVICE" >> appenv 

# optional environment variable that can be seen in application
if [ -n "$APPNAME" ]; then
 echo "PTX_APPNAME=\"$APPNAME\""
 echo "PTX_APPNAME=\"$APPNAME\"" >> appenv 
 echo "export PTX_APPNAME" >> appenv 
fi

echo "PTX_APPVERSION=\"$APPVERSION\""
echo "PTX_APPVERSION=\"$APPVERSION\"" >> appenv 
echo "export PTX_APPVERSION" >> appenv 

# save a copy in the install directory
cp appenv $DIRPATH/appenv

echo "Zipping up package"
zip -r $FILENAME appenv $DIRNAME

# optional copy up when building on target
if [ $copy = 1 ]; then
  if [ $rmpackage = 1 ]; then
     rm $copy_path/*
  fi
  cp $ZIPPATH/$FILENAME $copy_path/$FILENAME
fi

rm appenv
rm -r $DIRPATH

SCRIPTPATH=$HOMEDIR/src/Linux/InstallUtilities
DEVPATH=$HOMEDIR/src/Linux/Applications/Thunderball/Build
INSTALLFILENAME="install_$NOW.bsx"
export INSTALLDIRPATH=`mktemp -d /tmp/installtbpackage.XXXXXX`

cp $ZIPPATH/$FILENAME $INSTALLDIRPATH/$FILENAME
rm $FILENAME

tr -d '\r' < $SCRIPTPATH/UpStartInitScript.conf > $INSTALLDIRPATH/UpStartInitScript.conf
tr -d '\r' < $SCRIPTPATH/UpStartMainScript.conf > $INSTALLDIRPATH/UpStartMainScript.conf
tr -d '\r' < $SCRIPTPATH/UpStartInitSubScript.conf > $INSTALLDIRPATH/UpStartInitSubScript.conf
tr -d '\r' < $SCRIPTPATH/UpStartNodeScript.conf > $INSTALLDIRPATH/UpStartNodeScript.conf
tr -d '\r' < $SCRIPTPATH/UpStartEth0Script.conf > $INSTALLDIRPATH/UpStartEth0Script.conf
tr -d '\r' < $SCRIPTPATH/checkforupgrade.sh > $INSTALLDIRPATH/checkforupgrade.sh
tr -d '\r' < $SCRIPTPATH/smb.conf > $INSTALLDIRPATH/smb.conf
tr -d '\r' < $DEVPATH/sampledevenv > $INSTALLDIRPATH/sampledevenv
tr -d '\r' < $DEVPATH/setupDevice.sh > $INSTALLDIRPATH/setupDevice.sh

CDIR=$('pwd')
cd $INSTALLDIRPATH
tar czf $CDIR/payload.tar.gz *
cd $CDIR

#cat $SCRIPTPATH/selfextract.sh payload.tar.gz > $INSTALLFILENAME
tr -d '\r' < $SCRIPTPATH/selfextract.sh > $INSTALLFILENAME
cat payload.tar.gz >> $INSTALLFILENAME

# optional copy up when building on target
if [ $copy = 1 ]; then
  cp $INSTALLFILENAME $copy_path/$INSTALLFILENAME
  rm $INSTALLFILENAME
fi

rm payload.tar.gz
rm -r $INSTALLDIRPATH
rm -r $ZIPPATH

# try to force files onto disk
sync
