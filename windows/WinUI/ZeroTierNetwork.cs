using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Runtime.Serialization;
using System.Text;
using System.Threading.Tasks;
using Newtonsoft.Json;

namespace WinUI
{
    [Serializable]
    public class ZeroTierNetwork : ISerializable, IEquatable<ZeroTierNetwork>, IComparable<ZeroTierNetwork>, INotifyPropertyChanged
    {
        private string networkId;
        private string macAddress;
        private string networkName;
        private string networkStatus;
        private string networkType;
        private Int32 mtu;
        private bool dhcp;
        private bool bridge;
        private bool broadcastEnabled;
        private Int32 portError;
        private Int32 netconfRevision;
        private string[] assignedAddresses;
        private NetworkRoute[] routes;
        private string deviceName;
        private bool allowManaged;
        private bool allowGlobal;
        private bool allowDefault;
        private bool allowDNS;
        private bool isConnected;

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
                AllowDNS = info.GetBoolean("allowDNS");
            }
            catch { }
            IsConnected = false;
        }

        public event PropertyChangedEventHandler PropertyChanged;

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
            info.AddValue("allowDNS", AllowDNS);
        }

        public void UpdateNetwork(ZeroTierNetwork network)
        {
            if (network == null)
                return;

            if (!NetworkId.Equals(network.NetworkId))
            {
                NetworkId = network.NetworkId;
            }

            if (!MacAddress.Equals(network.MacAddress))
            {
                MacAddress = network.MacAddress;
            }

            if (!NetworkName.Equals(network.NetworkName))
            {
                NetworkName = network.NetworkName;
            }

            if (!NetworkStatus.Equals(network.NetworkStatus))
            {
                NetworkStatus = network.NetworkStatus;
            }

            if (!NetworkType.Equals(network.NetworkType))
            {
                NetworkType = network.NetworkType;
            }

            if (MTU != network.MTU)
            {
                MTU = network.MTU;
            }

            if (DHCP != network.DHCP)
            {
                DHCP = network.DHCP;
            }

            if (Bridge != network.Bridge)
            {
                Bridge = network.Bridge;
            }

            if (BroadcastEnabled != network.BroadcastEnabled)
            {
                BroadcastEnabled = network.BroadcastEnabled;
            }

            if (PortError != network.PortError)
            {
                PortError = network.PortError;
            }

            if (NetconfRevision != network.NetconfRevision)
            {
                NetconfRevision = network.NetconfRevision;
            }

            AssignedAddresses = network.AssignedAddresses;

            Routes = network.Routes;

            if (!DeviceName.Equals(network.DeviceName))
            {
                DeviceName = network.DeviceName;
            }

            if (AllowManaged != network.AllowManaged)
            {
                AllowManaged = network.AllowManaged;
            }

            if (AllowGlobal != network.AllowGlobal)
            {
                AllowGlobal = network.AllowGlobal;
            }

            if (AllowDefault != network.AllowDefault)
            {
                AllowDefault = network.AllowDefault;
            }

            if (AllowDNS != network.AllowDNS)
            {
                AllowDNS = network.AllowDNS;
            }

            if (IsConnected != network.IsConnected)
            {
                IsConnected = network.IsConnected;
            }
        }

        protected void NotifyPropertyChanged([CallerMemberName] string propertyName = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }

        [JsonProperty("nwid")]
        public string NetworkId {
            get
            {
                return networkId;
            }
            set
            {
                networkId = value;
                NotifyPropertyChanged();
            }
        }

        [JsonProperty("mac")]
        public string MacAddress
        {
            get
            {
                return macAddress;
            }
            set
            {
                macAddress = value;
                NotifyPropertyChanged();
            }
        }

        [JsonProperty("name")]
        public string NetworkName
        {
            get
            {
                return networkName;
            }
            set
            {
                networkName = value;
                NotifyPropertyChanged();
            }
        }

        [JsonProperty("status")]
        public string NetworkStatus
        {
            get
            {
                return networkStatus;
            }
            set
            {
                networkStatus = value;
                NotifyPropertyChanged();
            }

        }

        [JsonProperty("type")]
        public string NetworkType
        {
            get
            {
                return networkType;
            }
            set
            {
                networkType = value;
                NotifyPropertyChanged();
            }
        }

        [JsonProperty("mtu")]
        public int MTU
        {
            get
            {
                return mtu;
            }
            set
            {
                mtu = value;
                NotifyPropertyChanged();
            }
        }

        [JsonProperty("dhcp")]
        public bool DHCP
        {
            get
            {
                return dhcp;
            }
            set
            {
                dhcp = value;
                NotifyPropertyChanged();
            }
        }

        [JsonProperty("bridge")]
        public bool Bridge
        {
            get
            {
                return bridge;
            }
            set
            {
                bridge = value;
                NotifyPropertyChanged();
            }
        }

        [JsonProperty("broadcastEnabled")]
        public bool BroadcastEnabled
        {
            get
            {
                return broadcastEnabled;
            }
            set
            {
                broadcastEnabled = value;
                NotifyPropertyChanged();
            }
        }

        [JsonProperty("portError")]
        public int PortError
        {
            get
            {
                return portError;
            }
            set
            {
                portError = value;
                NotifyPropertyChanged();
            }
        }

        [JsonProperty("netconfRevision")]
        public int NetconfRevision
        {
            get
            {
                return netconfRevision;
            }
            set
            {
                netconfRevision = value;
                NotifyPropertyChanged();
            }
        }

        [JsonProperty("assignedAddresses")]
        public string[] AssignedAddresses
        {
            get
            {
                return assignedAddresses;
            }
            set
            {
                assignedAddresses = value;
                NotifyPropertyChanged();
            }
        }

        [JsonProperty("routes")]
        public NetworkRoute[] Routes
        {
            get
            {
                return routes;
            }
            set
            {
                routes = value;
                NotifyPropertyChanged();
            }
        }

        [JsonProperty("portDeviceName")]
        public string DeviceName
        {
            get
            {
                return deviceName;
            }
            set
            {
                deviceName = value;
                NotifyPropertyChanged();
            }
        }

        [JsonProperty("allowManaged")]
        public bool AllowManaged
        {
            get
            {
                return allowManaged;
            }
            set
            {
                allowManaged = value;
                NotifyPropertyChanged();
            }
        }

        [JsonProperty("allowGlobal")]
        public bool AllowGlobal
        {
            get
            {
                return allowGlobal;
            }
            set
            {
                allowGlobal = value;
                NotifyPropertyChanged();
            }
        }

        [JsonProperty("allowDefault")]
        public bool AllowDefault
        {
            get
            {
                return allowDefault;
            }
            set
            {
                allowDefault = value;
                NotifyPropertyChanged();
            }
        }

        [JsonProperty("allowDNS")]
        public bool AllowDNS
        {
            get
            {
                return allowDNS;
            }
            set
            {
                allowDNS = value;
                NotifyPropertyChanged();
            }
        }
        
        public bool IsConnected
        {
            get
            {
                return isConnected;
            }
            set
            {
                isConnected = value;
                NotifyPropertyChanged();
            }
        }

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

     public class NetworkEqualityComparer : IEqualityComparer<ZeroTierNetwork>
    {
        public bool Equals(ZeroTierNetwork lhs, ZeroTierNetwork rhs)
        {
            if (lhs.NetworkId.Equals(rhs.NetworkId))
            {
                lhs.UpdateNetwork(rhs);
                return true;
            }
            return false;
        }

        public int GetHashCode(ZeroTierNetwork obj)
        {
            return obj.NetworkId.GetHashCode();
        }
    }
}
