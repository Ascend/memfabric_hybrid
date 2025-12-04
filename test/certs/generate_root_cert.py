#!/usr/bin/env python
# coding=utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MemFabric_Hybrid is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

from datetime import datetime, timedelta
import argparse

from cryptography import x509
from cryptography.x509.oid import NameOID
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.primitives.serialization import Encoding, PrivateFormat, NoEncryption


parser = argparse.ArgumentParser()
parser.add_argument('--ca_cert_path', type=str, help='The path to save the certificate of ca',
                    default='/opt/ock/security/certs/ca.cert.pem')
parser.add_argument('--ca_key_path', type=str, help='The path to save the private key of ca',
                    default='/opt/ock/security/certs/ca.private.key.pem')
args = parser.parse_args()


# 生成根证书的私钥
root_private_key = rsa.generate_private_key(
    public_exponent=65537,
    key_size=3072,
)

# 构建根证书
root_subject = x509.Name([
    x509.NameAttribute(NameOID.COUNTRY_NAME, "CN"),
    x509.NameAttribute(NameOID.STATE_OR_PROVINCE_NAME, "Sichuan"),
    x509.NameAttribute(NameOID.LOCALITY_NAME, "Chengdu"),
    x509.NameAttribute(NameOID.ORGANIZATION_NAME, "Huawei"),
    x509.NameAttribute(NameOID.COMMON_NAME, "My Root CA"),
])

root_cert = x509.CertificateBuilder().subject_name(
    root_subject
).issuer_name(
    root_subject
).public_key(
    root_private_key.public_key()
).serial_number(
    x509.random_serial_number()
).not_valid_before(
    datetime.utcnow()
).not_valid_after(
    datetime.utcnow() + timedelta(days=3650)
).add_extension(
    x509.BasicConstraints(ca=True, path_length=None),
    critical=True,
).sign(root_private_key, hashes.SHA256())

# 保存根证书的私钥和证书
with open(args.ca_key_path, "wb") as f:
    f.write(root_private_key.private_bytes(
        encoding=Encoding.PEM,
        format=PrivateFormat.PKCS8,
        encryption_algorithm=NoEncryption()
    ))

with open(args.ca_cert_path, "wb") as f:
    f.write(root_cert.public_bytes(Encoding.PEM))

print("Root certificate has been saved. ")