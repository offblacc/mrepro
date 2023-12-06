#! /usr/bin/env python3
'''Command & Control server za LAB2 iz MrePro
   Slusa na PORTUDP i prima registracije od Bot klijenata (poruka REG).
   Na STDIN prima naredbe: r, r2, r3, l, p, n, s, q i h
'''

import socket
import sys
import select
import subprocess

PORTUDP = 5555

# UDP_server
server_ip = "127.0.0.1"
server_port = "1234"
PROG_TCP = "1 " + server_ip + " " + server_port
PROG_UDP = "2 " + server_ip + " " + server_port

botovi = set()

def adrese():
    string = "3 "
    string2 = b"3"
    portovi = ["5678", "6789", "hostmon", "2222", "3333", "5555"]

    izlaz = subprocess.check_output("hostname --all-ip-addresses", shell=True)
    i=0
    for ip in izlaz.decode().split():
        if ":" in ip:
            continue
        string = string + ip + " " + portovi[i] + " "
        string2 = string2 + ip.encode() + b"\0" * (16-len(ip)) + portovi[i].encode() + b"\0" * (22-len(portovi[i]))
        i += 1
    return (string,string2)

(POPIS,POPIS_send) = adrese()

# zrtve
IPS = "20.0.0.11 20.0.0.12 20.0.0.13".split(' ')
PORTS = "1111 2222 svn".split(' ')

vals = []
for i in range(0, len(IPS)):
    tup = (IPS[i], PORTS[i])
    vals.append(tup)

POPIS2 = "3 "
POPIS2_send = "3"

for (ip, port) in vals:
    POPIS2 = POPIS2 + ip + " " + port + " "
    POPIS2_send = POPIS2_send + ip + "\0" * (16-len(ip)) + port + "\0" * (22-len(port))

port = "2222"
ip = "20.0.0.255"
POPIS3 = "3 " + ip + " " + port + " "
POPIS3_send = "3" + ip + "\0" * (16-len(ip)) + port + "\0" * (22-len(port))

def help():
    print("Podrzane su naredbe:")
    print("pt... bot klijentima ssalje poruku PROG_TCP")
    print("      struct MSG:" + PROG_TCP)
    print("pu... bot klijentima ssalje poruku PROG_UDP")
    print("      struct MSG:" + PROG_UDP)
    print("r ... bot klijentima ssalje poruku RUN s adresama iz ifconfig")
    print("      struct MSG:" + POPIS)
    print("r2... bot klijentima ssalje poruku RUN s nekim adresama")
    print("      struct MSG:" + POPIS2)
    print("s ... bot klijentima ssalje poruku STOP ('4')")
    print("l ... lokalni ispis adresa bot klijenata")
    print("n ... salje poruku: 'NEPOZNATA'")
    print("q ... bot klijentima ssalje poruku QUIT ('0') i zavrssava s radom")
    print("h ... ispis naredbi")

def naredbe(udp_sock):
    """Ucitaj i obradi naredbe sa stdin
    """
    naredba = sys.stdin.readline()
    if naredba == "r\n":
        print("--> RUN")
        for (adresa, port) in botovi:
            print("%s:%d <--- RUN: %s" % (adresa, port, POPIS))
            leng = udp_sock.sendto(POPIS_send, (adresa, port))
    elif naredba == "l\n":
        print("--> lista botova:")
        for (adresa, port) in botovi:
              print("%s:%s;" % (adresa, port),end=" ")
        print("")
    elif naredba == "r2\n":
        print("--> RUN2")
        for (adresa, port) in botovi:
            print("%s:%d <--- RUN: %s" % (adresa, port, POPIS2))
            leng = udp_sock.sendto(POPIS2_send.encode(), (adresa, port))
    elif naredba == "r3\n":
        print("--> RUN3")
        for (adresa, port) in botovi:
            print("%s:%d <--- RUN: %s" % (adresa, port, POPIS3))
            leng = udp_sock.sendto(POPIS3_send.encode(), (adresa, port))
    elif naredba == "pt\n":
        print("--> PROG_TCP")
        for (adresa, port) in botovi:
            print("%s:%d <--- PROG_TCP: %s" % (adresa, port, PROG_TCP))
            prog = PROG_TCP.split(' ')
            leng = udp_sock.sendto((prog[0] + prog[1] + "\0" * (16-len(prog[1])) + prog[2] + "\0" * (22-len(prog[2]))).encode(), (adresa, port))
    elif naredba == "pu\n":
        print("--> PROG_UDP")
        for (adresa, port) in botovi:
            print("%s:%d <--- PROG_UDP: %s" % (adresa, port, PROG_UDP))
            prog = PROG_UDP.split(' ')
            leng = udp_sock.sendto((prog[0] + prog[1] + "\0" * (16-len(prog[1])) + prog[2] + "\0" * (22-len(prog[2]))).encode(), (adresa, port))
    elif naredba == "n\n":
        print("--> NEPOZNATA")
        for (adresa, port) in botovi:
            print("%s:%d <-- NEPOZNATA" % (adresa, port))
            leng = udp_sock.sendto("NEPOZNATA".encode(), (adresa, port))
    elif naredba == "s\n":
        print("--> STOP")
        for (adresa, port) in botovi:
            print("%s:%d <--- STOP" % (adresa, port))
            leng = udp_sock.sendto('4'.encode(), (adresa, port))
    elif naredba == "q\n":
        print("--> QUIT")
        for (adresa, port) in botovi:
            print("%s:%d <--- QUIT" % (adresa, port))
            leng = udp_sock.sendto('0'.encode(), (adresa, port))
        print("Kraj programa.")
        sys.exit(0)
    elif naredba == "h\n":
        help()
    else:
        print("Nepoznata naredba: %s" % naredba)
        help()

#####
# __main__
#
def main():
    """Glavni"""
    udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_sock.bind(('', PORTUDP))

    inputs = [udp_sock, sys.stdin]

    while True:

        print('C&C> ',end="")
        sys.stdout.flush()
        readable, writable, exceptional = select.select(inputs, [], inputs)

        for s in readable:

            # Naredba na stdin:
            if s is sys.stdin:
                naredbe(udp_sock)

            # Novi UDP klijent / Bot
            elif s is udp_sock:
                data, (novi_klijent, novi_port) = udp_sock.recvfrom(4096)
                if data == b"REG\n":
                    print("\nBot klijent %s:%d" % (novi_klijent, novi_port))
                    botovi.add((novi_klijent, novi_port))
                else:
                    print("\nBot klijent treba poslati naredbu: REG")

            else:
                print("\nZaboravio sam na fd: ", s)
                sys.exit(1)

if __name__ == "__main__":
    main()
