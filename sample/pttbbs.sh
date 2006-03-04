#!/bin/sh
# $Id: pttbbs.sh 2725 2005-05-16 18:36:27Z kcwu $
# �Ъ`�N�I�o���ɮױN�H root ���v������I
# �w�]�ϥ� bbs�o�ӱb���A�w�˥ؿ��� /home/bbs�C

case "$1" in
start)
	echo -n 'start...'
	# ��l�� shared-memory, ���J uhash, utmpsortd, timed(if necessary)
	# �p�G�ϥ� USE_HUGETLB ���ܽХ� root �] shmctl init
	/usr/bin/sudo -u bbs /home/bbs/bin/shmctl init > /dev/null

	# �H�H�ܯ��~
	/usr/bin/sudo -u bbs /home/bbs/bin/outmail &

	# ��H
	/usr/bin/sudo -u bbs /home/bbs/innd/innbbsd &

	# �Ұ� port 23 (port 23���ϥ� root �~��i�� bind ) �H��L
	#/home/bbs/bin/bbsctl start
	/usr/bin/sudo -u bbs /home/bbs/bin/mbbsd 3000

	# ����
	echo ' mbbsd'
	;;
stop)
	echo -n 'stop...'
	/usr/bin/killall outmail
	/usr/bin/killall innbbsd
	/usr/bin/killall mbbsd
	/usr/bin/killall shmctl
	/bin/sleep 2; /usr/bin/killall shmctl 2> /dev/null
	echo ' mbbsd'
	;;
*)
	echo "Usage: `basename $0` {start|stop}" >&2
	;;
esac

exit 0
