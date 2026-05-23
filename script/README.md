### 1 install dir
```
${INSTALL_PATH}/
          |--memfabric_hybrid
              |-- latest
              |-- set_env.sh
              |-- include    (头文件)
              |-- ${version}
                   |-- ${arch}-${os}
                        |-- lib64      (so库)
                        |-- whl        (python的whl包)
                        |-- test       (测试文件)
                   |-- uninstall.sh
                   |-- version.info


default ${INSTALL_PATH} is /usr/local/
```

### 2 rule of package name
```
memfabric_hybrid-${version}_${os}_${arch}${xpu_suffix}.run

其中 xpu_suffix 为空（NPU）/_cpu（XPU_TYPE=NONE）/_gpu（XPU_TYPE=GPU）
```

### 3 upgrade
support offline upgrade

### 4 where is the package
built from gitee and placed on gitee for downloading

### 5 check library version
user can get library version by linux 'strings' command

example to get the library version using 'strings' as following:
```
strings libmf_smem.so | grep commit

library version: 1.0.0, build time: Apr 27 2025 08:46:17, commit: 4ad27e5b4bd3353c5c20f16e8f3b6da41268d4e0
```

## Unit Test

See [run_ut.md](run_ut.md) for unit test usage including fast incremental build mode.