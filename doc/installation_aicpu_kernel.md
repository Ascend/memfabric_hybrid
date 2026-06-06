# 安装 HYBM AICPU Kernel Run 包

本文档介绍安装 `memfabric_hybrid_aicpu_kernel.run` 的方法。该安装包将 `cann-hybm-compat.tar.gz`、`libcann_hybm_kernel.json` 和 `cann_hybm_kernel_version` 安装到 CANN device-side OPP tree。

---

## 一、 前置条件

安装前请确认满足以下条件：

| 条件 | 说明 |
| -- | -- |
| **CANN 环境** | 已安装对应版本的 CANN 工具包，且 `ASCEND_HOME_PATH` 环境变量已配置 |
| **安装权限** | 具备目标 CANN 路径的写入权限（通常需要 `root` 或 `sudo`） |
| **Run 包** | 已获取 `memfabric_hybrid_aicpu_kernel.run`（参见下文"获取 run 包"） |

---

## 二、 获取 run 包

Run 包是项目 `build_aicpu_kernel_run` 构建目标的产物。执行项目构建流程（如 `bash script/build_and_pack_run.sh --build_aicpu_kernel_run ON`）后，产物位于：

```
./output/memfabric_hybrid_aicpu_kernel.run
```

也可从其他途径获取该 run 包后直接使用。

---

## 三、 安装前检查

### 1. 确认安装包存在

```bash
ls -l ./output/memfabric_hybrid_aicpu_kernel.run
```

### 2. 确认执行权限

若文件无可执行权限，需先添加：

```bash
chmod +x ./output/memfabric_hybrid_aicpu_kernel.run
```

### 3. 查看帮助

```bash
./output/memfabric_hybrid_aicpu_kernel.run -h
```

### 4. 确认 CANN 根目录

确保 `ASCEND_HOME_PATH` 环境变量已设置：

```bash
echo ${ASCEND_HOME_PATH}
```

---

## 四、 Run 包选项说明

| 选项 | 说明 |
| -- | -- |
| `-h, --help` | 显示帮助信息 |
| `--list` | 列出 payload 中的文件清单（含版本文件 `cann_hybm_kernel_version`）；仅当设置了 `ASCEND_HOME_PATH` 时，额外显示目标安装路径和待写入的 INI 配置 |
| `--check` | 检查 payload 完整性，读取并显示版本信息，以及检查目标路径可用性 |
| `--noexec` | 必须与 `--extract=<path>` 同时使用，仅解压到指定目录，不执行安装 |
| `--extract=<path>` | 将 payload 解压到指定目录；单独使用时解压后继续执行安装脚本；与 `--noexec` 同用时仅解压不安装 |
| `--install` | 执行安装（默认行为，不带任何选项时等效）。安装时会更新 `ascend_package_load.ini`。 |
| &emsp;`--install-for-all` | **必须与 `--install` 同时使用**。以所有用户可读权限安装（目录 755、文件 444），适用于多用户共享场景 |
| &emsp;`--force` | **必须与 `--install` 同时使用**。覆盖已存在的目标文件（不提供时，若目标已存在将报错） |
| `--uninstall` | 卸载已安装的文件，并从备份恢复原始文件。同时恢复/移除 `ascend_package_load.ini` 中本包的配置 |
| `--version` | 显示版本信息（读取 run 包内 `cann_hybm_kernel_version`） |

所有选项均可在 run 包上直接使用，例如：

```bash
# 查看版本
./output/memfabric_hybrid_aicpu_kernel.run --version

# 列出文件
./output/memfabric_hybrid_aicpu_kernel.run --list

# 检查完整性
./output/memfabric_hybrid_aicpu_kernel.run --check

# 仅解压到指定目录
./output/memfabric_hybrid_aicpu_kernel.run --noexec --extract=/tmp/mypkg
```

---

## 五、 安装与卸载

### 环境变量

`ASCEND_HOME_PATH` 必须设置且指向有效的 CANN 安装根目录：

```bash
export ASCEND_HOME_PATH=/usr/local/Ascend/ascend-toolkit/latest
```

### 安装

1. 设置 `ASCEND_HOME_PATH`（如未设置）。
2. 执行 run 包安装（默认安装）：

```bash
./output/memfabric_hybrid_aicpu_kernel.run
```

或显式指定 `--install`：

```bash
./output/memfabric_hybrid_aicpu_kernel.run --install
```

默认安装权限为：目录 750、文件 440。`ascend_package_load.ini` 保持系统默认权限（通常为普通用户可写），不受安装权限影响。

### 强制覆盖安装

如果目标文件已存在，可以使用 `--install --force` 强制覆盖：

```bash
./output/memfabric_hybrid_aicpu_kernel.run --install --force
```

安装包会在首次安装时自动备份原始文件（存放于 `$ASCEND_HOME_PATH/opp/vendors/cust/.hybm_aicpu_kernel_backup/`）。后续 `--force` 重复安装不会覆盖已有备份。

### 多用户共享安装

如需安装后所有用户均可读/可进入，使用 `--install --install-for-all`：

```bash
./output/memfabric_hybrid_aicpu_kernel.run --install --install-for-all
```

此模式下目标目录权限为 755，文件权限为 444，可供多用户共享使用。

### 强制覆盖 + 多用户共享

以上两个修饰选项可组合使用：

```bash
./output/memfabric_hybrid_aicpu_kernel.run --install --install-for-all --force
```

### ascend_package_load.ini 配置

