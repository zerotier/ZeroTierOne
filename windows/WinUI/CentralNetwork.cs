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
        public ObservableCollection<CentralMember> Members { get; }
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
    }
}
