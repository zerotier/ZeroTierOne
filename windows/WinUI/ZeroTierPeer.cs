using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Newtonsoft.Json;

namespace WinUI
{
    public class ZeroTierPeer : IEquatable<ZeroTierPeer>
    {
        [JsonProperty("address")]
        public string Address { get; set; }

        private Int64 _lastUnicast;
        [JsonProperty("lastUnicastFrame")]
        public Int64 LastUnicastFrame
        {
            get
            {
                if (_lastUnicast == 0)
                    return 0;

                TimeSpan t = DateTime.UtcNow - new DateTime(1970, 1, 1);
                Int64 millisecondsSinceEpoch = (Int64)t.TotalMilliseconds;
                return (millisecondsSinceEpoch - _lastUnicast) / 1000;
            }
            set
            {
                _lastUnicast = value;
            }
        }

        private Int64 _lastMulticast;
        [JsonProperty("lastMulticastFrame")]
        public Int64 LastMulticastFrame 
        {
            get
            {
                if (_lastMulticast == 0)
                    return 0;

                TimeSpan t = DateTime.UtcNow - new DateTime(1970, 1, 1);
                Int64 millisecondsSinceEpoch = (Int64)t.TotalMilliseconds;
                return (millisecondsSinceEpoch - _lastMulticast) / 1000;
            }
            set
            {
                _lastMulticast = value;
            }
        }

        [JsonProperty("versionMajor")]
        public int VersionMajor { get; set; }

        [JsonProperty("versionMinor")]
        public int VersionMinor { get; set; }

        [JsonProperty("versionRev")]
        public int VersionRev { get; set; }

        [JsonProperty("version")]
        public string Version { get; set; }

        public string VersionString
        {
            get
            {
                if (Version == "-1.-1.-1")
                    return "-";
                else
                    return Version;
            }
        }

        [JsonProperty("latency")]
        public string Latency { get; set; }

        [JsonProperty("role")]
        public string Role { get; set; }

        [JsonProperty("paths")]
        public List<ZeroTierPeerPhysicalPath> Paths { get; set; }

        public string DataPaths
        {
            get
            {
                string pathStr = "";
                foreach(ZeroTierPeerPhysicalPath path in Paths)
                {
                    pathStr += path.Address + "\n";
                }
                return pathStr;
            }
        }

        public bool Equals(ZeroTierPeer other)
        {
            return this.Address.Equals(other.Address, StringComparison.InvariantCultureIgnoreCase);
        }

        public void Update(ZeroTierPeer other)
        {
            _lastUnicast = other._lastUnicast;
            _lastMulticast = other._lastMulticast;
            VersionMajor = other.VersionMajor;
            VersionMinor = other.VersionMinor;
            VersionRev = other.VersionRev;
            Version = other.Version;
            Latency = other.Latency;
            Role = other.Role;
            Paths = other.Paths;
        }
    }
}
