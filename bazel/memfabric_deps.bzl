load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("//bazel:version_repo.bzl", "version_repo")

def memfabric_deps():
    maybe(
        git_repository,
        name = "libboundscheck",
        remote = "https://atomgit.com/openeuler/libboundscheck.git",
        branch = "master",
        build_file = "@hcom//src/ubsocket/3rdparty/boundscheck:BUILD.bazel",
    )
    maybe(
        git_repository,
        name = "hcom",
        remote = "https://atomgit.com/openeuler/ubs-comm.git",
        branch = "br_BeiMing_MF_Poc",
    )

    version_repo(
        name = "version_info",
        build_from_memcache = "false",
    )
