#!/bin/bash

CURRENT_DIR=$(
    cd $(dirname ${BASH_SOURCE:-$0})
    pwd
)

cd $CURRENT_DIR

OP=${1:0}

#       32*7168  64*7168  128*7168 256*7168  512*7168  1k*7168   2k*7168   4k*7168   8k*7168   16k*7168
params=(229376   458752   917504   1835008   3670016   7340032   14680064  29360128  58720256  117440512)

# 解析函数，去掉每张卡的最大值最小值求平均显示单卡性能，再对集群求平均
function fn_analyze_perf() {
    local search_dir="${1}"
    local keyword="${2}"

    find "$search_dir" -name "kernel_details.csv" -type f -exec awk -F, -v kw="$keyword" '
        $0 ~ kw {
            values[++count] = $11
            sum += $11
        }
        END {
            if (count >= 3) {
                min = values[1]
                max = values[1]
                for (i = 2; i <= count; i++) {
                    if (values[i] < min) min = values[i]
                    if (values[i] > max) max = values[i]
                }
                avg = (sum - min - max) / (count - 2)
                printf "%s: %.2f (原始数据:%d条, 去掉min=%.2f, max=%.2f)\n", FILENAME, avg, count, min, max
                print avg
            } else if (count > 0) {
                avg = sum / count
                printf "%s: %.2f (数据不足3条, 共%d条)\n", FILENAME, avg, count
                print avg
            } else {
                printf "%s: N/A (无匹配数据)\n", FILENAME
                print "NA"
            }
        }
    ' {} \; | awk -v kw="$keyword" '
        BEGIN {
            sum = 0
            count = 0
        }
        /^[0-9.]+$/ {
            sum += $1
            count++
            next
        }
        /^NA$/ {
            next
        }
        {
            print
        }
        END {
            if (count > 0) {
                total_avg = sum / count
                printf "关键字 \"%s\" 的总平均: %.2f (基于%d个有效文件)\n", kw, total_avg, count
            } else {
                printf "未找到关键字 \"%s\" 的有效数据\n", kw
            }
        }
    '
}

if [ "$OP" = "0" ]; then
  rm -rf profiling.zbal_*
  rm -rf profiling.hccl_*

  for param in "${params[@]}"; do
      bash test_zbal_reducescatter.sh "$param"
  done
else
  # zbal
  for param in "${params[@]}"; do
      echo "zbal reducescatter data_len:${param}" >> ../perf.txt
      dir_path="./profiling.zbal_${param}"
      fn_analyze_perf "${dir_path}" "ZeroBuffReduceScatter" >> ../perf.txt
  done

  # hccl
  for param in "${params[@]}"; do
      echo "hccl reducescatter data_len:${param}" >> ../perf.txt
      dir_path="./profiling.hccl_${param}"
      fn_analyze_perf "${dir_path}" "hcom_reduceScatter__" >> ../perf.txt
  done
fi