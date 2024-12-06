#!/bin/sh

#加载 tun/tap 内核模块
sudo modprobe tun

#创建 TAP 设备
sudo ip tuntap add dev tap0 mode tap

#启动 TAP 设备
sudo ip link set dev tap0 up

#创建一个桥接接口（bridge）
sudo ip link add name br0 type bridge
sudo ip link set dev br0 up

#将 ens33 和 tap0 接入桥接接口
sudo ip link set dev ens33 master br0
sudo ip link set dev tap0 master br0

#配置桥接接口的 IP 地址
sudo ip addr add 192.168.8.200/24 dev br0

#启动桥接接口
sudo ip link set dev br0 up

#为桥接接口 br0 配置默认网关
sudo ip route add default via 192.168.8.1