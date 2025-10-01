# ZeroTier One 容器化部署！

**注意：** _大部分信息仅适用于 Docker 镜像。如需了解更多关于 ZeroTier 的信息，请查看代码仓库_：[这里](https://github.com/zerotier/ZeroTierOne) 或 [官方网站](https://www.zerotier.com)。

[ZeroTier](https://www.zerotier.com) 是一个面向地球的智能可编程以太网交换机。它允许所有联网设备、虚拟机、容器和应用程序像它们都位于同一个物理数据中心或云区域一样进行通信。

这是通过将加密寻址和安全的点对点网络（称为 VL1）与类似于 VXLAN 的以太网仿真层（称为 VL2）相结合来实现的。我们的 VL2 以太网虚拟化层包含企业级 SDN 的高级功能，如用于网络微分段和安全监控的细粒度访问控制规则。

所有 ZeroTier 流量都使用您控制的密钥进行端到端加密。大部分流量以点对点方式传输，尽管我们为无法建立点对点连接的用户提供免费（但较慢）的中继服务。

ZeroTier 的目标和设计原则受到原创 [Google BeyondCorp](https://static.googleusercontent.com/media/research.google.com/en//pubs/archive/43231.pdf) 论文和 [Jericho Forum](https://en.wikipedia.org/wiki/Jericho_Forum) 及其"去边界化"概念的启发。

访问 [ZeroTier 网站](https://www.zerotier.com/) 获取更多信息和 [预构建二进制包](https://www.zerotier.com/download/)。Android 和 iOS 应用在 Google Play 和 Apple 应用商店免费提供。

ZeroTier 采用 [BSL 版本 1.1](https://mariadb.com/bsl11/) 许可证。详情请见 [LICENSE.txt](https://github.com/zerotier/ZeroTierOne/blob/master/LICENSE.txt) 和 [ZeroTier 定价页面](https://www.zerotier.com/pricing)。ZeroTier 可在企业内部和学术机构中免费使用，也适用于非商业用途。某些商业用途，如基于 ZeroTier 构建闭源应用程序和设备，或提供 ZeroTier 网络控制器和网络管理 SaaS 服务，需要商业许可证。

ZeroTier 还包含少量第三方代码，这些代码不受我们的 BSL 许可证约束。请查看 [AUTHORS.md](https://github.com/zerotier/ZeroTierOne/blob/master/AUTHORS.md) 了解第三方代码列表、包含位置及其适用的许可证。ZeroTier 中的所有第三方代码都采用宽松许可证（MIT、BSD、Apache、公共领域等）。

## 构建 Docker 镜像

由于网络是大多数应用程序的基础而不是应用程序本身，许多人希望基于我们的方案构建自己的镜像是合理的。

该镜像基于 `debian:buster`。

`Dockerfile.release` 文件包含构建 README 其余部分所述镜像的构建说明。该构建支持多架构和多版本。

这些构建参数驱动构建过程：

- `PACKAGE_BASEURL`：要获取的软件包仓库的基本 URL。（默认：`https://download.zerotier.com/debian/buster/pool/main/z/zerotier-one/`）
- `ARCH`：软件包的架构，采用 debian 格式。必须与您的镜像架构匹配。（默认：`amd64`）
- `VERSION`：**必需** 要获取的 ZeroTier 版本。

您可以这样构建该镜像：

```
docker build -f Dockerfile.release -t mybuild --build-arg VERSION=1.6.5 .
```

## 使用 Docker 镜像

Docker 镜像中的 `entrypoint.sh` 有些不同；zerotier 将在后台启动，"主进程"实际上只是一个休眠的 shell 脚本。这允许 `zerotier-one` 在一些主要针对 Docker 的情况下优雅终止。

`zerotier/zerotier` 镜像需要 `CAP_NET_ADMIN` 能力，并且必须将 `/dev/net/tun` 设备转发给它。

要加入网络，只需在命令行中提供网络 ID；您可以提供多个网络。

```
docker run --name myzerotier --rm --cap-add NET_ADMIN --device /dev/net/tun zerotier/zerotier:latest abcdefdeadbeef00
```

加入您提供的所有网络后，它将休眠直到被终止。请注意，在 ZeroTier 中，加入网络并不一定意味着您获得了 IP 地址或可以做任何事情。您需要探测控制套接字：

```
docker exec myzerotier zerotier-cli listnetworks
```

以确保在尝试监听之前网络可用。如果没有预配置身份，这通常意味着需要前往中央管理面板并在您的 zerotier 身份旁点击复选框。

### 环境变量

您可以控制一些设置，包括使用的身份和用于与控制套接字交互的认证令牌（您可以通过 `localhost:9993` 转发和访问）。

- `ZEROTIER_JOIN_NETWORKS`：加入网络的附加方式。
- `ZEROTIER_API_SECRET`：在启动前替换 `authtoken.secret`，允许您管理控制套接字的认证密钥。
- `ZEROTIER_IDENTITY_PUBLIC`：zerotier-one 的 `identity.public` 文件。使用 `zerotier-idtool` 为您生成一个。
- `ZEROTIER_IDENTITY_SECRET`：zerotier-one 的 `identity.secret` 文件。使用 `zerotier-idtool` 为您生成一个。
- `ZEROTIER_LOCAL_CONF`：设置 zerotier-one 的 `local.conf` 文件内容

### 提示

- 对于高流量服务，将端口 `<dockerip>:9993` 转发到外部可能是个好主意。
- 将 `localhost:9993` 转发到您可以远程控制的控制网络可能是个好主意，只需确保通过环境变量正确设置您的认证令牌。
- 通过我们的 [terraform 插件](https://github.com/zerotier/terraform-provider-zerotier) 预生成身份可能简单得多。