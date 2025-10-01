ZeroTier One 网络虚拟化服务
======

这是 ZeroTier One 的实际实现，该服务为桌面电脑、笔记本电脑、服务器、虚拟机等提供 ZeroTier 虚拟网络连接（iOS 和 Android 的移动版本使用原生 Java 和 Objective C 实现，仅利用 ZeroTier 核心引擎。）

### 本地配置文件

ZeroTier [主目录](https://github.com/zerotier/ZeroTierOne/blob/6faca86bb424d0b9643b6efa50571f73310d8276/README.md)中名为 `local.conf` 的文件包含适用于本地节点的配置选项。（除非您创建它，否则它不存在）。它可以用于设置可信路径、将物理路径列入黑名单、为某些节点设置物理路径提示以及定义可信的上游设备（联合根）。在大型部署中，可以使用 Puppet、Chef、SaltStack 等工具来在系统间设置统一配置。

这是一个 JSON 格式的文件，ZeroTier One 本身也可以编辑和重写，因此请确保使用正确的 JSON 格式。要验证您的配置，请将其粘贴到 [jsonlint.com](https://jsonlint.com) 等网站，或使用 `jq` 等工具。

检查 `zerotier-cli info -j` 的输出以查看您的配置是否正在加载。

`local.conf` 中可用的设置（这不是有效的 JSON，JSON 不允许注释）：

```javascript
{
	"physical": { /* 应用于物理 L2/L3 网络路径的设置。 */
		"NETWORK/bits": { /* 网络，例如 10.0.0.0/24 或 fd00::/32 */
			"blacklist": true|false, /* 如果为 true，则将此路径列入黑名单，禁止所有 ZeroTier 流量 */
			"trustedPathId": 0|!0, /* 如果存在且非零，则将此定义为可信路径（见下文） */
			"mtu": 0|!0 /* 如果存在且非零，则设置此路径的 UDP 最大载荷 MTU */
		} /* ,... 其他网络 */
	},
	"virtual": { /* 应用于 ZeroTier 虚拟网络设备 (VL1) 的设置 */
		"##########": { /* 10 位 ZeroTier 地址 */
			"try": [ "IP/port"/*,...*/ ], /* 如果没有上游/根在线，则提示如何到达此对等方 */
			"blacklist": [ "NETWORK/bits"/*,...*/ ] /* 仅为该对等方将物理路径列入黑名单。 */
		}
	},
	"settings": { /* 其他全局设置 */
		"primaryPort": 1-65535, /* 如果设置，则覆盖默认端口 9993 和任何命令行端口 */
		"secondaryPort": 1-65535, /* 如果设置，则覆盖默认随机辅助端口 */
		"tertiaryPort": 1-65535, /* 如果设置，则覆盖默认随机第三端口 */
		"portMappingEnabled": true|false, /* 如果为 true（默认），则尝试使用 uPnP 或 NAT-PMP 映射端口 */
		"allowSecondaryPort": true|false /* false 也将禁用辅助端口 */
		"softwareUpdate": "apply"|"download"|"disable", /* 自动应用更新、仅下载或禁用内置软件更新 */
		"softwareUpdateChannel": "release"|"beta", /* 软件更新通道 */
		"softwareUpdateDist": true|false, /* 如果为 true，则分发软件更新（仅对 ZeroTier, Inc. 本身有用，默认为 false） */
		"interfacePrefixBlacklist": [ "XXX",... ], /* 要为 ZT 流量列入黑名单的接口名称前缀数组（例如 eth 表示 eth#） */
		"allowManagementFrom": [ "NETWORK/bits", ...] |null, /* 如果非 NULL，则允许从此 IP 网络进行 JSON/HTTP 管理。默认仅为 127.0.0.1。 */
		"bind": [ "ip",... ], /* 如果存在且非 null，则绑定到这些 IP 而不是每个接口（允许通配符 IP） */
		"allowTcpFallbackRelay": true|false, /* 允许或禁止建立 TCP 中继连接（默认为 true） */
		"multipathMode": 0|1|2 /* 多路径模式：无 (0)、随机 (1)、比例 (2) */
	}
}
```

 * **trustedPathId**：可信路径是不需要加密和认证的物理网络。这提供了性能提升，但在通过此路径通信时会牺牲 ZeroTier 的所有安全功能。仅在您知道自己在做什么并且确实需要性能时才使用！要设置可信路径，使用它的所有设备必须为同一网络具有*相同的可信路径 ID*。可信路径 ID 是任意的正非零整数。例如，IP 在 10.0.0.0/24 范围内的一组设备可以将其用作快速可信路径，如果它们都为该网络定义了相同的 "25" 可信路径 ID。

一个 `local.conf` 示例：

```javascript
{
	"physical": {
		"10.0.0.0/24": {
			"blacklist": true
		},
		"10.10.10.0/24": {
			"trustedPathId": 101010024
		},
	},
	"virtual": {
		"feedbeef12": {
			"role": "UPSTREAM",
			"try": [ "10.10.20.1/9993" ],
			"blacklist": [ "192.168.0.0/24" ]
		}
	},
	"settings": {
		"softwareUpdate": "apply",
		"softwareUpdateChannel": "release"
	}
}
```

### 网络虚拟化服务 API

JSON API 支持 GET、POST/PUT 和 DELETE。PUT 被视为 POST 的同义词。不支持其他方法，包括 HEAD。

POST 到 JSON API 的值对类型*极其*敏感。内容*必须*是指示的类型，否则将被忽略或产生错误。任何用引号括起来的内容都是字符串，因此布尔值和整数必须没有引号。布尔值必须是 *true* 或 *false*，不能是其他值。整数不能包含小数点，否则它们是浮点数（反之亦然）。如果某些内容似乎被忽略或设置为奇怪的值，或者您收到错误，请根据下面列出的类型检查您提交的所有 JSON 字段的类型。JSON 对象中无法识别的字段也会被忽略。

API 请求必须通过认证令牌进行认证。ZeroTier One 将此令牌保存在其工作目录中的 *authtoken.secret* 文件中。此令牌可以通过 *auth* URL 参数（例如 '?auth=...'）或 *X-ZT1-Auth* HTTP 请求头提供。静态 UI 页面是服务器允许的唯一无需认证的内容。

可以提供 *jsonp* URL 参数以请求 JSONP 封装。JSONP 响应作为脚本发送，其 JSON 响应载荷包装在以 *jsonp* 参数值作为函数名的调用中。

#### /status

 * 目的：获取运行节点状态和地址信息
 * 方法：GET
 * 返回：{ 对象 }

| 字段                  | 类型          | 描述                                          | 可写 |
| --------------------- | ------------- | --------------------------------------------- | ---- |
| address               | 字符串        | 此节点的 10 位十六进制 ZeroTier 地址          | 否   |
| publicIdentity        | 字符串        | 此节点的 ZeroTier identity.public             | 否   |
| worldId               | 整数          | ZeroTier 世界 ID（除测试外从不更改）          | 否   |
| worldTimestamp        | 整数          | 最新世界定义的时间戳                          | 否   |
| online                | 布尔值        | 如果为 true，则至少有一个上游对等方可到达     | 否   |
| tcpFallbackActive     | 布尔值        | 如果为 true，则我们正在使用慢速 TCP 回退      | 否   |
| relayPolicy           | 字符串        | 中继策略：ALWAYS、TRUSTED 或 NEVER            | 否   |
| versionMajor          | 整数          | 软件主版本号                                  | 否   |
| versionMinor          | 整数          | 软件次版本号                                  | 否   |
| versionRev            | 整数          | 软件修订版本号                                | 否   |
| version               | 字符串        | 主版本.次版本.修订版本                        | 否   |
| clock                 | 整数          | 节点处的当前系统时钟（自纪元以来的毫秒数）    | 否   |

#### /network

 * 目的：获取所有网络成员资格
 * 方法：GET
 * 返回：[ {对象}, ... ]

获取 /network 返回此节点已加入的所有网络的数组。请参见下文了解网络对象格式。

#### /network/\<网络 ID\>

 * 目的：获取、加入或离开网络
 * 方法：GET、POST、DELETE
 * 返回：{ 对象 }

要加入网络，请 POST 到它。由于网络没有强制性的可写参数，POST 数据是可选的，可以省略。例如：POST 到 /network/8056c2e21c000001 以加入公共 "Earth" 网络。要离开网络，请 DELETE 它，例如 DELETE /network/8056c2e21c000001。

大多数网络设置不可写，因为它们由网络控制器定义。

| 字段                  | 类型          | 描述                                          | 可写 |
| --------------------- | ------------- | --------------------------------------------- | ---- |
| id                    | 字符串        | 16 位十六进制网络 ID                          | 否   |
| nwid                  | 字符串        | 16 位十六进制网络 ID（遗留字段）              | 否   |
| mac                   | 字符串        | 此网络的网络设备的 MAC 地址                   | 否   |
| name                  | 字符串        | 此网络的简称（来自控制器）                    | 否   |
| status                | 字符串        | 网络状态（OK、ACCESS_DENIED 等）              | 否   |
| type                  | 字符串        | 网络类型（PUBLIC 或 PRIVATE）                 | 否   |
| mtu                   | 整数          | 以太网 MTU                                    | 否   |
| dhcp                  | 布尔值        | 如果为 true，则应使用 DHCP 获取 IP 信息       | 否   |
| bridge                | 布尔值        | 如果为 true，则此设备可以桥接其他设备         | 否   |
| broadcastEnabled      | 布尔值        | 如果为 true，则 ff:ff:ff:ff:ff:ff 广播工作    | 否   |
| portError             | 整数          | 底层 tap 驱动程序返回的错误代码               | 否   |
| netconfRevision       | 整数          | 网络配置修订 ID                               | 否   |
| assignedAddresses     | [字符串]      | ZeroTier 分配的 IP 地址数组 (/bits)           | 否   |
| routes                | [对象]        | ZeroTier 分配的路由数组（见下文）             | 否   |
| portDeviceName        | 字符串        | 虚拟网络设备的名称（如果有）                  | 否   |
| allowManaged          | 布尔值        | 允许 IP 和路由管理                            | 是   |
| allowGlobal           | 布尔值        | 允许与全局 IP 重叠的 IP 和路由                | 是   |
| allowDefault          | 布尔值        | 允许覆盖系统默认路由                          | 是   |
| allowDNS              | 布尔值        | 允许在网络中配置 DNS                          | 是   |

路由对象：

| 字段                  | 类型          | 描述                                          | 可写 |
| --------------------- | ------------- | --------------------------------------------- | ---- |
| target                | 字符串        | 目标网络 / 网络掩码位数                       | 否   |
| via                   | 字符串        | 网关 IP 地址（下一跳）或 null 表示 LAN        | 否   |
| flags                 | 整数          | 标志，目前始终为 0                            | 否   |
| metric                | 整数          | 路由度量（目前未使用）                        | 否   |

#### /peer

 * 目的：获取所有对等方
 * 方法：GET
 * 返回：[ {对象}, ... ]

获取 /peer 返回所有当前对等方的对等方对象数组。请参见下文了解对等方对象格式。

#### /peer/\<地址\>

 * 目的：获取或设置对等方信息
 * 方法：GET、POST
 * 返回：{ 对象 }

| 字段                  | 类型          | 描述                                          | 可写 |
| --------------------- | ------------- | --------------------------------------------- | ---- |
| address               | 字符串        | 对等方的 10 位十六进制 ZeroTier 地址          | 否   |
| versionMajor          | 整数          | 远程主版本号（如果已知）                      | 否   |
| versionMinor          | 整数          | 远程次版本号（如果已知）                      | 否   |
| versionRev            | 整数          | 远程软件修订版本号（如果已知）                | 否   |
| version               | 字符串        | 主版本.次版本.修订版本                        | 否   |
| latency               | 整数          | 延迟（毫秒），如果已知                        | 否   |
| role                  | 字符串        | LEAF、UPSTREAM、ROOT 或 PLANET                | 否   |
| paths                 | [对象]        | 当前活动的物理路径（见下文）                  | 否   |

路径对象：

| 字段                  | 类型          | 描述                                          | 可写 |
| --------------------- | ------------- | --------------------------------------------- | ---- |
| address               | 字符串        | 物理套接字地址，例如 IP/端口                  | 否   |
| lastSend              | 整数          | 通过此路径的最后一次发送时间                  | 否   |
| lastReceive           | 整数          | 通过此路径的最后一次接收时间                  | 否   |
| active                | 布尔值        | 此路径是否正在使用？                          | 否   |
| expired               | 布尔值        | 此路径是否已过期？                            | 否   |
| preferred             | 布尔值        | 这是否是当前首选路径？                        | 否   |
| trustedPathId         | 整数          | 如果非零，则这是可信路径（未加密）            | 否   |