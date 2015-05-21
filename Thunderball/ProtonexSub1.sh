# PTX dependent application # 1
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

ulimit -c unlimited
sysctl -w "kernel.core_pattern=/opt/protonex/cores/TB-%e.core"
/opt/protonex/package/bin/ThunderballUI --pru-app /opt/protonex/package/bin/SpiPruMain.bin --id 5 --path /opt/protonex/package --logfile /opt/protonex/log/PtxSubMain.log --environ /opt/protonex/appuserenv --remoteui
