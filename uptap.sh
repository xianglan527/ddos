#!/bin/sh

# 配置变量
TAP_IFS="tap0 tap1"             # 定义所有 TAP 接口
BR_IF="br0"
LAN_IF="ens33"

# 错误处理函数
error_exit() {
    echo "$1" >&2
    exit 1
}

# 确保桥接转发功能已启用
# echo "启用桥接转发功能..."
# sudo sysctl -w net.bridge.bridge_nf_call_iptables=1
# sudo sysctl -w net.bridge.bridge_nf_call_ip6tables=1

# 加载 tun/tap 内核模块（如果尚未加载）
if ! lsmod | grep -q '^tun'; then
    echo "检测到未加载 tun 模块，正在加载..."
    sudo modprobe tun
    if [ $? -ne 0 ]; then
        error_exit "无法加载 tun 模块。"
    fi
else
    echo "tun 模块已加载。"
fi

# 检查并创建所有 TAP 接口，同时设置 MAC 地址
for TAP_IF in $TAP_IFS; do
    if ! ip link show "$TAP_IF" >/dev/null 2>&1; then
        echo "$TAP_IF 不存在，正在创建..."
        sudo ip tuntap add dev "$TAP_IF" mode tap
        if [ $? -ne 0 ]; then
            error_exit "无法创建 TAP 接口 $TAP_IF。"
        fi
        sudo ip link set dev "$TAP_IF" up
    fi
done

# 检查并创建桥接接口 (br0) 并禁止 STP
if ! ip link show "$BR_IF" >/dev/null 2>&1; then
    echo "$BR_IF 不存在，正在创建并禁止 STP..."
    sudo ip link add name "$BR_IF" type bridge
    if [ $? -ne 0 ]; then
        error_exit "无法创建桥接接口 $BR_IF。"
    fi
    sudo ip link set dev "$BR_IF" up
    sudo ip link set dev "$BR_IF" type bridge stp_state 0
else
    echo "$BR_IF 已存在，确保已禁用 STP..."
    sudo ip link set dev "$BR_IF" type bridge stp_state 0
fi

# 确保物理接口 (ens33) 存在
if ! ip link show "$LAN_IF" >/dev/null 2>&1; then
    error_exit "网络接口 $LAN_IF 不存在，请检查接口名称和配置。"
fi

# 将 ens33 从现有配置中移除 IP 并加入到 br0
echo "将 $LAN_IF 加入到 $BR_IF 桥接中..."
sudo ip link set dev "$LAN_IF" down
sudo ip addr flush dev "$LAN_IF"
sudo ip link set dev "$LAN_IF" master "$BR_IF"
sudo ip link set dev "$LAN_IF" up

# 将所有 TAP 接口添加到 br0（如果尚未添加）
for TAP_IF in $TAP_IFS; do
    if ! bridge link | grep -q "$TAP_IF"; then
        echo "将 $TAP_IF 附加到 $BR_IF..."
        sudo ip link set dev "$TAP_IF" master "$BR_IF"
        if [ $? -ne 0 ]; then
            error_exit "无法将 TAP 接口 $TAP_IF 附加到 $BR_IF。"
        fi
    else
        echo "$TAP_IF 已经附加到 $BR_IF。"
    fi
done

# 开启混杂模式
echo "开启混杂模式(Promiscuous Mode)..."
for IFACE in "$BR_IF" $TAP_IFS; do
    sudo ip link set dev "$IFACE" promisc on
    if [ $? -ne 0 ]; then
        error_exit "无法开启接口 $IFACE 的混杂模式。"
    else
        echo "已开启接口 $IFACE 的混杂模式。"
    fi
done

echo "网络配置完成！"

# 显示当前路由表
ip route

# 显示桥接接口状态，包括 STP 信息
echo "桥接接口 $BR_IF 状态："
bridge link show "$BR_IF"
