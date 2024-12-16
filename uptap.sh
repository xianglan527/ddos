#!/bin/sh

# Configuration variables
TAP_IFS="tap0 tap1"             # 定义所有 TAP 接口
BR_IF="br0"
LAN_IF="ens33"

# Function to display error and exit
error_exit() {
    echo "$1" >&2
    exit 1
}

# 检查是否已经配置好 br0（存在且具有 IP 地址）
BR_EXIST_AND_CONFIGURED=false
if ip link show "$BR_IF" >/dev/null 2>&1; then
    # 检查 br0 是否有 IPv4 地址
    if ip -4 addr show "$BR_IF" | grep -qP '\binet\s\d+\.\d+\.\d+\.\d+/\d+'; then
        BR_EXIST_AND_CONFIGURED=true
        echo "$BR_IF 已存在且已配置 IP 地址，跳过获取 $LAN_IF 的 IP 和默认网关。"
    else
        echo "$BR_IF 已存在但未配置 IP 地址，准备进行配置。"
    fi
else
    echo "$BR_IF 不存在，准备创建并配置。"
fi

# 如果 br0 未配置，则获取 ens33 的 IP 和默认网关
if [ "$BR_EXIST_AND_CONFIGURED" = false ]; then
    # 获取 ens33 的当前 IPv4 地址和子网掩码
    ENS33_IP_INFO=$(ip -4 addr show "$LAN_IF" | grep -oP '(?<=inet\s)\d+(\.\d+){3}/\d+')

    if [ -z "$ENS33_IP_INFO" ]; then
        error_exit "接口 $LAN_IF 没有配置 IPv4 地址，无法设置桥接 IP。"
    fi

    BR_IP=$(echo "$ENS33_IP_INFO" | cut -d'/' -f1)
    BR_NETMASK=$(echo "$ENS33_IP_INFO" | cut -d'/' -f2)

    echo "检测到 $LAN_IF 的 IP 地址为 $BR_IP/$BR_NETMASK，准备配置桥接接口 $BR_IF。"

    # 获取 ens33 的当前默认网关
    CURRENT_DEFAULT_ROUTE=$(ip route show default 0.0.0.0/0 | grep "dev $LAN_IF")

    if [ -n "$CURRENT_DEFAULT_ROUTE" ]; then
        GW_IP=$(echo "$CURRENT_DEFAULT_ROUTE" | awk '{print $3}')
        echo "检测到 $LAN_IF 的默认网关为 $GW_IP。"
    else
        error_exit "未检测到通过 $LAN_IF 的默认网关，请手动设置 GW_IP。"
    fi
fi

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

# 检查并创建桥接接口 (br0) 并启用 STP
if ! ip link show "$BR_IF" >/dev/null 2>&1; then
    echo "$BR_IF 不存在，正在创建并启用 STP..."
    sudo ip link add name "$BR_IF" type bridge
    if [ $? -ne 0 ]; then
        error_exit "无法创建桥接接口 $BR_IF。"
    fi
    sudo ip link set dev "$BR_IF" up
    sudo ip link set dev "$BR_IF" type bridge stp_state 1
else
    echo "$BR_IF 已存在，确保已启用 STP..."
    sudo ip link set dev "$BR_IF" type bridge stp_state 1
fi

# 确保物理接口 (ens33) 存在
if ! ip link show "$LAN_IF" >/dev/null 2>&1; then
    error_exit "网络接口 $LAN_IF 不存在，请检查接口名称和配置。"
fi

# 如果 br0 未配置，则移除 ens33 上的所有 IP 地址并获取 IP 和 GW
if [ "$BR_EXIST_AND_CONFIGURED" = false ]; then
    # 移除 ens33 上的所有 IP 地址
    echo "移除 $LAN_IF 上的 IP 配置..."
    sudo ip addr flush dev "$LAN_IF"

    # 将 ens33 添加到 br0（如果尚未添加）
    if ! bridge link | grep -q "$LAN_IF"; then
        echo "将 $LAN_IF 附加到 $BR_IF..."
        sudo ip link set dev "$LAN_IF" master "$BR_IF"
    else
        echo "$LAN_IF 已经附加到 $BR_IF。"
    fi

    # 将所有 TAP 接口添加到 br0（如果尚未添加）
    for TAP_IF in $TAP_IFS; do
        if ! bridge link | grep -q "$TAP_IF"; then
            echo "将 $TAP_IF 附加到 $BR_IF..."
            sudo ip link set dev "$TAP_IF" master "$BR_IF"
        else
            echo "$TAP_IF 已经附加到 $BR_IF。"
        fi
    done

    # 配置 br0 的 IP 地址（如果尚未配置）
    if ! ip addr show "$BR_IF" | grep -q "$BR_IP/$BR_NETMASK"; then
        echo "为 $BR_IF 配置 IP 地址: $BR_IP/$BR_NETMASK..."
        sudo ip addr add "$BR_IP/$BR_NETMASK" brd + dev "$BR_IF"
    else
        echo "$BR_IF 已有相同的 IP 地址配置，跳过。"
    fi

    # 移除现有的默认路由（如果通过 ens33）
    CURRENT_DEFAULT_DEV=$(ip route | grep '^default' | awk '{print $5}')
    if [ "$CURRENT_DEFAULT_DEV" = "$LAN_IF" ]; then
        echo "移除通过 $LAN_IF 的现有默认路由..."
        sudo ip route del default via "$GW_IP" dev "$LAN_IF"
    fi

    # 配置默认网关通过 br0（如果尚未配置）
    if ! ip route show default | grep -q "via $GW_IP dev $BR_IF"; then
        echo "为 $BR_IF 配置默认网关: $GW_IP..."
        sudo ip route add default via "$GW_IP" dev "$BR_IF"
    else
        echo "默认网关已经通过 $BR_IF 配置，跳过。"
    fi
else
    # 如果 br0 已配置，则确保 ens33 被添加到 br0，并确保 TAP 接口也被添加
    echo "确保所有接口已附加到 $BR_IF..."

    # 将 ens33 添加到 br0（如果尚未添加）
    if ! bridge link | grep -q "$LAN_IF"; then
        echo "将 $LAN_IF 附加到 $BR_IF..."
        sudo ip link set dev "$LAN_IF" master "$BR_IF"
    else
        echo "$LAN_IF 已经附加到 $BR_IF。"
    fi

    # 将所有 TAP 接口添加到 br0（如果尚未添加）
    for TAP_IF in $TAP_IFS; do
        if ! bridge link | grep -q "$TAP_IF"; then
            echo "将 $TAP_IF 附加到 $BR_IF..."
            sudo ip link set dev "$TAP_IF" master "$BR_IF"
        else
            echo "$TAP_IF 已经附加到 $BR_IF。"
        fi
    done
fi

echo "网络配置完成！"

# 显示当前路由表
ip route

# 显示桥接接口状态，包括 STP 信息
echo "桥接接口 $BR_IF 状态："
bridge link show "$BR_IF"
