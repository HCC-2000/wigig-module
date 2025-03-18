#!/bin/bash

# 設置 pipenv 環境配置腳本
# 此腳本解決 pipenv 的 PATH 和編碼問題

echo "正在配置 pipenv 環境..."

# 添加 .local/bin 到當前會話的 PATH
export PATH="$PATH:$HOME/.local/bin"
echo "已將 $HOME/.local/bin 添加到當前 PATH"

# 檢查 .bashrc 中是否已存在 PATH 設置
if grep -q "export PATH=\"\$PATH:\$HOME/.local/bin\"" ~/.bashrc; then
    echo "PATH 設置已存在於 .bashrc 中"
else
    echo 'export PATH="$PATH:$HOME/.local/bin"' >> ~/.bashrc
    echo "已將 PATH 設置添加到 .bashrc"
fi

# 設置編碼環境變量
export LC_ALL=C.UTF-8
export LANG=C.UTF-8
echo "已設置 LC_ALL 和 LANG 為 C.UTF-8"

# 檢查 .bashrc 中是否已存在編碼設置
if grep -q "export LC_ALL=C.UTF-8" ~/.bashrc; then
    echo "LC_ALL 設置已存在於 .bashrc 中"
else
    echo 'export LC_ALL=C.UTF-8' >> ~/.bashrc
    echo "已將 LC_ALL 設置添加到 .bashrc"
fi

if grep -q "export LANG=C.UTF-8" ~/.bashrc; then
    echo "LANG 設置已存在於 .bashrc 中"
else
    echo 'export LANG=C.UTF-8' >> ~/.bashrc
    echo "已將 LANG 設置添加到 .bashrc"
fi

# 重新加載 .bashrc
source ~/.bashrc
echo "已重新加載 .bashrc"

echo "環境配置完成！現在可以使用 pipenv 了。"

# 測試 pipenv 是否可用
if command -v pipenv &> /dev/null; then
    echo "pipenv 命令可用，版本信息："
    pipenv --version
else
    echo "警告：pipenv 命令仍不可用，可能需要安裝："
    echo "pip3 install pipenv==2020.11.15"
fi
