zerotier-one(8) -- ZeroTier 虚拟网络端点服务
============================================================

## 概要

`zerotier-one` [-switches] [working directory]

## 描述

**zerotier-one** 是负责将 Unix（Linux/BSD/OSX）系统连接到一个或多个 ZeroTier 虚拟网络并将这些网络呈现给系统作为虚拟网络端口的服务/守护进程。您可以将其视为点对点 VPN 客户端。

它通常由 systemd（Linux）或 launchd（Mac）等 init 系统运行，而不是由用户直接运行，并且必须以 root 身份运行，除非您给它 `-U` 开关并且不打算实际加入网络（例如，仅运行网络控制器微服务）。

**zerotier-one** 服务将其状态和其他文件保存在工作目录中。如果在启动时未指定此目录，则在 Linux 上默认为"/var/lib/zerotier-one"，在 Mac 上默认为"/Library/Application Support/ZeroTier/One"，在 FreeBSD 和其他类似的 BSD 上默认为"/var/db/zerotier-one"。工作目录应该是持久的。它不应该被系统清理守护进程自动清理或存储在易失性位置。丢失其 identity.secret 文件会导致丢失此系统的唯一 10 位 ZeroTier 地址和密钥。

只要使用不同的主端口（请参见开关）和不同的工作目录，就可以在同一系统上运行多个 **zerotier-one** 实例。但由于单个服务可以加入任意数量的网络，通常这样做没有意义。

**zerotier-one** 服务通过在 127.0.0.1:<主端口> 上可用的 JSON API 进行控制，默认主端口为 9993。访问此 API 需要通常在服务工作目录中的 authtoken.secret 文件中找到的授权令牌。在某些平台上，如果启用了其他安全选项，访问可能受到其他措施（如套接字对等 UID/GID 查找）的保护（这不是默认设置）。

首次在新的工作目录中启动服务时，它会生成 ZeroTier 身份。在慢速系统上，由于 ZeroTier 在地址生成中使用了反 DDOS/反伪造的工作量证明函数，此过程可能需要十秒或更长时间。这只发生一次，一旦生成，结果将保存在工作目录中的 identity.secret 中。此文件代表并定义/声明您的 ZeroTier 地址和相关的 ECC-256 密钥对。

## 开关

 * `-h`:
   显示帮助。

 * `-v`:
   显示 ZeroTier One 版本。

 * `-U`:
   跳过权限检查并允许非特权用户运行。这通常在使用包含网络控制器选项构建 **zerotier-one** 时使用。在这种情况下，ZeroTier 服务可能仅充当网络控制器，可能永远不会实际加入网络，在这种情况下，它不需要提升的系统权限。

 * `-p<port>`:
   指定不同的主端口。如果未给出，则默认为 9993。如果给定零，则每次选择随机端口。

 * `-d`:
   作为守护进程分叉并运行。

 * `-i`:
   调用 **zerotier-idtool** 个性，在这种情况下，二进制文件的行为类似于 zerotier-idtool(1)。如果二进制文件（或指向它的符号链接）的名称是 zerotier-idtool，则会自动发生。

 * `-q`:
   调用 **zerotier-cli** 个性，在这种情况下，二进制文件的行为类似于 zerotier-cli(1)。如果二进制文件（或指向它的符号链接）的名称是 zerotier-cli，则会自动发生。

## 示例

使用 OS 默认工作目录和默认端口作为守护进程运行：

    $ sudo zerotier-one -d

使用不同的工作目录和端口作为守护进程运行：

    $ sudo zerotier-one -d -p12345 /tmp/zerotier-working-directory-test

## 文件

这些文件位于服务的工作目录中。

 * `identity.public`:
   您的 ZeroTier 身份的公共部分，即您的 10 位十六进制地址和相关的公钥。

 * `identity.secret`:
   您的完整 ZeroTier 身份，包括其私钥。此文件标识网络上的系统，这意味着您可以通过复制此文件来移动 ZeroTier 地址，如果您想保存系统的静态 ZeroTier 地址，应该备份此文件。此文件必须受到保护，因为窃取其私钥将允许任何人冒充您的设备在任何网络上并解密流量。对于网络控制器，此文件特别敏感，因为它构成了控制器网络的证书颁发机构的私钥。

 * `authtoken.secret`:
   用于向服务的本地 JSON API 验证请求的机密令牌。如果它不存在，则在服务启动时从安全随机源生成。要使用，请在发送到 127.0.0.1:<主端口> 的 HTTP 请求中将"X-ZT1-Auth"标头发送。

 * `devicemap`:
   记住 zt# 接口编号到 ZeroTier 网络的映射，以便在重新启动时保持映射。在某些支持可以编码网络 ID 的较长接口名称的系统上（如 FreeBSD），此文件可能不存在。

 * `zerotier-one.pid`:
   ZeroTier 的 PID。此文件在正常关闭时删除。

 * `zerotier-one.port`:
   ZeroTier 的主端口，也是其 JSON API 在 127.0.0.1:<此端口> 上的位置。此文件在启动时创建，并由 zerotier-cli(1) 读取以确定它应该在哪里找到控制 API。

 * `controller.db`:
   如果 ZeroTier One 服务内置了网络控制器，则此文件包含控制器的 SQLite3 数据库。

 * `controller.db.backup`:
   如果 ZeroTier One 服务内置了网络控制器，则它会定期在此文件中备份其 controller.db 数据库（目前每 5 分钟一次，如果有更改）。由于此文件不是当前正在使用的 SQLite3 数据库，因此在不损坏的情况下备份更安全。在新备份中，文件被轮换而不是就地重写。

 * `iddb.d/` (目录):
   缓存 ZeroTier 在过去 60 天内与之通信的每个对等方的公共身份。此目录及其内容可以删除，但这可能导致连接启动较慢，因为需要我们出去重新获取与我们通信的对等方的完整身份。

 * `networks.d` (目录):
   这会缓存您所属网络的网络配置和证书信息。ZeroTier 在启动时扫描此目录以查找 <网络 ID>.conf 文件以回忆其网络，因此在此目录中"touch"一个空的 <网络 ID>.conf 文件是预配置 ZeroTier 在启动时加入特定网络而不使用 API 的方法。如果配置文件为空，ZeroTier 将从网络的控制器获取它。

## 版权

(c)2011-2016 ZeroTier, Inc. -- https://www.zerotier.com/ -- https://github.com/zerotier

## 另请参见

zerotier-cli(1), zerotier-idtool(1)