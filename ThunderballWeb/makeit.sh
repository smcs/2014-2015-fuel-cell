cd /opt/protonex/src/Linux/Applications/ThunderballUI

if [ $# = 1 ]; then
	if [ $1 = "rebuild" ]; then
		make -f Makefile.BBB clean
	fi
fi

make -f Makefile.BBB


