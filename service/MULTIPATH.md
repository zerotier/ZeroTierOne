### **2.1.5.** Link aggregation

Link aggregation allows the simultaneous (or conditional) use of multiple physical links to enable increased throughput, load balancing, redundancy, and fault tolerance. There are a variety of standard policies available that can be used right out of the box with little to no configuration. These policies are directly inspired by [the policies offered by the Linux kernel](https://www.kernel.org/doc/Documentation/networking/bonding.txt).

#### Standard Policies

| Policy name        | Fault tolerance       | Min. failover (sec.)    | Default Failover (sec.) | Balancing             | Aggregation efficiency | Redundancy | Sequence Reordering |
|--------------------|:---------------------:|---------------------:|---------------------:|----------------------:|-----------------------:|-----------:|--------------------:|
| `none`             | None                  | `60+`                | `60+`                | none                  | `none`                 |1           | No
| `active-backup`    | Brief interruption    | `0.25`  | `10`                  | none                  | `low`                  |1           | Only during failover
| `broadcast`        | Fully tolerant                | `N/A`                  | `N/A`                  | none                  | `very low`             |N           | Often
| `balance-rr`       | Self-healing | `0.25`   | `10`                | packet-based          | `high`                 |1           | Often
| `balance-xor`      | Self-healing | `0.25`   | `10`                | flow-based            | `very high`            |1           | Only during failover
| `balance-aware`    | Self-healing | `0.25`   | `10`                | *adaptive* flow-based | `very high`            |1           | Only during failover and re-balance

A policy can be used easily without specifying any additional parameters:

```
{
    "settings": {
        "defaultBondingPolicy": "active-backup"
    }
}
```

#### Custom Policies

To customize a bonding policy for your use-case simply specify a `basePolicy` and override chosen parameters. For example, to create a more aggressive `active-backup` policy with low monitoring overhead that will failover `0.250` seconds after it detects a link failure, one could do the following: 

```
{
	"settings":
  {
		"defaultBondingPolicy": "aggressive-active-backup",
		"policies":
    {
      "aggressive-active-backup":
      {
				"failoverInterval": 250,
				"pathMonitorStrategy": "dynamic",
				"basePolicy": "active-backup"
			}
    }
  }
}
```

#### Specifying Slave interfaces

Available system network interfaces are referred to as `slaves`. Different sets of slaves can be constructed for different bonding policies and used simultaneously. One can specify the links that ZeroTier should use in any given bonding policy simply by providing an array of slaves with names corresponding to interface names. If a user doesn't specify a set of interfaces to use, ZeroTier will assume every system interface is available for use. However, if the user **does** specify a set of interfaces, ZeroTier will only use what is specified. The same applies to failover rules, if none are specified, ZeroTier will failover to any operational slave. On the other hand, if the user does specify failover rules and there is ever a situation where a slave is available for usage but does not fit within the rules specified by the user, it will go unused.

To specify that ZeroTier should only use `eth0` and `eth1` as primary slaves, and `eth2` as a backup spare and that it should prefer IPv4 over IPv6 except on `eth2` where only IPv6 is allowed:

```
{
	"settings": {
		"defaultBondingPolicy": "aggressive-active-backup",
		"policies": {
			"aggressive-active-backup": {
				"slaves": {
					"eth0": {
						"ipvPref": 46,
						"failoverTo": "eth2",
						"mode": "primary"
					},
					"eth1": {
						"ipvPref": 46,
						"failoverTo": "eth2",
						"mode": "primary"
					},
					"eth2": {
						"ipvPref": 6,
						"mode": "spare"
					}
				}
			}
		}
	}
}
```

Additional slave-specific parameters:

```
"slaves":
{
  "interfaceName": /* System-name of the network interface. */
  {
    "failoverInterval": 0-65535, /* (optional) How quickly a path on this slave should failover after a detected failure. */
    "ipvPref": [0,4,6,46,64], /* (optional) IP version preference for detected paths on a slave. */
    "speed": 0-1000000, /* (optional) How fast this slave is (in arbitrary units). This is a useful way to manually allocate a bond. */
    "alloc": 0-255, /* (optional) A relative value representing a desired allocation. */
    "upDelay": 0-65535, /* (optional) How long after a path becomes alive before it is added to the bond. */
    "downDelay": 0-65535, /* (optional) How long after a path fails before it is removed from the bond. */
    "failoverTo": "spareInterfaceName", /* (optional) Which slave should be used next after a failure of this slave. */
    "enabled": true|false, /* (optional) Whether any paths on this slave are allowed to be used this bond. */
    "mode": "primary"|"spare" /* (optional) Whether this slave is used by default or only after failover events. */
  }
}
```

#### Peer-specific Bonds

It is possible to direct ZeroTier to form a certain type of bond with specific peers of your choice. For instance, if one were to want `active-backup` by default but for certain peers to be bonded with a custom load-balanced bond such as `my-custom-balance-aware` one could do the following:

```
{
	"settings":
  {
		"defaultBondingPolicy": "active-backup",
		"policies":
		{
      "my-custom-balance-aware":
      {
				"failoverInterval": 2000,
        "monitorStrategy": "dynamic",
				"basePolicy": "balance-aware"
			}
    },
    "peerSpecificBonds":
		{
			"f6203a2db3":"my-custom-balance-aware",
			"45b0301da2":"my-custom-balance-aware",
			"a92cb526fa":"my-custom-balance-aware"
		}
  }
}
```

#### Active Backup (`active-backup`)

Traffic is sent only on (one) path at any given time. A different path becomes active if the current path fails. This mode provides fault tolerance with a nearly immediate fail-over. This mode **does not** increase total throughput.

 - `mode`: `primary, spare` Slave option which specifies which slave is the primary device. The specified device is intended to always be the active slave while it is available. There are exceptions to this behavior when using different `slaveSelectMethod` modes. There can only be one `primary` slave in this bonding policy.

 - `slaveSelectMethod`: Specifies the selection policy for the active slave during failure and/or recovery events. This is similar to the Linux Kernel's `primary_reselect` option but with a minor extension:
     - `optimize`: **(default if user provides no failover guidance)** The primary slave can change periodically if a superior path is detected.
     - `always`: **(default when slaves are explicitly specified)**: Primary slave regains status as active slave whenever it comes back up.
     - `better`: Primary slave regains status as active slave when it comes back up and (if) it is better than the currently-active slave.
     - `failure`: Primary slave regains status as active slave only if the currently-active slave fails.

```
{
  "settings":
  {
    "defaultBondingPolicy": "active-backup",
    "active-backup":
    {
      "slaveSelectMethod": "always",
      "slaves":
      {
        "eth0": { "failoverTo": "eth1", "mode": "primary" },
        "eth1": { "mode": "spare" },
        "eth2": { "mode": "spare" },
        "eth3": { "mode": "spare" }
      }
    }
  }
}
```

#### Broadcast (`broadcast`)

Traffic is sent on (all) available paths simultaneously. This mode provides fault tolerance and effectively immediate failover due to transmission redundancy. This mode is a poor utilization of throughput resources and will **not** increase throughput but can prevent packet loss during a link failure. The only option available is `dedup` which will de-duplicate all packets on the receiving end if set to `true`.

#### Balance Round Robin (`balance-rr`)

Traffic is striped across multiple paths. Offers partial fault tolerance immediately, full fault tolerance eventually. This policy is unaware of protocols and is primarily intended for use with protocols that are not sensitive to reordering delays. The only option available for this policy is `packetsPerSlave` which specifies the number of packets to transmit via a path before moving to the next in the RR sequence. When set to `0` a path is chosen at random for each outgoing packet. The default value is `8`, low values can begin to add overhead to packet processing.

#### Balance XOR (`balance-xor`, similar to the Linux kernel's [balance-xor](https://www.kernel.org/doc/Documentation/networking/bonding.txt) with `xmit_hash_policy=layer3+4`)

Traffic is categorized into *flows* based on *source port*, *destination port*, and *protocol type* these flows are then hashed onto available slaves. Each flow will persist on its assigned slave interface for its entire life-cycle. Traffic that does not have an assigned port (such as ICMP pings) will be randomly distributed across slaves. The hash function is simply: `src_port ^ dst_port ^ proto`.

#### Balance Aware (`balance-aware`, similar to Linux kernel's [`balance-*lb`](https://www.kernel.org/doc/Documentation/networking/bonding.txt) modes)

Traffic is dynamically allocated and balanced across multiple slaves simultaneously according to the target allocation. Options allow for *packet* or *flow-based* processing, and active-flow reassignment. Flows mediated over a recently failed slaves will be reassigned in a manner that respects the target allocation of the bond. An optional `balancePolicy` can be specified with the following effects: `flow-dynamic` (default) will hash flows onto slaves according to target allocation and may perform periodic re-assignments in order to preserve balance. `flow-static`, will hash flows onto slaves according to target allocation but will not re-assign flows unless a failure occurs or the slave is no longer operating within acceptable parameters. And lastly `packet` which simply load balances packets across slaves according to target allocation but with no concern for sequence reordering.

```
{
    "settings":
    {
        "defaultBondingPolicy": "balance-aware",
        "balance-aware": {
            "balancePolicy": "flow-dynamic"|"flow-static"|"packet"
        }
    }
}
```

#### Link Quality

ZeroTier measures various properties of a link (such as latency, throughput, jitter, packet loss ratio, etc) in order to arrive at a quality estimate. This estimate is used by bonding policies to make allocation and failover decisions:

| Policy name    | Role |
|:---------------|:-----|
|`active-backup` | Determines the order of the failover queue. And if `activeReselect=optimize` whether a new active slave is selected. |
|`broadcast`     | Does not use quality measurements. |
|`balance-rr`    | May trigger removal of slave from bond. |
|`balance-xor`   | May trigger removal of slave from bond. |
|`balance-aware` | Informs flow assignments and (re-)assignments. May trigger removal of slave from bond. |

A slave's eligibility for being included in a bond is dependent on more than perceived quality. If a path on a slave begins to exhibit disruptive behavior such as extremely high packet loss, corruption, or periodic inability to process traffic it will be removed from the bond, its traffic will be appropriately reallocated and it will be punished. Punishments gradually fade and a slave can be readmitted to the bond over time. However, punishments increase exponentially if applied more than once within a given window of time.

#### Asymmetric Links

In cases where it is necessary to bond physical links that vary radically in terms of cost, throughput, latency, and or reliability, there are a couple of ways to automatically (or manually) allocate traffic among them. Traffic distribution and balancing can be either `packet` or `flow` based. Where packet-based is suitable for protocols not susceptible to reordering penalties and flow-based is suitable for protocols such as TCP where it is desirable to keep a conversation on a single link unless we can't avoid having to re-assign it. Additionally, a *target allocation* of traffic used by the bonding policy can be derived/specified in the following ways:

 - **Automatically**: This is the easiest and requires no user configuration. The bonding layer measures and senses the link properties and determines a target allocation based on perceived quality and capacity. Weaker, less reliable links will have less traffic allocated to them and stronger, more reliable links will have more traffic allocated to them. Optionally, the user can specify a set of weights (totaling `1.0`) to inform the bonding layer how important certain link properties are. For instance, one may primarily be concerned with latency and jitter but not total throughput:

```
"balance-aware": {
    "quality": {
        "lat": 0.3, /* Moving average of latency in milliseconds */
        "ltm": 0.2, /* Maximum observed latency in milliseconds */
        "pdv": 0.3, /* Packet delay variance in milliseconds. Similar to jitter */
        "plr": 0.1, /* Packet loss ratio */
        "per": 0.1, /* Packet error ratio */
        "thr": 0.0, /* Mean throughput */
        "thm": 0.0, /* Maximum observed throughput */
        "thv": 0.0, /* Variance of throughput */
        "avl": 0.0, /* Availability */
    }
}
```
In the absence of user guidance ZeroTier will attempt to form an understanding of each link's speed and capacity but this value can be inaccurate if the links are not routinely saturated. Therefore we provide a way to explicitly signal the capacity of each link in terms of arbitrary but relative values:

```
"slaves": {
  "eth0": { "speed": 10000 },
  "eth1": { "speed": 1000 },
  "eth2": { "speed": 100 }
}
```

The user specifies allocation percentages (totaling `1.0`). In this case quality measurements will only be used to determine a slave's eligibility to be a member of a bond, now how much traffic it will carry:

```
"slaves": {
  "eth0": { "alloc": 0.50 },
  "eth1": { "alloc": 0.25 },
  "eth2": { "alloc": 0.25 }
}
```

#### Performance and Overhead Considerations

  - Only packets with internal IDs divisible by `16` are included in measurements, this amounts to about `6.25%` of all traffic. 
  - `failoverInterval` specifies how quickly failover should occur during a link failure. In order to accomplish this a combination of active and passive measurement techniques are employed which may result in `VERB_HELLO` probes being sent every `failoverInterval / 4` time units. As a mitigation `monitorStrategy` may be set to `dynamic` so that probe frequency directly correlates with native application traffic.


