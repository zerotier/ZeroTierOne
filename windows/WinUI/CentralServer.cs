using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Newtonsoft.Json;

namespace WinUI
{
    class CentralServer
    {
        public CentralServer()
        {
            ServerURL = "https://my.zerotier.com";
        }

        [JsonProperty("server_url")]
        public string ServerURL { get; set; }

        [JsonProperty("api_key")]
        public string APIKey { get; set; }
    }
}
