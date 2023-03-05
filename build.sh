#!/bin/sh
g++ -o out test.cpp lib/ftp.cc


./out -s 192.168.2.94 -u meti -p 1111
