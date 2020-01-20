using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Newtonsoft.Json;

namespace WinUI
{
    public class ZeroTierStatus
    {
        [JsonProperty("address")]
        public string Address { get; set; }

        [JsonProperty("publicIdentity")]
        public string PublicIdentity { get; set; }

        [JsonProperty("online")]
        public bool Online { get; set; }

        [JsonProperty("tcpFallbackActive")]
        public bool TcpFallbackActive { get; set; }

        [JsonProperty("versionMajor")]
        public int VersionMajor { get; set; }

        [JsonProperty("versionMinor")]
        public int VersionMinor { get; set; }

        [JsonProperty("versionRev")]
        public int VersionRev { get; set; }

        [JsonProperty("version")]
        public string Version { get; set; }

        [JsonProperty("clock")]
        public UInt64 Clock { get; set; }
    }
}
