#!/usr/bin/env bash
#
# Intro
# -----
# After years of manual fiddling, here's a script written by chatgpt to deal 
# with certs. We use cert chains to isolate root cert from running app. Not important
# in test, and we have no prod, but best practice FTW.
#
# It may not be best practice to push the certs to github LOL, but it's just for
# my local test server binding to localhost.
#
# In actual prod, certs would reside elsehwere.
#
# boa@20251009.
#
# PKI bootstrap for a test/lab TLS stack (Root CA -> Intermediate CA -> Server leaf).
#
# WHAT THIS DOES (high level):
#  1) Creates an OFFLINE Root CA (long lifetime). Never deploy this key to servers.
#  2) Creates an Intermediate CA signed by the Root (medium lifetime). Use this to sign leafs.
#  3) Creates a Server certificate (leaf) with proper EKU/SAN, signed by the Intermediate.
#  4) Builds a chain file (leaf + intermediate) for servers to present during TLS handshake.
#
# WHY THIS LAYOUT:
#  - Keeps the Root key out of harm’s way (reduced blast radius).
#  - Lets you rotate intermediates/leaves without touching the Root.
#  - Matches how clients expect chains (no root in the served chain).
#
# HOW TO USE THE OUTPUT:
#  - Server presents:   server_chain.pem  (leaf first, then intermediate)
#  - Server private key: server.key
#  - Clients trust:     root.crt  (install in the client trust store for your lab)
#
# VERIFICATION (quick checks after run):
#  - openssl verify -CAfile pki/root/root.crt -untrusted pki/intermediate/int.crt pki/server/server.crt
#  - openssl x509 -in pki/server/server.crt -noout -text | less   # sanity: EKU, KU, SAN, dates
#
# SAFETY:
#  - Root key is created with umask 077; keep it OFFLINE.
#  - Script refuses to overwrite files unless FORCE=1 is set in env.
#
# REQUIREMENTS: OpenSSL (no other deps). No installation steps provided by design.
#
# USAGE:
#   ./mkpki.sh
#   FORCE=1 ./mkpki.sh        # allow overwrite
#
set -euo pipefail

# -------- Config (tweak as you like) -----------------------------------------
OUTDIR="pki"
ROOT_CN="boa-test ROOT CA"
INT_CN="boa-test INTERMEDIATE CA"
SERVER_CN="server.local"

# Subject Alternative Names (comma-separated for DNS; single IP optional)
SERVER_SAN_DNS="server.local,localhost"
SERVER_SAN_IP="127.0.0.1"

# Lifetimes (days). Private PKI: be pragmatic.
DAYS_ROOT=7300      # ~20 years
DAYS_INT=3650       # ~10 years
DAYS_LEAF=1825      # ~5 years

# Key sizes
BITS_ROOT=4096
BITS_INT=4096
BITS_LEAF=2048

# -----------------------------------------------------------------------------
_force="${FORCE:-0}"
umask 077

log() { printf '[+] %s\n' "$*"; }
die() { printf '[!] %s\n' "$*" >&2; exit 1; }
need() { command -v "$1" >/dev/null 2>&1 || die "Missing dependency: $1"; }

need openssl

mkdir -p "${OUTDIR}/root" "${OUTDIR}/intermediate" "${OUTDIR}/server"

ROOT_KEY="${OUTDIR}/root/root.key"
ROOT_CRT="${OUTDIR}/root/root.crt"

INT_KEY="${OUTDIR}/intermediate/int.key"
INT_CSR="${OUTDIR}/intermediate/int.csr"
INT_CRT="${OUTDIR}/intermediate/int.crt"

SRV_KEY="${OUTDIR}/server/server.key"
SRV_CSR="${OUTDIR}/server/server.csr"
SRV_CRT="${OUTDIR}/server/server.crt"
SRV_CHAIN="${OUTDIR}/server/server_chain.pem"

_guard_file() {
  local f="$1"
  if [[ -f "$f" && "${_force}" != "1" ]]; then
    die "Refusing to overwrite existing file: $f (set FORCE=1 to override)"
  fi
}

# =============================================================================
# 1) ROOT CA (offline): long-lived, CA:true, keyCertSign,cRLSign
#    Purpose: anchor of trust. Never served, never deployed to networked hosts.
# =============================================================================
_guard_file "${ROOT_KEY}"; _guard_file "${ROOT_CRT}"

