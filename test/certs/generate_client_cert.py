#  Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

from datetime import datetime, timedelta
import argparse
import os

from cryptography import x509
from cryptography.x509.oid import NameOID
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.primitives.serialization import (Encoding, PrivateFormat, NoEncryption, load_pem_private_key,
                                                          BestAvailableEncryption)


parser = argparse.ArgumentParser()
parser.add_argument('--ca_cert_path', type=str, help='The path to load the certificate of ca',
                    default='/opt/ock/security/certs/ca.cert.pem')
parser.add_argument('--ca_key_path', type=str, help='The path to load the private key of ca',
                    default='/opt/ock/security/certs/ca.private.key.pem')
parser.add_argument('--client_cert_path', type=str, help='The path to save the certificate of the client',
                    default='/opt/ock/security/certs/client.cert.pem')
parser.add_argument('--client_key_path', type=str, help='The path to save the private key of the client',
                    default='/opt/ock/security/certs/client.private.key.pem')
parser.add_argument('--passphrase', type=str, help='Passphrase for encrypting the private key', default=None)
parser.add_argument('--passphrase_file', type=str, help='File to save the passphrase', 
                    default='/opt/ock/security/certs/client.passphrase')
args = parser.parse_args()


# 生成客户端证书的私钥
client_private_key = rsa.generate_private_key(
    public_exponent=65537,
    key_size=3072,
)

# 构建客户端证书
client_subject = x509.Name([
    x509.NameAttribute(NameOID.COUNTRY_NAME, "CN"),
    x509.NameAttribute(NameOID.STATE_OR_PROVINCE_NAME, "Sichuan"),
    x509.NameAttribute(NameOID.LOCALITY_NAME, "Chengdu"),
    x509.NameAttribute(NameOID.ORGANIZATION_NAME, "Huawei"),
    x509.NameAttribute(NameOID.COMMON_NAME, "client"),
])

# 读取根证书和私钥
with open(args.ca_key_path, "rb") as f:
    root_private_key = load_pem_private_key(f.read(), password=None)

with open(args.ca_cert_path, "rb") as f:
    root_cert = x509.load_pem_x509_certificate(f.read())

# 构建客户端证书
client_cert = x509.CertificateBuilder().subject_name(
    client_subject
).issuer_name(
    root_cert.subject
).public_key(
    client_private_key.public_key()
).serial_number(
    x509.random_serial_number()
).not_valid_before(
    datetime.utcnow()
).not_valid_after(
    datetime.utcnow() + timedelta(days=365)
).sign(root_private_key, hashes.SHA256())

# 确定私钥加密方式
if args.passphrase:
    encryption = BestAvailableEncryption(args.passphrase.encode())
    # 保存passphrase到文件（如果指定了文件路径）
    if args.passphrase_file:
        with open(args.passphrase_file, "w") as f:
            f.write(args.passphrase)
        # 设置密码文件权限为600 (仅所有者可读写)
        os.chmod(args.passphrase_file, 0o600)
else:
    encryption = NoEncryption()

# 保存客户端证书的私钥和证书
with open(args.client_key_path, "wb") as f:
    f.write(client_private_key.private_bytes(
        encoding=Encoding.PEM,
        format=PrivateFormat.PKCS8,
        encryption_algorithm=encryption
    ))

with open(args.client_cert_path, "wb") as f:
    f.write(client_cert.public_bytes(Encoding.PEM))

# 获取并打印证书序列号
client_cert_loaded = x509.load_pem_x509_certificate(client_cert.public_bytes(Encoding.PEM))
print(f"Client certificate and private key has been saved.")
print(f"Client certificate serial number: {client_cert_loaded.serial_number}")