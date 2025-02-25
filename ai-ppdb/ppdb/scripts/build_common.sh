#!/bin/bash

# 通用构建函数库
# 依赖 build_env.sh 中的环境变量
# 仅在未加载时加载环境变量
if [ -z "${BUILD_DIR}" ]; then
    source "$(dirname "$0")/build_env.sh"
    if [ $? -ne 0 ]; then
        echo "Error: Failed to load build environment"
        exit 1
    fi
fi

# 提取核心编译函数
compile_core() {
    local src=$1
    local obj=$2
    local cflags=$3
    
    if [ ! -f "$src" ]; then
        echo -e "Error: Source file ${src} not found"
        return 1
    fi

    echo "-e Compiling ${src}..."
    "${CC}" ${cflags} -I"${PPDB_DIR}/include" -I"${SRC_DIR}" -I"${PPDB_DIR}/vendor/sqlite3" -I"${PPDB_DIR}/vendor/duckdb" -I"${LIBDILL_DIR}" -c "${src}" -o "${obj}"
    return $?
}

# 获取 CPU 核心数
get_cpu_count() {
    if [ "$(uname)" = "Darwin" ]; then
        sysctl -n hw.ncpu
    else
        nproc
    fi
}

# 错误处理
handle_error() {
    local msg=$1
    echo "Error: ${msg}" >&2
    exit 1
}

# 增量编译检查
needs_rebuild() {
    local src=$1
    local obj=$2
    [ ! -f "${obj}" ] || [ "${src}" -nt "${obj}" ]
}

# 包含依赖文件
include_deps() {
    for obj in "${BUILD_DIR}/obj"/*.o; do
        dep="${obj}.d"
        if [ -f "${dep}" ]; then
            # 处理依赖文件格式
            while read -r line; do
                if [[ "${line}" == *":"* ]]; then
                    # 跳过目标文件行
                    continue
                fi
                # 处理依赖文件路径
                dep_file=$(echo "${line}" | sed 's/\\//g' | xargs)
                if [ -n "${dep_file}" ]; then
                    # 检查文件是否存在
                    if [ ! -f "${dep_file}" ]; then
                        echo "Warning: Missing dependency file: ${dep_file}"
                    fi
                fi
            done < "${dep}"
        fi
    done
}

# 等待进程完成
wait_for_pids() {
    local pids=("$@")
    for pid in "${pids[@]}"; do
        wait "${pid}" || handle_error "Compilation failed for process ${pid}"
    done
}

# 并行编译函数
compile_files() {
    local src_files=("${@:1:$#-2}")
    local build_dir="${@: -2:1}"
    local module_name="${@: -1}"
    local jobs=$(get_cpu_count)
    local pids=()
    local local_objects=()
    
    # 创建构建目录（如果不存在）
    mkdir -p "${build_dir}"
    
    # 并行编译源文件
    for src in "${src_files[@]}"; do
        local obj="${build_dir}/$(basename "${src}" .c).o"
        local_objects+=("$obj")
        if needs_rebuild "${src}" "${obj}"; then
            compile_core "${src}" "${obj}" "${CFLAGS}" &
            pids+=($!)
            
            # 控制并行度
            while [ ${#pids[@]} -ge ${jobs} ]; do
                wait_for_pids "${pids[@]}"
                pids=()
            done
        fi
    done
    
    # 等待剩余的编译任务完成
    wait_for_pids "${pids[@]}"
    
    # 导出目标文件列表
    OBJECTS=("${local_objects[@]}")
}
