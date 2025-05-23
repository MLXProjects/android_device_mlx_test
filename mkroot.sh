#!/bin/bash
echo "Generating rootfs.tar..."
OLD_PWD=$PWD
TARGET_PATH=$ANDROID_PRODUCT_OUT
cd "$TARGET_PATH"
sudo chmod -R 777 root system
sudo chown -R root:root root system
tar cpf ./rootfs.tar -C root .
tar rpf ./rootfs.tar system
cd "$OLD_PWD"
echo "File at $TARGET_PATH/rootfs.tar"
