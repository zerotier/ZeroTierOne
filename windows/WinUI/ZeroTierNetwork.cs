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
            try
            {
                NetworkId = info.GetString("nwid");
                MacAddress = info.GetString("mac");
                NetworkName = info.GetString("name");
                NetworkStatus = info.GetString("status");
                NetworkType = info.GetString("type");
                MTU = info.GetInt32("mtu");
                DHCP = info.GetBoolean("dhcp");
                Bridge = info.GetBoolean("bridge");
                BroadcastEnabled = info.GetBoolean("broadcastEnabled");
                PortError = info.GetInt32("portError");
                NetconfRevision = info.GetInt32("netconfRevision");
                AssignedAddresses = (string[])info.GetValue("assignedAddresses", typeof(string[]));
                Routes = (NetworkRoute[])info.GetValue("routes", typeof(NetworkRoute[]));
                DeviceName = info.GetString("portDeviceName");
                AllowManaged = info.GetBoolean("allowManaged");
                AllowGlobal = info.GetBoolean("allowGlobal");
                AllowDefault = info.GetBoolean("allowDefault");
            }
            catch { }
            IsConnected = false;
        }

        public virtual void GetObjectData(SerializationInfo info, StreamingContext ctx)
        {
            info.AddValue("nwid", NetworkId);
            info.AddValue("mac", MacAddress);
            info.AddValue("name", NetworkName);
            info.AddValue("status", NetworkStatus);
            info.AddValue("type", NetworkType);
            info.AddValue("mtu", MTU);
            info.AddValue("dhcp", DHCP);
            info.AddValue("bridge", Bridge);
            info.AddValue("broadcastEnabled", BroadcastEnabled);
            info.AddValue("portError", PortError);
            info.AddValue("netconfRevision", NetconfRevision);
            info.AddValue("assignedAddresses", AssignedAddresses);
            info.AddValue("routes", Routes);
            info.AddValue("portDeviceName", DeviceName);
            info.AddValue("allowManaged", AllowManaged);
            info.AddValue("allowGlobal", AllowGlobal);
            info.AddValue("allowDefault", AllowDefault);
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
            if (NetworkId == null || network == null)
                return false;

            return NetworkId.Equals(network.NetworkId);
        }
        
        public int CompareTo(ZeroTierNetwork network)
        {
            if (NetworkId == null || network == null)
                return -1;

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
