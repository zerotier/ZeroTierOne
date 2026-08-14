# Linux TAP multiqueue receive path

On Linux, enabling `multicoreEnabled` creates multiple TAP receive workers.
Each worker needs its own TAP queue to preserve packet ordering within a flow.

## Why separate queues are required

Having several workers read a single `/dev/net/tun` descriptor lets different
threads dequeue adjacent packets from the same FIFO. Their processing can then
complete in a different order, which is particularly visible with sustained,
high-rate UDP traffic.

When the configured concurrency is greater than one, ZeroTier creates the TAP
interface with `IFF_MULTI_QUEUE` and attaches one descriptor per worker. Linux
assigns a flow to a queue, so each flow has one reader while unrelated flows
can still be processed in parallel. Single-worker operation continues to use a
normal, single-queue TAP interface.

## Configuration

The existing local configuration controls this behavior; no new setting is
introduced:

```json
{
  "settings": {
    "multicoreEnabled": true,
    "concurrency": 2,
    "cpuPinningEnabled": false
  }
}
```

At startup, a multiqueue interface reports the number of configured queues:

```text
Configured 2 Linux TAP queues for ztabcdefgh
```

If an additional queue cannot be attached, interface creation fails instead
of silently falling back to multiple readers on the shared descriptor.
