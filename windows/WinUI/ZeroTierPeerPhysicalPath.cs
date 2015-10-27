using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Newtonsoft.Json;

namespace WinUI
{
    public class ZeroTierPeerPhysicalPath
    {
        [JsonProperty("address")]
        public string Address { get; set; }

        [JsonProperty("lastSend")]
        public UInt64 LastSend { get; set; }

        [JsonProperty("lastReceive")]
        public UInt64 LastReceive { get; set; }

        [JsonProperty("fixed")]
        public bool Fixed { get; set; }

        [JsonProperty("preferred")]
        public bool Preferred { get; set; }
    }
}
