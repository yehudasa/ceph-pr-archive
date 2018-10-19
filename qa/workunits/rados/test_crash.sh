#!/bin/sh

set -x

# run on a single-node three-OSD cluster

sudo killall -ABRT ceph-osd
sleep 5

# kill caused coredumps; find them and delete them, carefully, so as
# not to disturb other coredumps, or else teuthology will see them
# and assume test failure.  sudos are because the core files are
# root/600
for f in $(find $TESTDIR/archive/coredump -type f); do
	gdb_output=$(echo "quit" | sudo gdb /usr/bin/ceph-osd $f)
	if expr match "$gdb_output" ".*generated.*ceph-osd.*" && \
	   ( \

	   	expr match "$gdb_output" ".*terminated.*signal 6.*" || \
	   	expr match "$gdb_output" ".*terminated.*signal SIGABRT.*" \
	   )
	then
		sudo rm $f
	fi
done

# let daemon find crashdumps on startup
sudo systemctl restart ceph-crash
sleep 30

# must be 3 crashdumps registered and moved to crash/posted
[ $(ceph crash ls | wc -l) = 3 ]  || exit 1
[ $(sudo find /var/lib/ceph/crash/posted/ -name meta | wc -l) = 3 ] || exit 1
