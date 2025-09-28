#  Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

from datetime import datetime, timedelta
import argparse
import os

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization


parser = argparse.ArgumentParser()
parser.add_argument('--ca_cert_path', type=str, help='The path to load the CA certificate',
                    default='/opt/ock/security/certs/ca.cert.pem')
parser.add_argument('--ca_key_path', type=str, help='The path to load the CA private key',
                    default='/opt/ock/security/certs/ca.private.key.pem')
parser.add_argument('--crl_path', type=str, help='The path to save the CRL',
                    default='/opt/ock/security/certs/ca.crl.pem')
parser.add_argument('--days', type=int, help='Number of days the CRL should be valid',
                    default=30)
parser.add_argument('--revoked_serials', type=str, nargs='*', help='Serial numbers of revoked certificates',
                    default=[])
args = parser.parse_args()

# 检查CA证书和私钥文件是否存在
if not os.path.exists(args.ca_cert_path):
    raise FileNotFoundError(f"CA certificate file not found: {args.ca_cert_path}")

if not os.path.exists(args.ca_key_path):
    raise FileNotFoundError(f"CA private key file not found: {args.ca_key_path}")

# 加载CA私钥
with open(args.ca_key_path, "rb") as key_file:
    ca_private_key = serialization.load_pem_private_key(
        key_file.read(),
        password=None,
    )

# 加载CA证书
with open(args.ca_cert_path, "rb") as cert_file:
    ca_cert = x509.load_pem_x509_certificate(cert_file.read())

# 创建CRL构建器
crl_builder = x509.CertificateRevocationListBuilder()
crl_builder = crl_builder.issuer_name(ca_cert.subject)
crl_builder = crl_builder.last_update(datetime.utcnow())
crl_builder = crl_builder.next_update(datetime.utcnow() + timedelta(days=args.days))

# 添加被吊销的证书
for serial in args.revoked_serials:
    revoked_cert_builder = x509.RevokedCertificateBuilder()
    revoked_cert_builder = revoked_cert_builder.serial_number(int(serial))
    revoked_cert_builder = revoked_cert_builder.revocation_date(datetime.utcnow())
    revoked_cert_builder = revoked_cert_builder.add_extension(
        x509.CRLReason(x509.ReasonFlags.key_compromise),
        critical=False
    )
    revoked_cert = revoked_cert_builder.build()
    crl_builder = crl_builder.add_revoked_certificate(revoked_cert)

# 签名生成CRL
crl = crl_builder.sign(
    private_key=ca_private_key,
    algorithm=hashes.SHA256()
)

# 保存CRL到文件
with open(args.crl_path, "wb") as f:
    f.write(crl.public_bytes(serialization.Encoding.PEM))

print(f"CRL has been saved to {args.crl_path}")
print(f"CRL is valid until {datetime.utcnow() + timedelta(days=args.days)}")
if args.revoked_serials:
    print(f"Revoked certificate serial numbers: {', '.join(args.revoked_serials)}")
else:
    print("No certificates were revoked (empty CRL)")