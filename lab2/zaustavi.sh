#! /bin/sh

# Before doing anything check for root rights.
if [ `id -u` -ne  0 ]; then
    echo "You must be root to run this script."
    exit 1
fi

for p in \
	'bash -c himage CnC python3 /CandC.py' \
	'python3 /CandC.py' \
	'nc -kul -p 3333' \
	'nc -kul -p 2222' \
	'nc -kul -p 1111' \
        '/server -p' \
        '/bot 10.0.0.10 5555'
do
    echo -n "Trazim program: " $p
pid=`pgrep -f "$p"`
if test -n "$pid"; then
    kill $pid
    echo "... kill"
else
    echo "... nema"
fi
done

