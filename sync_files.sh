#!/bin/bash

set -e

# 检查是否提供了源目录参数
if [ -z "$1" ]; then
  echo "用法: $0 <源目录>"
  exit 1
fi

SOURCE_DIR=$1
DEST_DIR=$(pwd)

# 检查源目录是否存在
if [ ! -d "$SOURCE_DIR" ]; then
  echo "错误: 源目录 '$SOURCE_DIR' 不存在。"
  exit 1
fi

# 定义要同步的目标子目录
TARGET_SUBDIRS=("api" "rtc_base")

# 遍历每个目标子目录
for SUBDIR in "${TARGET_SUBDIRS[@]}"; do
  DEST_SUBDIR="$DEST_DIR/$SUBDIR"
  SOURCE_SUBDIR="$SOURCE_DIR/$SUBDIR"

  # 检查目标子目录是否存在
  if [ ! -d "$DEST_SUBDIR" ]; then
    echo "警告: 目标目录 '$DEST_SUBDIR' 不存在，将跳过。"
    continue
  fi

  # 检查源子目录是否存在
  if [ ! -d "$SOURCE_SUBDIR" ]; then
    echo "警告: 源目录中缺少 '$SOURCE_SUBDIR' 目录，将跳过。"
    continue
  fi

  echo "正在从 '$SOURCE_SUBDIR' 同步文件到 '$DEST_SUBDIR'..."

  # 在目标子目录中查找所有文件，并逐个处理
  find "$DEST_SUBDIR" -type f | while read -r dest_file; do
    # 计算文件相对于目标子目录的路径
    relative_path="${dest_file#$DEST_SUBDIR/}"
    
    # 构建源文件的完整路径
    source_file="$SOURCE_SUBDIR/$relative_path"

    # 如果源文件存在，则执行拷贝
    if [ -f "$source_file" ]; then
      # 确保目标文件的父目录存在
      mkdir -p "$(dirname "$dest_file")"
      # 拷贝文件，并保留权限
      cp -p "$source_file" "$dest_file"
      echo "  已拷贝: $relative_path"
    fi
  done
done

echo "文件同步完成。" 