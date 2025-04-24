### 1 install dir
```
${INSTALL_PATH}/
    |--mxc
          |--memfabric_hybrid
              |-- latest
              |-- ${version}
                   |-- ${arch}-${os}
                        |-- include    (头文件)
                        |-- bin        (用于TLS相关二进制文件)
                        |-- lib64      (so库)
                        |-- whl        (python的whl包)


default ${INSTALL_PATH} is /usr/local/
```

### 2 rule of package name
```
mxc-memfabric-hybrid-${ascend_version}_${version}_${os}_${arch}.run
```

### 3 upgrade
support offline upgrade

### 4 where is the package
built from gitee and placed on gitee for downloading