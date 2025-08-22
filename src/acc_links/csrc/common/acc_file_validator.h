/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */
#ifndef ACC_LINKS_ACC_FILE_VALIDATOR_H
#define ACC_LINKS_ACC_FILE_VALIDATOR_H

#include <string>

#include "mf_file_util.h"

namespace ock {
namespace acc {
class FileValidator {
public:
    /** Regular the file path using realPath.
     * @param filePath raw file path
     * @param baseDir file path must in base dir
     * @param errMsg the err msg
     * @return
     */
    static bool RegularFilePath(const std::string &filePath, const std::string &baseDir, std::string &errMsg);

    /** Check the existence of the file and the size of the file.
     * @param configFile the input file path
     * @param errMsg the err msg
     * @return
     */
    static bool IsFileValid(const std::string &configFile, std::string &errMsg);

    /** Check the permission of the file.
     * @param filePath the input file path
     * @param mode the permission allowed
     * @param onlyCurrentUserOp strict check, only current user can write or execute
     * @param errMsg the err msg
     * @return
     */
    static bool CheckPermission(const std::string &filePath, const mode_t &mode, bool onlyCurrentUserOp,
                                std::string &errMsg);
};
}  // namespace acc
}  // namespace ock

#endif
