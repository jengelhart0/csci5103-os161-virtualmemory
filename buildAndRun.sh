set -e

if [[ $@ == *t* ]]; then 
	#cd ~/csci5103/os161/src/userland/testbin
	cd ~/csci5103/os161/src
	if [[ $@ == *c* ]]; then 
		bmake distclean > /dev/null
		bmake includes > /dev/null
	fi
	bmake depend > /dev/null
	bmake -j6
	bmake install > /dev/null
fi
if [[ $@ != *q* ]]; then
	cd ~/csci5103/os161/src/kern/compile/DUMBVM
	if [[ $@ == *c* ]]; then 
		bmake clean > /dev/null
	#	bmake distclean > /dev/null
	#	bmake includes > /dev/null
	fi
	bmake depend > /dev/null
	bmake -j6
	bmake install > /dev/null
fi

cd ~/csci5103/os161/root
if [[ $@ == *d* ]]; then 
	sys161 -w kernel
elif [[ $@ == *u* ]]; then 
	trace161 -w -t u kernel
elif [[ $@ == *r* ]]; then
	sys161 -X kernel
fi
