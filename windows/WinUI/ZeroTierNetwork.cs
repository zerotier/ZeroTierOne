using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.Serialization;
using System.Text;
using System.Threading.Tasks;
using Newtonsoft.Json;

namespace WinUI
{
    [Serializable]
    public class ZeroTierNetwork : ISerializable, IEquatable<ZeroTierNetwork>, IComparable<ZeroTierNetwork>
    {
        protected ZeroTierNetwork(SerializationInfo info, StreamingContext ctx)
        {
            NetworkId = info.GetString("NetworkId");
            MacAddress = info.GetString("MacAddress");
            NetworkName = info.GetString("NetworkName");
            NetworkStatus = info.GetString("NetworkStatus");
            NetworkType = info.GetString("NetworkType");
            MTU = info.GetInt32("MTU");
            DHCP = info.GetBoolean("DHCP");
            Bridge = info.GetBoolean("Bridge");
            BroadcastEnabled = info.GetBoolean("BroadcastEnabled");
            PortError = info.GetInt32("PortError");
            NetconfRevision = info.GetInt32("NetconfRevision");
            AssignedAddresses = (string[])info.GetValue("AssignedAddresses", typeof(string[]));
            Routes = (NetworkRoute[])info.GetValue("Routes", typeof(NetworkRoute[]));
            DeviceName = info.GetString("DeviceName");
            AllowManaged = info.GetBoolean("AllowManaged");
            AllowGlobal = info.GetBoolean("AllowGlobal");
            AllowDefault = info.GetBoolean("AllowDefault");
            IsConnected = false;
        }

        public virtual void GetObjectData(SerializationInfo info, StreamingContext ctx)
        {
            info.AddValue("NetworkId", NetworkId);
            info.AddValue("MacAddress", MacAddress);
            info.AddValue("NetworkName", NetworkName);
            info.AddValue("NetworkStatus", NetworkStatus);
            info.AddValue("NetworkType", NetworkType);
            info.AddValue("MTU", MTU);
            info.AddValue("DHCP", DHCP);
            info.AddValue("Bridge", Bridge);
            info.AddValue("BroadcastEnabled", BroadcastEnabled);
            info.AddValue("PortError", PortError);
            info.AddValue("NetconfRevision", NetconfRevision);
            info.AddValue("AssignedAddresses", AssignedAddresses);
            info.AddValue("Routes", Routes);
            info.AddValue("DeviceName", DeviceName);
            info.AddValue("AllowManaged", AllowManaged);
            info.AddValue("AllowGlobal", AllowGlobal);
            info.AddValue("AllowDefault", AllowDefault);
        }


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

        [JsonProperty("assignedAddresses")]
        public string[] AssignedAddresses { get; set; }

        [JsonProperty("routes")]
        public NetworkRoute[] Routes { get; set; }

        [JsonProperty("portDeviceName")]
        public string DeviceName { get; set; }

        [JsonProperty("allowManaged")]
        public bool AllowManaged { get; set; }

        [JsonProperty("allowGlobal")]
        public bool AllowGlobal { get; set; }

        [JsonProperty("allowDefault")]
        public bool AllowDefault { get; set; }
        
        public bool IsConnected { get; set; } = false;

        public String Title
        {
            get
            {

                if (NetworkName != null && NetworkName.Length > 0)
                {
                    return NetworkId + " (" + NetworkName + ")";
                }
                else
                {
                    return NetworkId;
                }
            }
        }

        public bool Equals(ZeroTierNetwork network)
        {
            return NetworkId.Equals(network.NetworkId);
        }
        
        public int CompareTo(ZeroTierNetwork network)
        {
            UInt64 thisNwid = UInt64.Parse(NetworkId, System.Globalization.NumberStyles.HexNumber);
            UInt64 otherNwid = UInt64.Parse(network.NetworkId, System.Globalization.NumberStyles.HexNumber);

            if (thisNwid > otherNwid)
            {
                return 1;
            }
            else if (thisNwid < otherNwid)
            {
                return -1;
            }
            else
            {
                return 0;
            }
        }
    }
}
