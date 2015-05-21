if [ $# = 1 ]; then
if [ $1 = "install" ]; then
	sh /home/ubuntu/src/Linux/Applications/Thunderball/overlays/makedtc.sh install
	export PTX_DTC_INSTALL=1
fi
fi

if [ $PTX_DTC_INSTALL = 1 ]; then
	echo "Hardware configured"
else
	sh /home/ubuntu/src/Linux/Applications/Thunderball/overlays/makedtc.sh install
	export PTX_DTC_INSTALL=1
fi

/home/ubuntu/src/Linux/Applications/LauncherApp/BBB/Debug/LauncherApp /home/ubuntu/src/Linux/Applications/Thunderball/ThunderballNodeProcessList.txt
