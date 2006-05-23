#!/bin/sh
# $FreeBSD: src/sbin/gpt/mkdisk.sh,v 1.2.2.1 2004/11/07 23:30:07 marcel Exp $

dd if=/dev/zero of=disk count=125307
sudo mdconfig -a -t vnode -f disk -u 4
sudo fdisk -f - md4 <<EOF
g c567 h13 s17
p 1 165 221 124865
EOF
sudo bsdlabel -w -r md4s1 pan60
sudo mdconfig -d -u 4
