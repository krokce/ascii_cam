#!/bin/sh
# Checks asciicam-wifi-conf, which turns wifi.txt into the settings file
# for wpa_supplicant, and the log line that names the joined network.
# Run with dash or BusyBox ash from the tests folder: sh test_wifi.sh
S=../br2-external/board/asciicam/rootfs-overlay/usr/bin/asciicam-wifi-conf
fail=0
check() {
	# $1 description, $2 expected, $3 actual
	if [ "$2" != "$3" ]; then
		echo "FAILED: $1"
		echo "  expected: $2"
		echo "  got:      $3"
		fail=$((fail + 1))
	fi
}

# One pair, as in an existing wifi.txt, with Windows line endings.
out=$(printf '# comment\r\nnetwork = ExampleNet\r\npassword = abcdefgh1234\r\n' | sh $S 2>/dev/null)
check "one pair" "$(printf 'network={\n\tssid="ExampleNet"\n\tpsk="abcdefgh1234"\n\tpriority=10\n}')" "$out"

# Two passwords for the same network, the first preferred.
out=$(printf 'network = Home\npassword = first-pass\nnetwork = Home\npassword = second-pass\n' | sh $S 2>/dev/null | grep -E 'psk|priority' | tr -d '\t' | tr '\n' ' ')
check "two passwords" 'psk="first-pass" priority=10 psk="second-pass" priority=9 ' "$out"

# Two networks, spaces inside a password kept, spaces after = dropped.
out=$(printf 'network =   Home Net\npassword =   my secret word\nnetwork=Phone\npassword=hotspot99\n' | sh $S 2>/dev/null | grep -E 'ssid|psk' | tr -d '\t' | tr '\n' ' ')
check "two networks and spaces" 'ssid="Home Net" psk="my secret word" ssid="Phone" psk="hotspot99" ' "$out"

# The example entry, a short password and a missing password are skipped.
msg=$(printf 'network = example-network-name\npassword = example-password\nnetwork = A\npassword = short\nnetwork = B\nnetwork = C\npassword = longenough\n' | sh $S 2>&1 >/dev/null)
check "skipped entries" "wifi.txt holds 1 usable networks or passwords to try; 2 entries were skipped, because a password was missing or not 8 to 63 characters long." "$msg"

# No usable pair gives an empty file.
out=$(printf 'network = example-network-name\npassword = example-password\n' | sh $S 2>/dev/null)
check "template only" "" "$out"

# The names list follows the same order as the network blocks.
out=$(printf 'network = Home\npassword = first-pass\nnetwork = A\npassword = x\nnetwork = Phone\npassword = hotspot99\n' | sh $S --names | tr '\n' ' ')
check "names" "Home Phone " "$out"

# The start-up script finds the entry number in the Wi-Fi log.
id=$(printf '9.1: wlan0: Trying to associate\n12.3: wlan0: CTRL-EVENT-CONNECTED - Connection to 11:22:33:44:55:66 completed [id=1 id_str=]\n' | sed -n 's/.*CTRL-EVENT-CONNECTED.*\[id=\([0-9]*\).*/\1/p' | tail -n 1)
check "connected entry" "1" "$id"

if [ "$fail" -gt 0 ]; then
	echo "$fail Wi-Fi configuration checks failed."
	exit 1
fi
echo "The Wi-Fi configuration script handled one pair, two passwords for one network, two networks, spaces, Windows line endings and skipped entries correctly, and the joined network is found in the Wi-Fi log."
