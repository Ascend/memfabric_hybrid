from cryptography import x509
from cryptography.x509.oid import NameOID
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.primitives.serialization import Encoding, PrivateFormat, NoEncryption, load_pem_private_key
from datetime import datetime, timedelta
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('--ca_cert_path', type=str, help='The path to save the certificate of ca',
                    default='/opt/ock/security/certs/ca.cert.pem')
parser.add_argument('--ca_key_path', type=str, help='The path to save the private key of ca',
                    default='/opt/ock/security/certs/ca.private.key.pem')
parser.add_argument('--server_cert_path', type=str, help='The path to save the certificate of the server',
                    default='/opt/ock/security/certs/server.cert.pem')
parser.add_argument('--server_key_path', type=str, help='The path to save the private key of the server',
                    default='/opt/ock/security/certs/server.private.key.pem')
args = parser.parse_args()


# 生成服务器证书的私钥
server_private_key = rsa.generate_private_key(
    public_exponent=65537,
    key_size=3072,
)

# 构建服务器证书
server_subject = x509.Name([
    x509.NameAttribute(NameOID.COUNTRY_NAME, "CN"),
    x509.NameAttribute(NameOID.STATE_OR_PROVINCE_NAME, "Sichuan"),
    x509.NameAttribute(NameOID.LOCALITY_NAME, "Chengdu"),
    x509.NameAttribute(NameOID.ORGANIZATION_NAME, "Huawei"),
    x509.NameAttribute(NameOID.COMMON_NAME, "localhost"),
])

# 读取根证书和私钥
with open(args.ca_key_path, "rb") as f:
    root_private_key = load_pem_private_key(f.read(), password=None)

with open(args.ca_cert_path, "rb") as f:
    root_cert = x509.load_pem_x509_certificate(f.read())

# 构建服务器证书
server_cert = x509.CertificateBuilder().subject_name(
    server_subject
).issuer_name(
    root_cert.subject
).public_key(
    server_private_key.public_key()
).serial_number(
    x509.random_serial_number()
).not_valid_before(
    datetime.utcnow()
).not_valid_after(
    datetime.utcnow() + timedelta(days=365)
).add_extension(
    x509.SubjectAlternativeName([x509.DNSName("localhost")]),
    critical=False,
).sign(root_private_key, hashes.SHA256())

# 保存服务器证书的私钥和证书
with open(args.server_key_path, "wb") as f:
    f.write(server_private_key.private_bytes(
        encoding=Encoding.PEM,
        format=PrivateFormat.PKCS8,
        encryption_algorithm=NoEncryption()
    ))

with open(args.server_cert_path, "wb") as f:
    f.write(server_cert.public_bytes(Encoding.PEM))

print("Server certificate and private key has been saved. ")