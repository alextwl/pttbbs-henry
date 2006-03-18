#!/bin/sh
for i in `find $1 -name .DIR`;
do
	mv $i "$i.bck";
	#rm -f $i;
	pttbbs/tran/wddir2ptt "$i.bck" $i;
done
