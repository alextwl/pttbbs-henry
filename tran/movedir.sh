#!/bin/sh
for i in `ls $1`;
do
	BRDNAME=$i;
	BRDPATH="$1$i";
	BRDFST=${BRDNAME:0:1};
	mv $BRDPATH "$2${BRDFST}/";
done;
