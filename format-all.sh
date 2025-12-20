#!/bin/bash

# 1. 确保脚本在当前目录执行（防止路径错误）
cd "$(dirname "$0")"

# 2. 检查 .clang-format 是否存在
if [ ! -f ".clang-format" ]; then
    echo "❌ 错误: 当前目录下未找到 .clang-format 文件！"
    echo "请确保脚本和配置文件在同一个目录下。"
    exit 1
fi

echo "🚀 开始格式化..."

# 3. 查找并格式化
# -type f: 只找文件
# \( ... \): 组合条件，查找 .c 或 .h
# -print0 和 -0: 安全处理带有空格的文件路径
find . -type f \( -name "*.c" -o -name "*.h" \) -print0 | xargs -0 clang-format -i -style=file

echo "✅ 所有 .c 和 .h 文件格式化完成！"