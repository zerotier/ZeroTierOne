using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Newtonsoft.Json;

namespace WinUI
{
    class CentralMemberVM
    {
        
        public string Name { get; set; }
        public string Value { get; set; }
        public CentralMemberVM(string name, string value)
        {
            Name = name;
            Value = value;
        }
        public static void Populate(CentralMember m)
        {
            if (m.Properties == null)
                m.Properties = new List<CentralMemberVM>();
            else
                m.Properties.Clear();

            Add(m, "NodeID", m.NodeId);
            if(!string.IsNullOrEmpty(m.Description))
                Add(m, "Description", m.Description);
            if(!m.Online)
                Add(m, "LastOnline", NumberToDifferenceConverter.DoConvert(m.LastOnline));
            if(!string.IsNullOrEmpty(m.PhysicalAddress))
                Add(m, "Address", m.PhysicalAddress);
        }
        public static void Add(CentralMember m, string name, string value)
        {
            m.Properties.Add(new CentralMemberVM(name, value));
        }
    }
    class CentralMember
    { 
        public static CentralMember CopyFrom(CentralMember c)
        {
            var o = (CentralMember)c.MemberwiseClone();
            o.Config = CentralMemberConfig.CopyFrom(c.Config);
            return o;
        }
        public List<CentralMemberVM> Properties
        {
            get; set;
        }
    /// <summary>
     /// Member record ID, which is formed from the network and node IDs [ro]
     /// </summary>
        [JsonProperty("id")]
        public string Id { get; set; }
        /// <summary>
        /// Object type (\"Member\") [ro]
        /// </summary>
        [JsonProperty("type")]
        public string Type { get; set; }
        /// <summary>
        /// System clock on server [ro]
        /// </summary>
        [JsonProperty("clock")]
        public UInt64 Clock { get; set; }
        /// <summary>
        /// 16-digit ZeroTier network ID [ro]
        /// </summary>
        [JsonProperty("networkId")]
        public string NetworkId { get; set; }
        /// <summary>
        /// 10-digit ZeroTier node ID / device address [ro]
        /// </summary>
        [JsonProperty("nodeId")]
        public string NodeId { get; set; }
        /// <summary>
        /// 0-digit ZeroTier node ID of controller (same as first 10 digits of network ID) [ro]
        /// </summary>
        [JsonProperty("controllerId")]
        public string ControllerId { get; set; }
        /// <summary>
        /// Hidden in UI? [rw]
        /// </summary>
        [JsonProperty("hidden")]
        public bool Hidden { get; set; }
        /// <summary>
        /// Short name describing member [rw]
        /// </summary>
        [JsonProperty("name")]
        public string Name { get; set; }
        /// <summary>
        /// Long form description [rw]
        /// </summary>
        [JsonProperty("description")]
        public string Description { get; set; }

        public class OnlinePresenceClass
        {
            public bool Online { get; set; }
            public ulong LastOnline { get; set; }
        }
        public OnlinePresenceClass OnlinePresence
        {
            get => new OnlinePresenceClass() { Online = Online, LastOnline = LastOnline };
        }

        /// <summary>
        /// Member is online? (has requested an update recently) [ro]
        /// </summary>
        [JsonProperty("online")]
        public bool Online { get; set; }
        /// <summary>
        /// Time member was last determined to be online [ro]"
        /// </summary>
        [JsonProperty("lastOnline")]
        public UInt64 LastOnline { get; set; }
        /// <summary>
        /// Time member was last determined to be online [ro]"
        /// </summary>
        [JsonProperty("lastOffline")]
        public UInt64 LastOffline { get; set; }
        /// <summary>
        /// Latest physical address of member [ro]
        /// </summary>
        [JsonProperty("physicalAddress")]
        public string PhysicalAddress { get; set; }
        /// <summary>
        /// Lat/lon of estimated (GeoIP-determined) location of physicalAddress (if available) [ro]"
        /// </summary>
        [JsonProperty("physicalLocation")]
        public string PhysicalLocation { get; set; }

        /// <summary>
        /// Most recent client software version [ro]
        /// </summary>
        [JsonProperty("clientVersion")]
        public string ClientVersion { get; set; }

