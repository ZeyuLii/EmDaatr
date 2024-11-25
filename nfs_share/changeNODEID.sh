#!/bin/bash

# 参数校验，确保用户传入了新内容（数字）
if [ $# -lt 1 ]; then
    echo "请提供新内容作为参数"
    exit 1
fi

# 检查输入的参数是否为数字
if ! [[ "$1" =~ ^[0-9]+$ ]]; then
    echo "请输入有效的数字"
    exit 1
fi

# 传入的参数
input_number=$1

# 计算结果
result=$((input_number + 1))

# 文件路径和行号
IP_file_path="/etc/network/interfaces"  # 要修改的IP配置文件路径
NAME_file_path="/etc/hostname"          # 要修改的主机名文件路径
hosts_file_path="/etc/hosts"
line_num=22                             # 要修改的行号

# 确保文件存在并可写
if [ ! -w "$IP_file_path" ]; then
    echo "无法写入 $IP_file_path，确保文件存在且有写权限"
    exit 1
fi

if [ ! -w "$NAME_file_path" ]; then
    echo "无法写入 $NAME_file_path，确保文件存在且有写权限"
    exit 1
fi

# 修改IP配置文件的指定行（第22行）
sed -i "${line_num}s/.*/address 192.168.50.${result}/" "$IP_file_path"
echo "已将IP文件的第${line_num}行修改为：address 192.168.50.${result}"

# 修改主机名文件（第一行）
sed -i "1s/.*/Zynq-Tronlong_NODE$input_number/" "$NAME_file_path"
sed -i "10s/.*127.0.0.1    Zynq-Tronlong_NODE$input_number/" "$hosts_file_path"

echo "已将主机名文件修改为：Zynq-Tronlong_NODE$input_number"

reboot

