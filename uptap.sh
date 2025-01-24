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

# 确保桥接转发功能已启用
# echo "启用桥接转发功能..."
# sudo sysctl -w net.bridge.bridge_nf_call_iptables=1
# sudo sysctl -w net.bridge.bridge_nf_call_ip6tables=1

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

# 检查 br0 是否已经有 IP 地址
BR_IP_EXIST=$(ip -4 addr show "$BR_IF" | grep -oP '(?<=inet\s)\d+(\.\d+){3}/\d+')
# 检查 br0 是否已经设置默认网关
BR_GW_EXIST=$(ip route | grep "^default.*$BR_IF" | awk '{print $3}')

# 独立判断并设置 IP 地址
if [ -z "$BR_IP_EXIST" ]; then
    echo "$BR_IF 缺少 IP 地址，正在从 $LAN_IF 获取并设置。"

    # 获取 ens33 的 IP 地址和子网掩码
    ENS33_IP=$(ip -4 addr show "$LAN_IF" | grep -oP '(?<=inet\s)\d+(\.\d+){3}/\d+')
    if [ -z "$ENS33_IP" ]; then
        error_exit "无法获取接口 $LAN_IF 的 IP 地址。"
    fi

    # 分配 IP 地址给 br0
    echo "为 $BR_IF 分配 IP 地址 $ENS33_IP..."
    sudo ip addr add "$ENS33_IP" dev "$BR_IF"
    # sudo ip addr add "$ENS33_IP" broadcast 192.168.0.255 dev "$BR_IF"
    if [ $? -ne 0 ]; then
        error_exit "无法为 $BR_IF 设置 IP 地址。"
    fi

else
    echo "$BR_IF 已具有 IP 地址 $BR_IP_EXIST。"
fi



# 独立判断并设置默认网关
if [ -z "$BR_GW_EXIST" ]; then
    echo "$BR_IF 缺少默认网关，正在从 $LAN_IF 获取并设置。"

    # 获取 ens33 的默认网关
    ENS33_GW=$(ip route | grep "^default.*$LAN_IF" | awk '{print $3}')
    if [ -z "$ENS33_GW" ]; then
        echo "无法获取接口 $LAN_IF 的默认网关，将使用 br0 的网络网关作为默认网关。"

        # 获取 br0 的 IP 地址（不包含子网掩码）
        BR_IP_ADDR=$(echo "$BR_IP_EXIST" | cut -d'/' -f1)
        if [ -z "$BR_IP_ADDR" ]; then
            error_exit "无法获取 $BR_IF 的 IP 地址。"
        fi

        # 从 br0 的 IP 地址中提取前三个八位字节，并设置网关为 .1
        GATEWAY=$(echo "$BR_IP_ADDR" | awk -F. '{print $1"."$2"."$3".1"}')

        # 检查生成的网关是否有效
        if ! echo "$GATEWAY" | grep -E '^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$' >/dev/null 2>&1; then
            error_exit "生成的默认网关 $GATEWAY 无效。"
        fi

        ENS33_GW="$GATEWAY"
        echo "使用推导的默认网关: $ENS33_GW"
    fi

    # 删除现有的默认路由
    echo "删除现有的默认路由..."
    sudo ip route del default dev $LAN_IF 2>/dev/null

    # 设置默认网关
    echo "设置默认网关为 $ENS33_GW..."
    sudo ip route add default via "$ENS33_GW" dev "$BR_IF"
    if [ $? -ne 0 ]; then
        error_exit "无法设置默认网关。"
    fi
else
    echo "默认网关已设置为 $BR_GW_EXIST。"
fi

# 确保物理接口 (ens33) 存在
if ! ip link show "$LAN_IF" >/dev/null 2>&1; then
    error_exit "网络接口 $LAN_IF 不存在，请检查接口名称和配置。"
fi

# 将 ens33 加入到 br0
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
echo "开启混杂模式（Promiscuous Mode）..."
for IFACE in "$BR_IF" $TAP_IFS; do
    sudo ip link set dev "$IFACE" promisc on
    if [ $? -ne 0 ]; then
        error_exit "无法开启接口 $IFACE 的混杂模式。"
    else
        echo "已开启接口 $IFACE 的混杂模式。"
    fi
done

#启用 IP 转发
sudo sysctl -w net.ipv4.ip_forward=1

# 删除ens33的ip
sudo ip addr del via $ENS33_IP dev $LAN_IF 2>/dev/null
sudo ip route del via $ENS33_GW default dev $LAN_IF 2>/dev/null

echo "网络配置完成！"

# 显示当前路由表
ip route

# 显示桥接接口状态，包括 STP 信息
echo "桥接接口 $BR_IF 状态："
bridge link show "$BR_IF"