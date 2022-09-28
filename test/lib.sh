#!/bin/sh
# Helper functions for testing mdnsd

# Session set from Makefile before calling unshare -mrun
if [ -z "$SESSION" ]; then
	SESSION=$(mktemp -d mdnsd.XXXXXXXX)
	TMPSESS=1
fi

# Test name, used everywhere as /tmp/$NM/foo
NM=$(basename "$0" .sh)
DIR="${SESSION}/${NM}"
client="${DIR}/client"
server="${DIR}/server"
client_addr=192.168.42.101
server_addr=192.168.42.1
client_addr_6=2001:db8:0:f101::2a:65
server_addr_6=2001:db8:0:f101::2a:1

# Print heading for test phases
print()
{
	printf "\e[7m>> %-80s\e[0m\n" "$1"
}

SKIP()
{
	print "TEST: SKIP"
	[ $# -gt 0 ] && echo "$*"
	exit 77
}

FAIL()
{
	print "TEST: FAIL"
	[ $# -gt 0 ] && echo "$*"
	exit 99
}

OK()
{
	print "TEST: OK"
	[ $# -gt 0 ] && echo "$*"
	exit 0
}

# prefer source tree build, fall back to check one level up
mdnsd()
{
	bin="../src/mdnsd"
	[ -x "$bin" ] || SKIP "Cannot find mdnsd"

	print "Starting mdnsd ..."
	nsenter --net="$server" -- "$bin" -H test -n ../examples &
	echo "$! mdnsd" >>"$DIR/pids"
	sleep 1
}

# stop all instances of the mdnsd
mdnsd_stop()
{
	[ -f "${DIR}/pids" ] || SKIP "Cannot find PID file"

	print "Stopping mdnsd ..."
	leftpids=""
	while read pid prog; do
		if [ x"$prog" = x"mdnsd" ] ; then
			kill "$pid" 2>/dev/null
		else
			leftpids="${leftpids}$pid $prog\n"
		fi
	done < "${DIR}/pids"
	echo -n "${leftpids}" > "${DIR}/pids"

	sleep 1
}

# prefer source tree build, fall back to check one level up
mquery()
{
	bin="../src/mquery"
#	bin="/usr/bin/mdns-scan"
	[ -x "$bin" ] || SKIP "Cannot find mquery"
	nsenter --net="$client" -- "$bin" -i eth0 -w 2 "$@"
}

# Gather a pcap of the session
# Example:
#          collect eth0 -c10 'dst 224.0.0.251'
# Dump:
#          tshark -r "$DIR/pcap" 2>/dev/null | grep foo
#
collect()
{
    print "Starting collector on $client ..."
    nsenter --net="$client" -- tshark -w "$DIR/pcap" -lni eth0 2>/dev/null &
    echo "$! tshark" >> "$DIR/pids"
    sleep 2
}

# stop all instances of the pcap collector
stop_collect()
{
	[ -f "${DIR}/pids" ] || SKIP "Cannot find PID file"

	print "Stopping collector ..."
	leftpids=""
	while read pid prog; do
		if [ x"$prog" = x"tshark" ] ; then
			kill "$pid" 2>/dev/null
		else
			leftpids="${leftpids}$pid $prog\n"
		fi
	done < "${DIR}/pids"
	echo -n "${leftpids}" > "${DIR}/pids"

	sleep 1
}

# Set up two logically separated network namespaces, connected via a
# VETH pair, to simulate a basic LAN with two devices; one for mdnsd
# and one client (mquery)
topo_basic()
{
	touch "$server" "$client"

	unshare --net="$server" -- ip link set lo up
	nsenter --net="$server" -- ip link add eth0 type veth peer tmp0
	nsenter --net="$server" -- ip link set tmp0 netns $$
	nsenter --net="$server" -- ip link set eth0 up
	nsenter --net="$server" -- ip link set eth0 multicast on
	nsenter --net="$server" -- ip addr add "${server_addr}"/24 dev eth0
	nsenter --net="$server" -- ip route add default via "${server_addr}"
	nsenter --net="$server" -- ip -br link  > "$DIR/tmp"
	nsenter --net="$server" -- ip -br addr >> "$DIR/tmp"
	nsenter --net="$server" -- ip -br rout >> "$DIR/tmp"
	awk '{print "     "$0}' "$DIR/tmp"

	unshare --net="$client" -- ip link set lo up
	nsenter --net="$client" -- sleep 2 &
	sleep 0.3
	ip link set tmp0 netns $!
	nsenter --net="$client" -- ip link set tmp0 name eth0
	nsenter --net="$client" -- ip link set eth0 up
	nsenter --net="$client" -- ip link set eth0 multicast on
	nsenter --net="$client" -- ip addr add "${client_addr}"/24 dev eth0
	nsenter --net="$client" -- ip route add default via "${client_addr}"
	nsenter --net="$client" -- ip -br link  > "$DIR/tmp"
	nsenter --net="$client" -- ip -br addr >> "$DIR/tmp"
	nsenter --net="$client" -- ip -br rout >> "$DIR/tmp"
	awk '{print "     "$0}' "$DIR/tmp"

	print "Verifying connectivity ..."
	nsenter --net="$client" -- ping -c1 "${server_addr}" || FAIL "No connectivity"

	echo "$server" >> "$DIR/mounts"
	echo "$client" >> "$DIR/mounts"
}

topo_teardown()
{
	if [ -z "$NM" ]; then
		echo "NM variable unset, skippnig teardown"
		exit 1
	fi

	# shellcheck disable=SC2162
	if [ -f "${DIR}/pids" ]; then
		while read pid prog; do kill "$pid" 2>/dev/null; done < "${DIR}/pids"
	fi

	if [ -n "$KEEP_TEST_DATA" ]; then
		return
	fi

	# shellcheck disable=SC2162
	if [ -f "${DIR}/mounts" ]; then
		while read ln; do umount "$ln" 2>/dev/null; rm -f "$ln"; done < "${DIR}/mounts"
	fi

# NOTE: Uncomment the following two lines for debugging tests
#	echo "Resulting files in ${DIR}"
#	return;
	rm -rf "${DIR}"
	if [ -n "$TMPSESS" ] && [ -d "$SESSION" ]; then
		rm -rf "$SESSION"
	fi
}

signal()
{
	echo
	if [ "$1" != "EXIT" ]; then
		print "Got signal, cleaning up"
	fi
	topo_teardown
}

# props to https://stackoverflow.com/a/2183063/1708249
# shellcheck disable=SC2064
trapit()
{
	func="$1" ; shift
	for sig ; do
		trap "$func $sig" "$sig"
	done
}

topo()
{
	if [ $# -lt 1 ]; then
		print "Too few arguments to topo()"
		exit 1
	fi
	t=$1
	shift

	print "Creating world ..."
	case "$t" in
		basic)
			topo_basic
			;;
		teardown)
			topo_teardown
			;;
		*)
			print "No such topology: $t"
			exit 1
			;;
	esac
}

# Runs once when including lib.sh
mkdir -p "${DIR}"
touch "${DIR}/pids"
touch "${DIR}/mounts"
trapit signal INT TERM QUIT EXIT
