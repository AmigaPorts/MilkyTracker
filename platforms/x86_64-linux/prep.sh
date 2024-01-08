#!/bin/bash

# exit when any command fails
set -e

#set compiler params
sudo apt update
sudo apt install -y libsdl2-dev
cd "${CURPATH}"
