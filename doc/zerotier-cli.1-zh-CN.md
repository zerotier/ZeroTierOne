zerotier-cli(1) -- 控制本地 ZeroTier 虚拟网络服务
=================================================================

## 概要

`zerotier-cli` [-switches] <command> [arguments]

## 描述

**zerotier-cli** 提供了一个简单的命令行界面，用于访问 ZeroTier 虚拟网络端点服务 zerotier-one(8) 的本地 JSON API。

默认情况下，**zerotier-cli** 必须以 root 身份或使用 `sudo` 运行。如果您想允许非特权用户使用 **zerotier-cli** 控制系统 ZeroTier 服务，您可以在用户的主目录中创建 ZeroTier 服务授权令牌的本地副本：

    sudo cp /var/lib/zerotier-one/authtoken.secret /home/user/.zeroTierOneAuthToken
    chown user /home/user/.zeroTierOneAuthToken
    chmod 0600 /home/user/.zeroTierOneAuthToken

（ZeroTier 服务主目录的位置可能因平台而异。请参见 zerotier-one(8)。）

请注意，这使用户拥有了将系统连接或断开与任何虚拟网络的权限，这是一个重要的权限。

**zerotier-cli** 有几个命令行参数，可以在 `help` 输出中看到。最常用的两个是 `-j` 用于原始 JSON 输出和 `-D<path>` 用于指定替代的 ZeroTier 服务工作目录。原始 JSON 输出更容易在脚本中解析，还包含表格输出中没有的详细信息。`-D<path>` 选项指定了服务的 zerotier-one.port 和 authtoken.secret 文件的位置，如果服务未在系统的默认位置运行。

## 命令

 * `help`:
   显示 **zerotier-cli** 帮助。

 * `info`:
   显示有关此设备的信息，包括其 10 位 ZeroTier 地址和明显的连接状态。使用 `-j` 获取更详细的输出。

 * `listpeers`:
   此命令列出了此服务已知并在最近（大约在过去 30 分钟内）通信过的 ZeroTier VL1（虚拟层 1，点对点网络）对等方。这些不一定都是您虚拟网络上的所有设备，也可能包括一些您未加入的任何虚拟网络的设备。这些通常要么是根服务器，要么是网络控制器。

 * `listnetworks`:
   这将列出您的系统所属的网络以及有关它们的一些信息，例如您被分配的任何 ZeroTier 管理的 IP 地址。（手动分配给 ZeroTier 接口的 IP 地址不会在此处列出。使用标准网络接口命令查看这些地址。）

 * `join`:
   要加入网络，只需使用 `join` 和其 16 位十六进制网络 ID。就是这样。然后使用 `listnetworks` 查看状态。您要么会收到网络控制器的回复，其中包含证书和其他信息，如 IP 分配，要么会收到"访问被拒绝"。在这种情况下，您需要网络管理员在控制器上通过您的 10 位设备 ID（通过 `info` 可见）授权您的设备。

 * `leave`:
   离开网络就像加入网络一样简单。这会断开与网络的连接并从系统中删除其接口。请注意，网络上的对等方可能会在 `listpeers` 中停留长达 30 分钟，直到由于缺乏流量而超时。但如果它们不再与您共享网络，它们实际上无法以任何有意义的方式与您通信。

## 示例

加入"Earth"，ZeroTier 的大型公共派对线网络：

    $ sudo zerotier-cli join 8056c2e21c000001
    $ sudo zerotier-cli listnetworks
    ( 等待直到您获得 Earth IP )
    $ ping earth.zerotier.net
    ( 您现在应该能够 ping 通我们的 Earth 测试 IP )

离开"Earth"：

    $ sudo zerotier-cli leave 8056c2e21c000001

列出 VL1 对等方：

    $ sudo zerotier-cli listpeers

## 版权

(c)2011-2016 ZeroTier, Inc. -- https://www.zerotier.com/ -- https://github.com/zerotier

## 另请参见

zerotier-one(8), zerotier-idtool(1)