安装时会向 `${ASCEND_HOME_PATH}/conf/ascend_package_load.ini` 写入以下配置（若文件不存在则自动创建）：

```text
name:cann-hybm-compat.tar.gz
install_path:2
optional:true
package_path:opp/vendors/cust/op_impl/aicpu/kernel
load_as_per_soc:false
```

安装前会自动备份该文件（如存在），确保卸载时可恢复原样。若安装前该文件不存在，卸载时会删除本次创建的文件。

### 卸载

```bash
./output/memfabric_hybrid_aicpu_kernel.run --uninstall
```

卸载操作会删除本包安装的文件；若存在备份，则恢复原始文件；同时恢复或移除 `${ASCEND_HOME_PATH}/conf/ascend_package_load.ini` 中本包的配置；随后清理备份目录。

---

## 六、 在 A5 环境关闭验签（不建议再生产环境关闭）

在 A5 硬件环境下，安装算子 run 包前可能需要关闭包签名验证，否则安装可能失败。

drv_hlt_dsmi_test 
可以使用底软的工具：drv_hlt_dsmi_test

**使用改工具关闭验签前，需要先打开授权**

工具获取
在编译出来的testcase包里，路径为test/lib/host下，drv_hlt_dsmi_test和libdrvhltdsmi.so
 
注意：testcase要和1 2包匹配
验签授权配置/查询/清除
打开授权标志 参数:devid 1 looptimes
./drv_hlt_dsmi_test set_auth_enable_user_config 0 1 1
关闭授权标志 参数:devid 0 looptimes
./drv_hlt_dsmi_test set_auth_enable_user_config 0 0 1
查询授权标志 参数:devid looptimes
./drv_hlt_dsmi_test get_auth_enable_user_config 0 1
清除授权标志  参数: devid looptimes
./drv_hlt_dsmi_test clear_auth_enable_user_config 0 1
 
参数说明：第一个是dev_id ，第二个是模式(0关闭，1开启)， 第三个是循环次数
 
打开授权后，会开启持久化配置。就是如果没有关闭授权，不管是重启还是重新升级，都会保持之前设置的验签模式和证书内容。
所以如果后续这个环境上一直都是要关闭验签的话 可以一直把授权打开
验签模式配置查询
配置验签模式
参数:【devid】【mode】【 looptimes】
./drv_hlt_dsmi_test set_cust_falg_device_info 0 0 1
./drv_hlt_dsmi_test set_cust_falg_device_info 0 1 1
./drv_hlt_dsmi_test set_cust_falg_device_info 0 2 1
./drv_hlt_dsmi_test set_cust_falg_device_info 0 3 1
 
8卡一键配置：
for i in {0..7};do ./drv_hlt_dsmi_test set_cust_falg_device_info $i 0 1; done;
 
mode说明：
0：关闭验证，不验签。
1：华为证书，使用华为证书验签。
2：客户证书，使用客户证书验签。
3: 华为证书和客户证书，或的关系
4：社区证书，使用社区证书验签。
5：华为证书和社区户证书，或的关系
6：社区证书和客户证书，或的关系
7：华为证书、客户证书、社区证书，或的关系
 
查询验签模式
参数:【devid】【looptimes】
./drv_hlt_dsmi_test get_cust_flag_device_info 0 1

---

## 七、 安装后验证

### 1. 检查退出码

安装命令返回 0 表示成功。

### 2. 检查 CANN OPP 下目标文件

确认对应文件已出现在 CANN device-side OPP 目录下：

```bash
ls -l ${ASCEND_HOME_PATH}/opp/vendors/cust/op_impl/aicpu/kernel/cann-hybm-compat.tar.gz
ls -l ${ASCEND_HOME_PATH}/opp/vendors/cust/op_impl/aicpu/config/libcann_hybm_kernel.json
ls -l ${ASCEND_HOME_PATH}/opp/vendors/cust/op_impl/aicpu/config/cann_hybm_kernel_version
```

### 3. 检查 ascend_package_load.ini

确认 `${ASCEND_HOME_PATH}/conf/ascend_package_load.ini` 中包含本包的配置：

```bash
grep -A4 '^name:cann-hybm-compat.tar.gz$' ${ASCEND_HOME_PATH}/conf/ascend_package_load.ini
```

### 4. 使用 --check 验证

也可使用 `--check` 选项直接验证 payload 完整性及目标路径可用性：

```bash
./output/memfabric_hybrid_aicpu_kernel.run --check
```

---

## 八、 常见问题

### 1. ASCEND_HOME_PATH 未设置

**现象**：安装或卸载时提示 `ASCEND_HOME_PATH is not set`。

**解决方法**：设置环境变量：

```bash
export ASCEND_HOME_PATH=/usr/local/Ascend/ascend-toolkit/latest
```

### 2. 权限不足

**现象**：安装时提示 `Permission denied`。

**解决方法**：使用 `chmod +x` 赋予执行权限，或以 `root` 用户（`sudo`）重新执行安装命令。

### 3. 目标文件已存在

**现象**：安装时提示 `target files already exist`。

**解决方法**：先执行 `--uninstall` 卸载，或使用 `--install --force` 强制覆盖安装。

### 4. 验签失败

**现象**：安装过程中提示签名验证失败错误。

**解决方法**：如果在 A5 环境上安装，请先按照 [六、在 A5 环境关闭验签](#六-在-a5-环境关闭验签) 章节的指引关闭验签后重试。
