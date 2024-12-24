#!/bin/sh

# 配置变量
TAP_IFS="tap0 tap1"             # 定义所有 TAP 接口
BR_IF="br0"
LAN_IF="ens33"
BR_IP="192.168.74.2/24"         # br0 的 IP 地址和子网掩码
GATEWAY="192.168.74.1"          # 默认网关

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

# 检查并创建所有 TAP 接口
for TAP_IF in $TAP_IFS; do
    if ! ip link show "$TAP_IF" >/dev/null 2>&1; then
        echo "$TAP_IF 不存在，正在创建..."
        sudo ip tuntap add dev "$TAP_IF" mode tap
        if [ $? -ne 0 ]; then
            error_exit "无法创建 TAP 接口 $TAP_IF。"
        fi
        sudo ip link set dev "$TAP_IF" up
    else
        echo "$TAP_IF 已存在，跳过创建。"
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

# 分配 IP 地址给 br0（如果尚未分配）
if ! ip addr show "$BR_IF" | grep -q "$BR_IP"; then
    echo "为 $BR_IF 分配 IP 地址 $BR_IP..."
    sudo ip addr add "$BR_IP" dev "$BR_IF"
    if [ $? -ne 0 ]; then
        error_exit "无法为 $BR_IF 设置 IP 地址。"
    fi
else
    echo "$BR_IF 已具有 IP 地址 $BR_IP。"
fi

# 设置默认网关（如果尚未设置）
if ! ip route | grep -q "default via $GATEWAY"; then
    echo "设置默认网关为 $GATEWAY..."
    sudo ip route add default via "$GATEWAY"
    if [ $? -ne 0 ]; then
        error_exit "无法设置默认网关。"
    fi
else
    echo "默认网关已设置为 $GATEWAY。"
fi

# 确保物理接口 (ens33) 存在并保持其 IP 地址
if ! ip link show "$LAN_IF" >/dev/null 2>&1; then
    error_exit "网络接口 $LAN_IF 不存在，请检查接口名称和配置。"
fi

# 确保 ens33 被设置为 up 状态，但不添加到桥接接口 br0
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
