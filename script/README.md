### 1 install dir
```
${INSTALL_PATH}/
          |--memfabric_hybrid
              |-- latest
              |-- set_env.sh
              |-- ${version}
                   |-- ${arch}-${os}
                        |-- include    (头文件)
                        |-- bin        (用于TLS相关二进制文件)
                        |-- lib64      (so库)
                        |-- whl        (python的whl包)
                   |-- uninstall.sh
                   |-- version.info


default ${INSTALL_PATH} is /usr/local/
```

### 2 rule of package name
```
memfabric-hybrid-${version}_${os}_${arch}.run
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