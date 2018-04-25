using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Newtonsoft.Json;

namespace WinUI
{
    class CentralToken
    {
        [JsonProperty("token")]
        public string Token { get; set; }

        [JsonProperty("clock")]
        public UInt64 Clock { get; set; }

        [JsonProperty("raw")]
        public string Raw { get; set; }
    }
}