log "Creating Root CA key (${BITS_ROOT} bits) and self-signed cert (${DAYS_ROOT} days)"
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:${BITS_ROOT} -out "${ROOT_KEY}"
openssl req -x509 -new -key "${ROOT_KEY}" -sha256 -days "${DAYS_ROOT}" \
  -subj "/CN=${ROOT_CN}" \
  -addext "basicConstraints=critical,CA:true,pathlen:1" \
  -addext "keyUsage=critical,keyCertSign,cRLSign" \
  -addext "subjectKeyIdentifier=hash" \
  -out "${ROOT_CRT}"

# =============================================================================
# 2) INTERMEDIATE CA: signed by Root, CA:true, pathlen:0
#    Purpose: day-to-day signer for leafs; rotate here when needed.
# =============================================================================
_guard_file "${INT_KEY}"; _guard_file "${INT_CSR}"; _guard_file "${INT_CRT}"

log "Creating Intermediate CA key (${BITS_INT} bits) and CSR"
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:${BITS_INT} -out "${INT_KEY}"
openssl req -new -key "${INT_KEY}" -subj "/CN=${INT_CN}" -out "${INT_CSR}"

log "Signing Intermediate with Root (${DAYS_INT} days)"
openssl x509 -req -in "${INT_CSR}" -CA "${ROOT_CRT}" -CAkey "${ROOT_KEY}" \
  -sha256 -days "${DAYS_INT}" -set_serial 1001 \
  -extfile <(cat <<EOF
basicConstraints=critical,CA:true,pathlen:0
keyUsage=critical,keyCertSign,cRLSign
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid,issuer
EOF
) -out "${INT_CRT}"

# =============================================================================
# 3) SERVER LEAF: signed by Intermediate, CA:false, EKU=serverAuth, SAN required
#    Purpose: the actual certificate your server presents to clients.
# =============================================================================
_guard_file "${SRV_KEY}"; _guard_file "${SRV_CSR}"; _guard_file "${SRV_CRT}"

# Build SAN extension string
_SAN_DNS="$(printf '%s' "${SERVER_SAN_DNS}" | awk -F, '{for(i=1;i<=NF;i++) printf "DNS:%s%s",$i,(i<NF?",":"") }')"
_SAN_IP=""
if [[ -n "${SERVER_SAN_IP}" ]]; then
  _SAN_IP=",IP:${SERVER_SAN_IP}"
fi

log "Creating Server key (${BITS_LEAF} bits) and CSR (CN=${SERVER_CN}, SAN=${_SAN_DNS}${_SAN_IP})"
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:${BITS_LEAF} -out "${SRV_KEY}"
openssl req -new -key "${SRV_KEY}" -subj "/CN=${SERVER_CN}" -out "${SRV_CSR}"

log "Signing Server with Intermediate (${DAYS_LEAF} days) with proper EKU/SAN"
openssl x509 -req -in "${SRV_CSR}" -CA "${INT_CRT}" -CAkey "${INT_KEY}" \
  -sha256 -days "${DAYS_LEAF}" -set_serial 2001 \
  -extfile <(cat <<EOF
basicConstraints=CA:false
keyUsage=critical,digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth
subjectAltName=${_SAN_DNS}${_SAN_IP}
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid,issuer
EOF
) -out "${SRV_CRT}"

# =============================================================================
# 4) CHAIN FILE: what the server should present (leaf first, then intermediate).
#    Never include the Root in the served chain.
# =============================================================================
_guard_file "${SRV_CHAIN}"
log "Building server chain (leaf + intermediate)"
cat "${SRV_CRT}" "${INT_CRT}" > "${SRV_CHAIN}"

# Quick verification to catch obvious mistakes
log "Verifying server cert against Root with Intermediate as untrusted chain"
openssl verify -CAfile "${ROOT_CRT}" -untrusted "${OUTDIR}/intermediate/int.crt" "${SRV_CRT}" || {
  die "Verification failed"
}

log "Done."
log "Serve: ${SRV_CHAIN}  with key: ${SRV_KEY}"
log "Trust on clients: ${ROOT_CRT}"

