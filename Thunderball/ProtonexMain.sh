# Main PTX application
#

# load environment
#. /opt/protonex/devenv
#. /opt/protonex/appenv

DEVENVFILE=/opt/protonex/devenv
APPENVFILE=/opt/protonex/appenv

if [ -f $DEVENVFILE ];
then
   . $DEVENVFILE
fi

if [ -f $APPENVFILE ];
then
   . $APPENVFILE
fi

mkdir -p /opt/protonex/cores/backup
cp /opt/protonex/cores/* /opt/protonex/cores/backup
rm /opt/protonex/cores/*
ulimit -c unlimited
sysctl -w "kernel.core_pattern=/opt/protonex/cores/TB-%e.core"
/opt/protonex/package/bin/Thunderball --path /opt/protonex/package --id 4 --logfile /opt/protonex/log/PtxDbgMain.log --pru-app /opt/protonex/package/bin/PwmPruMain.bin --environ /opt/protonex/appuserenv --paramfile /opt/protonex/Parameter.data
