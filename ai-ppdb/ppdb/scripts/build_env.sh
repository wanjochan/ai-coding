#!/bin/bash

# 设置目录路径
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
echo "ROOT_DIR=${ROOT_DIR}"

PPDB_DIR="${ROOT_DIR}/ppdb"
BUILD_DIR="${PPDB_DIR}/build"
BIN_DIR="${PPDB_DIR}/bin"
SRC_DIR="${PPDB_DIR}/src"
INCLUDE_DIR="${PPDB_DIR}/include"
INTERNAL_DIR="${PPDB_DIR}/internal"
TEST_DIR="${PPDB_DIR}/test"
POLY_DIR="${BUILD_DIR}/poly"
INFRA_DIR="${SRC_DIR}/internal/infra"
# prebuilt software and mannually extract from curl -L -o cosmopolitan.zip https://justine.lol/cosmopolitan/cosmopolitan.zip
COSMOS="${ROOT_DIR}/repos/cosmos"
# cosmo compiler
COSMOCC="${ROOT_DIR}/repos/cosmocc"
# cosmo src
COSMOPOLITAN="${ROOT_DIR}/repos/cosmopolitan"

# 设置工具链路径
TOOLCHAIN_DIR="${COSMOCC}/bin"
CC="${TOOLCHAIN_DIR}/cosmocc"
AR="${TOOLCHAIN_DIR}/cosmoar"
OBJCOPY="${TOOLCHAIN_DIR}/objbincopy"

# 验证工具链
for tool in "$CC" "$AR"; do
    if [ ! -f "$tool" ]; then
        echo "Error: ${tool}"
        exit 1
    else
        echo "Found ${tool}"
    fi
done

# 设置构建标志
CFLAGS="-Os -fomit-frame-pointer -fno-pie -fno-pic -fno-common -fno-plt -mcmodel=large -finline-functions"
LDFLAGS="-static -Wl,--gc-sections -Wl,--build-id=none"

# 创建必要的目录
mkdir -p "${BUILD_DIR}" "${BIN_DIR}" "${POLY_DIR}"

# 导出环境变量
export ROOT_DIR PPDB_DIR BUILD_DIR CROSS9 CC AR OBJCOPY COSMOCC COSMOPOLITAN COSMOS CFLAGS LDFLAGS TEST_DIR SRC_DIR POLY_DIR INFRA_DIR
