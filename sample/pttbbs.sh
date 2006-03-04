#!/bin/sh
# $Id: pttbbs.sh 2725 2005-05-16 18:36:27Z kcwu $
# 請注意！這個檔案將以 root 的權限執行！
# 預設使用 bbs這個帳號，安裝目錄為 /home/bbs。

case "$1" in
start)
	echo -n 'start...'
	# 初始化 shared-memory, 載入 uhash, utmpsortd, timed(if necessary)
	# 如果使用 USE_HUGETLB 的話請用 root 跑 shmctl init
	/usr/bin/sudo -u bbs /home/bbs/bin/shmctl init > /dev/null

	# 寄信至站外
	/usr/bin/sudo -u bbs /home/bbs/bin/outmail &

	# 轉信
	/usr/bin/sudo -u bbs /home/bbs/innd/innbbsd &

	# 啟動 port 23 (port 23須使用 root 才能進行 bind ) 以其他
	#/home/bbs/bin/bbsctl start
	/usr/bin/sudo -u bbs /home/bbs/bin/mbbsd 3000

	# 提示
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
