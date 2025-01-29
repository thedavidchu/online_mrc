#!/usr/bin/bash
### @brief  This is the install script on a Debian-like machine.
### @note   This assumes you have a clean, up-to-date repository.
###         Otherwise, run the following (read notes carefully!!!):
###         ```bash
###         git fetch   # Fetch the remote
###         git checkout main
###         git reset --hard origin/main # Or, try `git pull` but you may have merge conflicts
###         git clean -ndx # List which files to delete
###         git clean -fdx  # Deletes all the listed files above!
###         ```
### @note   You need sudo privileges to run this script. If not, then
###         you can try building the required files from source and/or
###         use Python's PIP.
### @note   You will need access to Professor Ashvin Goel's QuickMRC
###         repository for this script to work.
### @todo   Allow building if Professor Ashvin Goel's QuickMRC repository
###         is not found.

# Make sure we are in the top level of this repository
cd "$(git rev-parse --show-toplevel)"

# Recursively clone repositories
# NOTE  You may need to ener your git credentials
git submodule update --init --recursive

# Install dependencies
sudo apt -y update
sudo apt -y upgrade
sudo apt install -y python3 python3-pip python3-setuptools \
    python3-wheel ninja-build meson
sudo apt install -y python3-numpy python3-matplotlib python3-tqdm
sudo apt install -y pkg-config libglib2.0-dev
# Depending on your OS version, this may default to 1.74. In this case,
# follow the instructions to install a backport. I found the package
# called 'libboost1.81-dev' worked.
sudo apt install -y libboost-all-dev

# Install dependencies for 1a1a11a/libCacheSim
sudo apt install -y cmake libgoogle-perftools-dev libzstd-dev

# Install environment niceties
sudo apt install -y clang-format clangd

# Install debugging tools
sudo apt install -y valgrind

# Build Meson
meson setup build
cd build
meson compile
