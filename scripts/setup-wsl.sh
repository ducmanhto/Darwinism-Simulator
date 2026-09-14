#!/usr/bin/env bash
set -euo pipefail

cat <<'MSG'
DarwinSim WSL2 setup
--------------------
This installs Linux-side development tools only.
Install Docker Desktop on Windows first, enable the WSL 2 based engine, and
turn on WSL integration for this Ubuntu distribution. Do not install a second
Docker daemon inside WSL when using Docker Desktop integration.
MSG

sudo apt-get update
sudo apt-get install -y \
  build-essential cmake ninja-build git curl wget unzip jq ca-certificates gnupg lsb-release \
  libgrpc++-dev protobuf-compiler protobuf-compiler-grpc libprotobuf-dev \
  libsqlite3-dev sqlite3 libboost-system-dev

ARCH=$(uname -m)
case "$ARCH" in
  x86_64) KARCH=amd64; AWS_ARCH=x86_64 ;;
  aarch64|arm64) KARCH=arm64; AWS_ARCH=aarch64 ;;
  *) echo "Unsupported architecture: $ARCH" >&2; exit 1 ;;
esac

# kubectl: current stable client.
KVER=$(curl -fsSL https://dl.k8s.io/release/stable.txt)
curl -fsSLo /tmp/kubectl "https://dl.k8s.io/release/${KVER}/bin/linux/${KARCH}/kubectl"
sudo install -o root -g root -m 0755 /tmp/kubectl /usr/local/bin/kubectl
rm -f /tmp/kubectl

# k3d: disposable local K3s-in-Docker clusters.
curl -fsSL https://raw.githubusercontent.com/k3d-io/k3d/main/install.sh | bash

# Terraform from HashiCorp's signed apt repository.
wget -qO- https://apt.releases.hashicorp.com/gpg | \
  gpg --dearmor | sudo tee /usr/share/keyrings/hashicorp-archive-keyring.gpg >/dev/null
UBUNTU_CODENAME=$(. /etc/os-release && echo "${UBUNTU_CODENAME:-$VERSION_CODENAME}")
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/hashicorp-archive-keyring.gpg] https://apt.releases.hashicorp.com ${UBUNTU_CODENAME} main" | \
  sudo tee /etc/apt/sources.list.d/hashicorp.list >/dev/null
sudo apt-get update
sudo apt-get install -y terraform

# AWS CLI v2 official Linux installer.
rm -rf /tmp/aws /tmp/awscliv2.zip
curl -fsSL "https://awscli.amazonaws.com/awscli-exe-linux-${AWS_ARCH}.zip" -o /tmp/awscliv2.zip
unzip -q /tmp/awscliv2.zip -d /tmp
sudo /tmp/aws/install --update
rm -rf /tmp/aws /tmp/awscliv2.zip

echo
printf '%-12s %s\n' "cmake" "$(cmake --version | head -1)"
printf '%-12s %s\n' "g++" "$(g++ --version | head -1)"
printf '%-12s %s\n' "kubectl" "$(kubectl version --client --output=yaml | grep gitVersion | head -1 | xargs)"
printf '%-12s %s\n' "k3d" "$(k3d version | head -1)"
printf '%-12s %s\n' "terraform" "$(terraform version | head -1)"
printf '%-12s %s\n' "aws" "$(aws --version 2>&1)"

if command -v docker >/dev/null 2>&1; then
  docker version >/dev/null && echo "Docker Desktop WSL integration: OK" || true
else
  echo "Docker CLI is not available inside WSL. Enable Docker Desktop -> Settings -> Resources -> WSL Integration."
fi
