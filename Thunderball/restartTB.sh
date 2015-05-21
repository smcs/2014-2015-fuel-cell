if [ $# = 1 ]; then
if [ $1 = "install" ]; then
	sh /opt/protonex/src/Linux/Applications/Thunderball/overlays/makedtc.sh install
fi
fi
/opt/protonex/src/Linux/Applications/LauncherApp/bin/LauncherApp /opt/protonex/src/Linux/Applications/Thunderball/ThunderballProcessList.txt
