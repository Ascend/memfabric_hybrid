#!/bin/bash

CURRENT_DIR=$(cd $(dirname ${BASH_SOURCE:-$0}) && pwd)
echo $CURRENT_DIR

TEST_TYPE=bfloat16_t
export TEST_TYPE=$TEST_TYPE
export CURRENT_DIR=$CURRENT_DIR
export ZBAL_ENABLE_ALLTOALL_PERF_TEST=1
mkdir -p golden output

OP=${1:0}

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


RANK_PER_NODE=16
IPs=("" "")
ip_size=${#IPs[@]}

function get_node_idx()
{
    local_ips=`hostname -I | tr ' ' '\n'`
    for ip in $local_ips; do
        for i in "${!IPs[@]}"; do
            if [[ "${IPs[i]}" == "$ip" ]]; then
                echo $i
                return
            fi
        done
    done
    echo "IPs not corrent, local IP not match"
}

rank_sizes=(2 4 8 16)

if [ "$OP" == '0' ]; then
    rm -rf output "profiling.*"
    for rank_size in "${rank_sizes[@]}"; do
        bash test_zbal_alltoallv.sh 1 ${rank_size}
    done
else
    for rank_size in "${rank_sizes[@]}"; do
        echo "zbal alltoallv rank_size:${rank_size}" >> ../perf.txt
        dir_path="./profiling.zbal_${rank_size}"
        fn_analyze_perf "${dir_path}" "ZBALAlltoAllInner" >> ../perf.txt
    done

    for rank_size in "${rank_sizes[@]}"; do
        echo "hccl alltoall rank_size:${rank_size}" >> ../perf.txt
        dir_path="./profiling.hccl_${rank_size}"
        fn_analyze_perf "${dir_path}" "hcom_alltoall__" >> ../perf.txt
    done
fi