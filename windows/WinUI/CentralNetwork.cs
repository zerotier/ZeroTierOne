using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Newtonsoft.Json;

namespace WinUI
{
    class CentralNetwork
    {
        public static CentralNetwork CopyFrom(CentralNetwork c)
        {
            var t = (CentralNetwork)c.MemberwiseClone();
            t.Config = new CentralNetworkConfig();
            t.Config.Id = c.Config.Id;
            t.Config.NetworkID = c.Config.NetworkID;
            t.Config.Name = c.Config.Name;
            if((c.Members != null) && (c.Members.Count>0))
            {
                t.Members = new ObservableCollection<CentralMember>();
                foreach (var m in c.Members)
                    t.Members.Add(CentralMember.CopyFrom(m));
            }
            return t;
        }
        [JsonProperty("id")]
        public string Id { get; set; }

        [JsonProperty("type")]
        public string Type { get; set; }

        [JsonProperty("clock")]
        public UInt64 Clock { get; set; }

        [JsonProperty("rulesSource")]
        public string RulesSource { get; set; }

        [JsonProperty("description")]
        public string Description { get; set; }

        [JsonProperty("ownerId")]
        public string OwnerID { get; set; }

        [JsonProperty("onlineMemberCount")]
        public int OnlineMemberCount { get; set; }

        [JsonProperty("config")]
        public CentralNetworkConfig Config { get; set; }

        /// <summary>
        /// Note: update me manually before binding
        /// </summary>
        public ObservableCollection<CentralMember> Members { get; private set; }
          = new ObservableCollection<CentralMember>();

        public class CentralNetworkConfig
        {
            [JsonProperty("id")]
            public string Id { get; set; }

            [JsonProperty("nwid")]
            public string NetworkID { get; set; }

            [JsonProperty("name")]
            public string Name { get; set; }
        }

        public override bool Equals(object obj)
        {
            var other = obj as CentralNetwork;
            if (other == null)
                return false;
            return (Id == other.Id) && (Type == other.Type) && (Clock == other.Clock) && (RulesSource == other.RulesSource) &&
                (Description == other.Description) && (OwnerID == other.OwnerID);
        }
        public override int GetHashCode()
        {
            return Id.GetHashCode() + Type.GetHashCode() + Clock.GetHashCode() + RulesSource.GetHashCode() +
                Description.GetHashCode() + OwnerID.GetHashCode();
        }
    }
}
