# make core files appear upon crash
sudo sysctl -w "kernel.core_pattern=/opt/protonex/cores/TB-%e.core"
ulimit -c unlimited

# start the fuel cell
sudo stop ProtonexMain
sudo sh /opt/protonex/src/Linux/Applications/Thunderball/restartTB.sh


