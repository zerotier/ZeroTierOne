ZeroTier - 全球区域网络
======

*本文档面向软件开发人员。如需了解如何使用ZeroTier，请参见：[官网](https://www.zerotier.com)、[文档网站](https://docs.zerotier.com)和[讨论论坛](https://discuss.zerotier.com)。*

ZeroTier是地球的智能可编程以太网交换机。它允许所有联网设备、虚拟机、容器和应用程序进行通信，就像它们都位于同一个物理数据中心或云区域中一样。

这是通过将加密寻址的安全点对点网络（称为VL1）与类似于VXLAN的以太网仿真层（称为VL2）相结合来实现的。我们的VL2以太网虚拟化层包含企业级SDN的高级功能，如用于网络微分段和安全监控的细粒度访问控制规则。

所有ZeroTier流量都使用只有您控制的密钥进行端到端加密。大多数流量以点对点方式传输，尽管我们为无法建立点对点连接的用户提供了免费（但较慢）的中继服务。

ZeroTier的目标和设计原则受到包括原始[Google BeyondCorp](https://static.googleusercontent.com/media/research.google.com/en//pubs/archive/43231.pdf)论文和具有"去边界化"概念的[Jericho论坛](https://en.wikipedia.org/wiki/Jericho_Forum)等的启发。

访问[ZeroTier官网](https://www.zerotier.com/)获取更多信息和[预构建二进制包](https://www.zerotier.com/download/)。Android和iOS的应用程序可在Google Play和Apple应用商店免费获取。

ZeroTier采用[BSL 1.1版本](https://mariadb.com/bsl11/)授权。详情请见[LICENSE.txt](LICENSE.txt)和[ZeroTier定价页面](https://www.zerotier.com/pricing)。ZeroTier可在企业内部和学术机构以及非商业用途中免费使用。某些类型的商业用途，如基于ZeroTier构建闭源应用程序和设备，或提供ZeroTier网络控制器和网络管理SaaS服务，需要商业许可证。

ZeroTier还包含少量第三方代码，这些代码不受我们的BSL许可证约束。有关第三方代码列表、包含位置及其适用许可证，请参见[AUTHORS.md](AUTHORS.md)。ZeroTier中的所有第三方代码均采用宽松许可证（MIT、BSD、Apache、公共领域等）。

### 入门指南

ZeroTier世界中的所有内容都由两种标识符控制：40位/10位数字的*ZeroTier地址*和64位/16位数字的*网络ID*。这些标识符可通过长度轻松区分。ZeroTier地址标识一个节点或"设备"（笔记本电脑、手机、服务器、虚拟机、应用程序等），而网络ID标识设备可以加入的虚拟以太网网络。

ZeroTier地址可以被视为支持VLAN的巨型全球企业以太网智能交换机上的端口号。网络ID是可分配给这些端口的VLAN ID。单个端口可分配给多个VLAN。

ZeroTier地址看起来像`8056c2e21c`，网络ID看起来像`8056c2e21c000001`。网络ID由该网络主控制器的ZeroTier地址和在该控制器上标识网络的任意24位ID组成。网络控制器与SDN协议（如[OpenFlow](https://en.wikipedia.org/wiki/OpenFlow)）中的SDN控制器大致相似，但与VXLAN和VL2之间的类比一样，这不应被理解为协议或设计相同。您可以使用我们在[my.zerotier.com](https://my.zerotier.com/)上提供的便捷且经济的SaaS托管控制器，或者如果您不介意处理JSON配置文件或编写脚本来[运行您自己的控制器](controller/)。

### 项目布局

基础路径包含ZeroTier One服务的主入口点（`one.cpp`）、自测代码、makefile等。

 - `artwork/`：图标、徽标等。
 - `attic/`：我们希望保留以供参考的旧内容和实验代码。
 - `controller/`：参考网络控制器实现，默认在桌面和服务器构建目标上构建和包含。
 - `debian/`：在Linux上构建Debian包的文件。
 - `doc/`：手册页和其他文档。
 - `ext/`：第三方库、我们在某些平台（Mac和Windows）上提供的二进制文件以及安装支持文件。
 - `include/`：ZeroTier核心的包含文件。
 - `java/`：与我们的Android移动应用程序一起使用的JNI包装器。（整个Android应用程序不是开源的，但将来可能会开源。）
 - `node/`：ZeroTier虚拟以太网交换机核心，设计为与代码的其余部分完全分离，并能够构建为独立的与操作系统无关的库。开发者注意：此处不要使用C++11特性，因为我们希望这能在缺乏C++11支持的旧嵌入式平台上构建。其他地方可以使用C++11。
 - `osdep/`：支持和集成操作系统的代码，包括仅为某些目标构建的平台特定内容。
 - `rule-compiler/`：用于定义网络级规则的JavaScript规则语言编译器。
 - `service/`：ZeroTier One服务，它包装ZeroTier核心并为桌面、笔记本电脑、服务器、虚拟机和容器提供类似VPN的虚拟网络连接。
 - `windows/`：Visual Studio解决方案文件、Windows服务代码和Windows任务栏应用程序UI。
 - `zeroidc/`：ZeroTier服务用于登录启用SSO网络的OIDC实现。（这部分是用Rust编写的，将来此存储库中会出现更多Rust代码。）

### 贡献

请从`dev`分支发起拉取请求。

通过将`dev`合并到`main`然后打标签和构建来发布版本。

### 构建和平台说明

在Mac和Linux上只需输入`make`。在FreeBSD和OpenBSD上需要`gmake`（GNU make），可从包或端口安装。Windows在`windows/`中有Visual Studio解决方案。

 - **Mac**
   - 需要macOS 10.13或更新版本的Xcode命令行工具。
   - *如果构建中启用了SSO*，则需要x86_64和ARM64目标的Rust。
 - **Linux**
   - 所需的最低编译器版本为GCC/G++ 8.x或CLANG/CLANG++ 5.x。
   - Linux makefile会自动检测并优先使用clang/clang++（如果存在），因为它在大多数情况下会产生更小且稍快的二进制文件。您可以通过在make命令行提供CC和CXX变量来覆盖。
   - *如果构建中启用了SSO*，则需要x86_64和ARM64目标的Rust。
 - **Windows**
   - Windows 10或更新版本上的Visual Studio 2022。
   - *如果构建中启用了SSO*，则需要x86_64和ARM64目标的Rust。
 - **FreeBSD**
   - 需要GNU make。输入`gmake`进行构建。
   - 需要`binutils`。输入`pkg install binutils`进行安装。
   - *如果构建中启用了SSO*，则需要x86_64和ARM64目标的Rust。
 - **OpenBSD**
   - OpenBSD上的网络成员身份限制为四个，因为只有四个tap设备（`/dev/tap0`到`/dev/tap3`）。
   - 需要GNU make。输入`gmake`进行构建。
   - *如果构建中启用了SSO*，则需要x86_64和ARM64目标的Rust。

输入`make selftest`将构建一个*zerotier-selftest*二进制文件，该文件对各种内部组件进行单元测试并报告构建环境的某些方面。在新的平台或架构上尝试这是个好主意。

### 运行

使用`-h`选项运行*zerotier-one*将显示帮助。

在Linux和BSD上，如果您从源代码构建，可以使用以下命令启动服务：

    sudo ./zerotier-one -d

在大多数发行版、macOS和Windows上，安装程序将启动服务并将其设置为开机启动。

系统将自动创建一个主文件夹。

服务通过JSON API进行控制，默认情况下在`127.0.0.1:9993`上可用。它还监听`0.0.0.0:9993`，但只有在`local.conf`中正确配置了`allowManagementFrom`时才能使用。我们包含了一个*zerotier-cli*命令行工具，用于进行加入和离开网络等标准API调用。主文件夹中的*authtoken.secret*文件包含访问此API的密钥令牌。有关API文档，请参见[service/README.md](service/README.md)。

以下是各操作系统上主文件夹的默认位置：

 * **Linux**: `/var/lib/zerotier-one`
 * **FreeBSD** / **OpenBSD**: `/var/db/zerotier-one`
 * **Mac**: `/Library/Application Support/ZeroTier/One`
 * **Windows**: `\ProgramData\ZeroTier\One`（这是默认位置。如果Windows安装时使用了非标准的驱动器分配或布局，基础"共享应用程序数据"文件夹可能不同。）

### 基本故障排除

对大多数用户来说，它能正常工作。

如果您正在运行本地系统防火墙，我们建议添加允许zerotier的规则。如果您安装了Windows二进制文件，这应该会自动完成。其他平台可能需要根据您的配置手动编辑本地防火墙规则。

更多信息请参见[文档网站](https://docs.zerotier.com/zerotier/troubleshooting)。

Mac防火墙可在系统偏好设置的"安全性"中找到。Linux有各种防火墙配置系统和工具。

在CentOS上检查`/etc/sysconfig/iptables`中的IPTables规则。对于其他发行版，请查阅您发行版的文档。如果运行了商业第三方防火墙应用程序（如Mac的Little Snitch、Windows的McAfee Firewall Enterprise等），您还需要检查其UI或文档。某些企业环境可能有集中管理的防火墙软件，因此您可能还需要联系IT部门。

ZeroTier One对等方将自动定位彼此并通过本地有线LAN直接通信*如果UDP端口9993入站开放*。如果该端口被过滤，它们将无法看到彼此的LAN宣告数据包。如果您在相同物理网络上的设备之间遇到性能不佳的问题，请检查它们的防火墙设置。没有LAN自动定位，对等方必须尝试"环回"NAT遍历，这有时会失败，无论如何都需要每个数据包两次遍历您的外部路由器。

位于某些类型防火墙和"对称"NAT设备后面的用户可能根本无法直接连接到外部对等方。ZeroTier对端口预测的支持有限，将*尝试*遍历对称NAT，但这并不总是有效。如果P2P连接失败，您将通过我们的中继服务器反弹UDP数据包，导致性能较慢。某些NAT路由器具有可配置的NAT模式，将其设置为"全锥型"将消除此问题。如果您这样做，您可能会看到VoIP电话、Skype、BitTorrent、WebRTC、某些游戏等的神奇改进，因为所有这些都使用与我们类似的NAT遍历技术。

如果您与互联网之间的防火墙阻止了ZeroTier的UDP流量，您将回退到通过端口443（https伪装）到根服务器的最后手段TCP隧道。这几乎可以在任何地方工作，但与UDP或直接点对点连接相比*非常慢*。

在我们的[知识库](https://zerotier.atlassian.net/wiki/spaces/SD/overview)中可以找到更多帮助。

### Prometheus指标

Prometheus指标在`/metrics` API端点可用。此端点受存储在`metricstoken.secret`中的API密钥保护，以防止不必要的信息泄露。从指标中可以获取的信息包括加入的网络和您的实例正在通信的对等方。

访问控制通过ZeroTier控制接口本身和`metricstoken.secret`实现。这可以通过承载认证令牌、`X-ZT1-Auth` HTTP头字段发送，或作为`?auth=<token>`附加到URL。您可以通过以下命令使用`cURL`查看当前指标：

    // Linux
    curl -H "X-ZT1-Auth: $(sudo cat /var/lib/zerotier-one/metricstoken.secret)" http://localhost:9993/metrics

    // macOS
    curl -H "X-ZT1-Auth: $(sudo cat /Library/Application\ Support/ZeroTier/One/metricstoken.secret)" http://localhost:9993/metrics

    // Windows PowerShell (管理员)
    Invoke-RestMethod -Headers @{'X-ZT1-Auth' = "$(Get-Content C:\ProgramData\ZeroTier\One\metricstoken.secret)"; } -Uri http://localhost:9993/metrics

要在运行ZeroTier的机器上配置Prometheus抓取作业，请将此内容添加到您的Prometheus `scrape_config`：

    - job_name: zerotier-one
      honor_labels: true
      scrape_interval: 15s
      metrics_path: /metrics
      static_configs:
      - targets:
        - 127.0.0.1:9993
        labels:
          group: zerotier-one
          node_id: $YOUR_10_CHARACTER_NODE_ID
      authorization:
        credentials: $YOUR_METRICS_TOKEN_SECRET

如果这两种方法都不理想，可能可以通过[Prometheus Proxy](https://github.com/pambrose/prometheus-proxy)或其他工具分发指标。注意：我们内部尚未测试过，但可能会通过正确的配置工作。

指标也可在ZeroTier工作目录的磁盘上获得：

   // Linux
   /var/lib/zerotier-one/metrics.prom

   // macOS
   /Library/Application\ Support/ZeroTier/One/metrics.prom

   //Windows
   C:\ProgramData\ZeroTier\One\metrics.prom

#### 可用指标

| 指标名称 | 标签 | 指标类型 | 描述 |
| ---         | ---    | ---         | ---         |
| zt_packet | packet_type, direction | Counter | ZeroTier数据包类型计数 |
| zt_packet_error | error_type, direction | Counter | ZeroTier数据包错误|
| zt_data | protocol, direction | Counter | ZeroTier传输或接收的字节数 |
| zt_num_networks | | Gauge | 此实例加入的网络数 |
| zt_network_multicast_groups_subscribed | network_id | Gauge | 网络订阅的多播组数 |
| zt_network_packets | network_id, direction | Counter | 每个网络的传入/传出数据包数 |
| zt_peer_latency | node_id | Histogram | 对等方延迟（毫秒） |
| zt_peer_path_count | node_id, status | Gauge | 到对等方的路径数 |
| zt_peer_packets | node_id, direction | Counter | 与对等方之间的数据包数 |
| zt_peer_packet_errors | node_id | Counter | 来自对等方的传入数据包错误数 |

如果您希望跟踪其他指标，请在Issue中告诉我们或向我们发送Pull Request！

### HTTP / 应用服务器

在http://localhost:9993/app/<app-path>有一个适合托管单页应用程序的静态http文件服务器

使用`zerotier-cli info -j`查找您的zerotier-one服务的homeDir

``` sh
cd $ZT_HOME
sudo mkdir -p app/app1
sudo mkdir -p app/appB
echo '<html><meta charset=utf-8><title>appA</title><body><h1>hello world A' | sudo tee app/appA/index.html 
echo '<html><meta charset=utf-8><title>app2</title><body><h1>hello world 2' | sudo tee app/app2/index.html 
curl -sL http://localhost:9993/app/appA http://localhost:9993/app/app2 
```

然后访问[http://localhost:9993/app/app1/](http://localhost:9993/app/app1/)和[http://localhost:9993/app/appB/](http://localhost:9993/app/appB/)

对不存在路径的请求会返回应用程序根目录的index.html，这在SPAs中是常见的做法。
如果您愿意，可以编写一些javascript与服务或控制器[api](https://docs.zerotier.com/service/v1)通信。