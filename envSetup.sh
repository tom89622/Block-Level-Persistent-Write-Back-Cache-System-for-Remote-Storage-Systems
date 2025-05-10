#!/bin/bash

sudo modprobe nbd 
sudo iscsiadm -m node -T iqn.1986-06.duke.edu:jc1068 -p 10.148.54.253 --login
sudo ipsec restart
sudo ipsec statusall
sudo ip xfrm state 

echo "plz start encryption on the server side"
echo "done"