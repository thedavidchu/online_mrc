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
sudo apt update
sudo apt upgrade
sudo apt install python3 python3-pip python3-setuptools \
                       python3-wheel ninja-build meson
sudo apt install python3-numpy python3-matplotlib python3-tqdm
sudo apt install pkg-config libglib2.0-dev

# Install environment niceties
sudo apt install clang-format clangd

# Install debugging tools
sudo apt install valgrind

# Build Meson
meson setup build
cd build
meson compile
