using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Newtonsoft.Json;

namespace WinUI
{
    public class NetworkRoute
    {
        [JsonProperty("target")]
        public string Target { get; set; }

        [JsonProperty("via")]
        public string Via { get; set; }

        [JsonProperty("flags")]
        public int Flags { get; set; }

        [JsonProperty("metric")]
        public int Metric { get; set; }
    }
}
