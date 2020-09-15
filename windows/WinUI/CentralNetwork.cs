using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Newtonsoft.Json;

namespace WinUI
{
    internal class CentralNetwork
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
