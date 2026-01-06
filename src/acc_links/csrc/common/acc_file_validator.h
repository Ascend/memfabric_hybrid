/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
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
} // namespace acc
} // namespace ock

#endif
