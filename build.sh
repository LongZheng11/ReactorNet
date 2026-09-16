#!/usr/bin/env bash
# 一键配置 + 构建（测试跑：./build/test_xxx 或 ctest --test-dir build）
set -euo pipefail
cd "$(dirname "$0")"
cmake -B build -S .
cmake --build build -j
