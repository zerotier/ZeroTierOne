#!/bin/bash

pushd spk/zerotier

make arch-x64-6.2.4
# make arch-braswell-6.2.4
# make arch-88f6281-6.2.4 #(std11)
# make arch-monaco-6.2.4 #(ZT_AES_NO_ACCEL=1)
# make arch-hi3535-6.2.4 #(take out -mfloat-abi=hard)
# make arch-comcerto2k-6.2.4 #(ZT_AES_NO_ACCEL=1, remove all flags from arm hf section)
# make arch-alpine4k-6.2.4 #(problem?)
# make arch-alpine-6.2.4 #(problem?)
# make arch-aarch64-6.2.4
# make arch-apollolake-6.2.4
# make arch-armada370-6.2.4
# make arch-armada375-6.2.4
# make arch-armada37xx-6.2.4
# make arch-armada38x-6.2.4
# make arch-armadaxp-6.2.4
# make arch-armv7-6.2.4
# make arch-avoton-6.2.4
# make arch-broadwell-6.2.4
# make arch-broadwellnk-6.2.4
# make arch-bromolow-6.2.4
# make arch-cedarview-6.2.4
# make arch-denverton-6.2.4
# make arch-evansport-6.2.4
# make arch-geminilake-6.2.4
# make arch-grantley-6.2.4
# make arch-kvmx64-6.2.4
# make arch-dockerx64-6.2.3
# make arch-purley-6.2.4
# make arch-qoriq-6.2.4
# make arch-rtd1296-6.2.4
# make arch-v1000-6.2.4
# make arch-x86-6.2.4

popd
