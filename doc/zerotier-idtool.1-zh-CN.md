zerotier-idtool(1) -- 用于创建和操作 ZeroTier 身份的工具
============================================================================

## 概要

`zerotier-idtool` <command> [args]

## 描述

**zerotier-idtool** 是一个用于处理 ZeroTier 身份的命令行实用程序。ZeroTier 身份由公钥/私钥对组成（或者如果只是 identity.public，则只有公钥）以及通过基于工作量证明的哈希函数从公钥派生的 10 位十六进制 ZeroTier 地址。

## 命令

当命令参数需要公钥或私钥（完整）身份时，身份可以指定为文件路径或直接在命令行上指定。

 * `help`:
   显示帮助。（不带命令运行也会显示帮助。）

 * `generate` [secret file] [public file] [vanity]:
   生成新的 ZeroTier 身份。如果指定了私钥文件，包含私钥的完整身份将写入此文件。如果指定了公钥文件，公钥部分将写入那里。如果未指定文件路径，完整的私钥身份将输出到 STDOUT。虚荣前缀是生成身份地址应以开头的一系列十六进制数字。通常不使用此功能，如果指定，由于身份生成的工作量证明函数的固有成本，生成可能需要很长时间。在 2.8ghz Core i5（使用一个核心）上生成具有已知 16 位（4 位数字）前缀的身份平均需要两个小时。

 * `validate` <identity, only public part required>:
   本地验证身份的密钥和工作量证明函数对应关系。

 * `getpublic` <full identity with secret>:
   提取 identity.secret 的公钥部分并打印到 STDOUT。

 * `sign` <full identity with secret> <file to sign>:
   使用 SHA512+ECC-256 (ed25519) 对文件内容进行签名。签名以十六进制格式输出到 STDOUT。

 * `verify` <identity, only public part required> <file to check> <signature in hex>:
   验证使用 `sign` 创建的签名。

 * `mkcom` <full identity with secret> [id,value,maxdelta] [...]:
   创建并签署网络成员证书。这通常不常用，因为网络控制器会自动执行此操作，主要包含用于测试目的。

## 示例

生成并转储新身份：

    $ zerotier-idtool generate

生成并写入新身份，包括私钥和公钥部分：

    $ zerotier-idtool generate identity.secret identity.public

生成以十六进制数字"beef"开头的虚荣地址（这将需要一段时间！）：

    $ zerotier-idtool generate beef.secret beef.public beef

使用身份的私钥对文件进行签名：

    $ zerotier-idtool sign identity.secret last_will_and_testament.txt

使用公钥验证文件签名：

    $ zerotier-idtool verify identity.public last_will_and_testament.txt

## 版权

(c)2011-2016 ZeroTier, Inc. -- https://www.zerotier.com/ -- https://github.com/zerotier

## 另请参见

zerotier-one(8), zerotier-cli(1)