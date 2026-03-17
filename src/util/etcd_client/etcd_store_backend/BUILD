#load("@io_bazel_rules_go//go:def.bzl", "go_binary", "go_library")

genrule(
    name = "etcd_client_so",
    srcs = [
        "etcd_client_v3.go",
        "go.mod",
        "go.sum",
    ],
    outs = [
        "libetcd_client_v3.so",
        "libetcd_client_v3.h",
    ],
    #tags = ["no-remote", "no-cache"],
    tags = ["enable_etcd_client"],

    cmd = """
        GO_MOD_PATH=$(location go.mod)
        echo "DEBUG: go.mod path: $$GO_MOD_PATH"
        echo "DEBUG: http_proxy=$$http_proxy"
        echo "DEBUG: https_proxy=$$https_proxy"
        echo "DEBUG: GOPROXY=$$GOPROXY"
        echo "DEBUG: GOMODCACHE=$$GOMODCACHE"

        cp $(location go.mod) .
        cp $(location go.sum) .
        cp $(location etcd_client_v3.go) .

        # 3. 编译 C-shared 动态库
        go mod tidy
        go build -o libetcd_client_v3.so -buildmode=c-shared .

        ls -la libetcd_client_v3.so libetcd_client_v3.h

        echo "=== Copying to expected location ==="
        cp libetcd_client_v3.so "$(@D)"
        cp libetcd_client_v3.h "$(@D)/libetcd_client_v3.h"

    """,
)

cc_import(
    name = "etcd_client_import",
    shared_library = "libetcd_client_v3.so",
    hdrs = ["libetcd_client_v3.h"],
    visibility = ["//visibility:public"],
)