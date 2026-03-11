# version_repo.bzl

def _version_repo_impl(repository_ctx):
    # 1. 获取 VERSION 文件路径
    version_path = repository_ctx.path(repository_ctx.attr.version_file)
    
    # 2. 检查文件是否存在
    if not version_path.exists:
        fail("Error: VERSION file not found at {}".format(version_path))
    
    # 3. 读取并去除空白 (对应 file(READ) + string(STRIP))
    content = repository_ctx.read(version_path).strip()
    
    # 4. 验证格式 (对应 MATCHES "^[0-9]+\.[0-9]+\.[0-9]+$")
    parts = content.split(".")
    
    if len(parts) != 3:
        fail("Invalid version format: '{}'. Expected X.Y.Z (e.g., 1.2.3)".format(content))
    
    for i, part in enumerate(parts):
        if not part.isdigit():
            fail("Invalid version part: '{}'. All parts must be integers.".format(part))
    
    # 5. 提取各个版本字段 (对应 list(GET ...))
    version_major = parts[0]
    version_minor = parts[1]
    version_fix = parts[2]
    
    # 6. 打印状态信息 (对应 message(STATUS))
    # 注意：这会在 bazel sync 或构建开始时的配置阶段输出
    print("In memfabric BUILD_FROM_MEMCACHE: {}, version: {}.".format(
        repository_ctx.attr.build_from_memcache, # 假设传了这个属性，否则可移除
        content
    ))
    print("VERSION_MAJOR = {}, VERSION_MINOR = {}, VERSION_FIX = {}".format(
        version_major, version_minor, version_fix
    ))

    # 7. 获取last commit id
    git_path = repository_ctx.which("git")
    commit_id = "empty"
    print(version_path.dirname)
    if git_path:
        git_path_str = ""
        if hasattr(git_path, "path"):
            git_path_str = git_path.path
        else:
            git_path_str = git_path
        # 7.1. 执行 git rev-parse HEAD
        result = repository_ctx.execute(
            [git_path_str, "-C", version_path.dirname, "rev-parse", "HEAD"],
            quiet = False, # 打印错误到控制台方便调试
        )
        
        # 7.2. 检查返回值 (result.return_code)
        if result.return_code == 0:
            commit_id = result.stdout.strip()
        else:
            # 如果 git 命令失败 (例如不在 git 仓库中)
            commit_id = "empty"
    else:
        # 找不到 git 命令
        commit_id = "empty"

    print("GIT_LAST_COMMIT = {}".format(commit_id))

    # 8. 生成 BUILD 文件
    build_content = """
load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "version_info",
    hdrs = [],
    defines = [
    'PROJECT_VERSION_RAW={version}',
    'VERSION_MAJOR={major}',
    'VERSION_MINOR={minor}',
    'VERSION_FIX={fix}',
    'GIT_LAST_COMMIT={commit}',
    ],
    copts = [
        # 对应 add_compile_definitions(PROJECT_VERSION_RAW=...)
        #'-DPROJECT_VERSION_RAW={version}',
        
        # 对应 add_compile_definitions(VERSION_MAJOR=...)
        #'-DVERSION_MAJOR={major}',
        #'-DVERSION_MINOR={minor}',
        #'-DVERSION_FIX={fix}',
    ],
    visibility = ["//visibility:public"],
)
""".format(
        version=content,
        major=version_major,
        minor=version_minor,
        fix=version_fix,
        commit=commit_id,
    )
    
    print(build_content)

    repository_ctx.file("BUILD.bazel", build_content)

version_repo = repository_rule(
    implementation = _version_repo_impl,
    attrs = {
        "version_file": attr.label(default = "//:VERSION"),
        # 如果你需要传递 BUILD_FROM_MEMCACHE 这样的变量，可以定义属性
        "build_from_memcache": attr.string(default = "false"), 
    },
    local = True,
    environ = ["PATH"], # 依赖 PATH 环境变量来查找 git
)