        /// <summary>
        /// Most recent client-reported ZeroTier protocol version [ro]
        /// </summary>
        [JsonProperty("protocolVersion")]
        public int protocolVersion { get; set; }
        /// <summary>
        /// True if member supports circuit testing [ro]
        /// </summary>
        [JsonProperty("SupportsCircuitTesting")]
        public bool supportsCircuitTesting { get; set; }
        /// <summary>
        /// True if member supports the new (post-1.2) rules engine [ro]
        /// </summary>
        [JsonProperty("SupportsRulesEngine")]
        public bool supportsRulesEngine { get; set; }
        /// <summary>
        /// Notify of offline after this many milliseconds [rw]
        /// </summary>
        [JsonProperty("OfflineNotifyDelay")]
        public UInt64 offlineNotifyDelay { get; set; }
        [JsonProperty("config")]
        public CentralMemberConfig Config { get; set; }

        public class CentralMemberConfig
        {

            public static CentralMemberConfig CopyFrom(CentralMemberConfig c)
            {
                return (CentralMemberConfig)c.MemberwiseClone();
            }
            [JsonProperty("id")]
            public string Id { get; set; }

            [JsonProperty("nwid")]
            public string NetworkID { get; set; }

            [JsonProperty("name")]
            public string Name { get; set; }

            /// <summary>
            /// Object type on controller (\"member\") [ro]
            /// </summary>
            [JsonProperty("objtype")]
            public string ObjectType { get; set; }

            /// <summary>
            /// True if authorized (only matters on private networks) [rw]
            /// </summary>
            [JsonProperty("authorized")]
            public string authorized { get; set; }

            /// <summary>
            /// History of most recent authentications [ro]"
            /// </summary>
            [JsonProperty("authHistory")]
            public List<object> Authorized { get; set; }

            /// <summary>
            /// Array of IDs of capabilities assigned to this member [rw]
            /// </summary>
            [JsonProperty("capabilities")]
            public List<string> Capabilities { get; set; }

            /// <summary>
            /// Array of tuples of tag ID, tag value [rw]
            /// </summary>
            [JsonProperty("tags")]
            public List<Tuple<string, string>> Tags { get; set; }


            /// <summary>
            /// Time member record was first created [ro]
            /// </summary>
            [JsonProperty("creationTime")]
            public UInt64 CreationTime { get; set; }

            /// <summary>
            /// ZeroTier public identity of member (address and public key) [ro]
            /// </summary>
            [JsonProperty("identity")]
            public string Identity { get; set; }

            /// <summary>
            /// Array of IP assignments published to member [rw]
            /// </summary>
            [JsonProperty("ipAssignments")]
            public List<string> IpAssignments { get; set; }

            /// <summary>
            /// Time member was last authorized on network [ro]
            /// </summary>
            [JsonProperty("lastAuthorizedTime")]
            public UInt64 LastAuthorizedTime { get; set; }

            /// <summary>
            /// Time member was last de-authorized on network [ro]
            /// </summary>
            [JsonProperty("lastDeauthorizedTime")]
            public UInt64 LastDeauthorizedTime { get; set; }

            /// <summary>
            /// Time member was last de-authorized on network [ro]
            /// </summary>
            [JsonProperty("If true do not auto-assign IPv4 or IPv6 addresses, overriding network settings [rw]")]
            public bool NoAutoAssignIps { get; set; }
            /// <summary>
            /// Last known physical address of member [ro]
            /// </summary>
            [JsonProperty("physicalAddr")]
            public string PhysicalAddr { get; set; }
            /// <summary>
            /// Member record revision counter [ro]
            /// </summary>
            [JsonProperty("revision")]
            public UInt64 Revision { get; set; }
        }

        public override bool Equals(object obj)
        {
            var other = obj as CentralMember;
            if (other == null)
                return false;
            if (!(Id.Equals(other.Id)))
                return false;
            if (!NetworkId.Equals(other.NetworkId))
                return false;
            if (!NodeId.Equals(other.NodeId))
                return false;
            if (!PhysicalAddress.Equals(other.PhysicalAddress))
                return false;
            if (Online != other.Online)
                return false;
            if (LastOnline != other.LastOnline)
                return false;
            return true;
        }
        public override int GetHashCode()
        {
            return Id.GetHashCode() + NetworkId.GetHashCode() +
                NodeId.GetHashCode() + PhysicalAddress.GetHashCode() +
                Online.GetHashCode() + LastOnline.GetHashCode();
        }
    }
}
