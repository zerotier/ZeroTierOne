using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Newtonsoft.Json;

namespace WinUI
{
    public class ZeroTierNetwork
    {
        [JsonProperty("nwid")]
        public string NetworkId { get; set; }

        [JsonProperty("mac")]
        public string MacAddress { get; set; }

        [JsonProperty("name")]
        public string NetworkName { get; set; }

        [JsonProperty("status")]
        public string NetworkStatus { get; set; }

        [JsonProperty("type")]
        public string NetworkType { get; set; }

        [JsonProperty("mtu")]
        public int MTU { get; set; }

        [JsonProperty("dhcp")]
        public bool DHCP { get; set; }

        [JsonProperty("bridge")]
        public bool Bridge { get; set ; }

        [JsonProperty("broadcastEnabled")]
        public bool BroadcastEnabled { get ; set; }

        [JsonProperty("portError")]
        public int PortError { get; set; }

        [JsonProperty("netconfRevision")]
        public int NetconfRevision { get; set; }

        [JsonProperty("multicastSubscriptions")]
        public string[] MulticastSubscriptions { get; set; }

        [JsonProperty("assignedAddresses")]
        public string[] AssignedAddresses { get; set; }

        [JsonProperty("portDeviceName")]
        public string DeviceName { get; set; }
    }
}
