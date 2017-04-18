set -e

# config option: k = rerun kernel config (uses g=GENERIC or default=DUMBVM)
# compile options:
# q = quick (don't rebuild); u = userland; g = GENERIC (else DUMBVM); c = clean rebuild
# run mode:
# d = debug; t = trace; r = run

if [[ $@ == *k* ]]; then 
	cd ~/csci5103/os161/src/kern/conf/
	if [[ $@ == *g* ]]; then
		./config GENERIC
	else
		./config DUMBVM
	fi
fi

if [[ $@ != *q* ]]; then

	if [[ $@ == *u* ]]; then 
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

	if [[ $@ == *g* ]]; then
		cd ~/csci5103/os161/src/kern/compile/GENERIC
	else
		cd ~/csci5103/os161/src/kern/compile/DUMBVM
	fi

	if [[ $@ == *c* ]]; then 
		bmake clean > /dev/null
	#	bmake distclean > /dev/null
	#	bmake includes > /dev/null
	fi

	if [[ $@ == *k* ]]; then 
		bmake depend 
	fi

	bmake -j6
	bmake install > /dev/null
fi

cd ~/csci5103/os161/root
if [[ $@ == *d* ]]; then 
	sys161 -w kernel
elif [[ $@ == *t* ]]; then 
	trace161 -w -t u kernel
elif [[ $@ == *r* ]]; then
	sys161 -X kernel
fi
