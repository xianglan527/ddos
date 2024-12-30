#!/bin/sh

# 配置变量
TAP_IFS="tap0 tap1"             # 定义所有 TAP 接口
BR_IF="br0"
LAN_IF="ens33"
BR_IP="192.168.74.2/24"         # br0 的 IP 地址和子网掩码

# 定义 MAC 地址变量
TAP0_MAC="02:ca:fe:f0:0d:03"    # tap0 的 MAC 地址
TAP1_MAC="02:ca:fe:f0:0d:04"    # tap1 的 MAC 地址
BR_MAC="02:ca:fe:f0:0d:02"      # br0 的 MAC 地址

# 错误处理函数
error_exit() {
    echo "$1" >&2
    exit 1
}

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

        # 根据接口名设置不同的 MAC 地址
        if [ "$TAP_IF" = "tap0" ]; then
            sudo ip link set dev "$TAP_IF" address "$TAP0_MAC"
        elif [ "$TAP_IF" = "tap1" ]; then
            sudo ip link set dev "$TAP_IF" address "$TAP1_MAC"
        fi

        sudo ip link set dev "$TAP_IF" up
    else
        echo "$TAP_IF 已存在，跳过创建。"
        if [ "$TAP_IF" = "tap0" ]; then
            sudo ip link set dev "$TAP_IF" address "$TAP0_MAC"
        elif [ "$TAP_IF" = "tap1" ]; then
            sudo ip link set dev "$TAP_IF" address "$TAP1_MAC"
        fi
    fi
done

# 检查并创建桥接接口 (br0) 并禁用 STP
if ! ip link show "$BR_IF" >/dev/null 2>&1; then
    echo "$BR_IF 不存在，正在创建并禁用 STP..."
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

# 为 br0 分配 IP 地址（如果尚未分配）
if ! ip addr show "$BR_IF" | grep -q "$BR_IP"; then
    echo "为 $BR_IF 分配 IP 地址 $BR_IP..."
    sudo ip addr add "$BR_IP" dev "$BR_IF"
    if [ $? -ne 0 ]; then
        error_exit "无法为 $BR_IF 设置 IP 地址。"
    fi
else
    echo "$BR_IF 已具有 IP 地址 $BR_IP。"
fi

# 设置 br0 的唯一 MAC 地址
echo "设置 $BR_IF 的 MAC 地址为 $BR_MAC..."
sudo ip link set dev "$BR_IF" address "$BR_MAC"
if [ $? -ne 0 ]; then
    error_exit "无法设置 $BR_IF 的 MAC 地址。"
fi

# 确保物理接口 (ens33) 存在并保持其 IP 地址
if ! ip link show "$LAN_IF" >/dev/null 2>&1; then
    error_exit "网络接口 $LAN_IF 不存在，请检查接口名称和配置。"
fi

# 确保 ens33 为 up 状态，但不添加到桥接接口 br0
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

echo "网络配置完成！"

# 显示当前路由表
ip route

# 显示桥接接口状态，包括 STP 信息
echo "桥接接口 $BR_IF 状态："
bridge link show "$BR_IF"
