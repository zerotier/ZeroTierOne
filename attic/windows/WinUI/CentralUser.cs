using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Newtonsoft.Json;

namespace WinUI
{
    class CentralUser
    {
        public class CentralGlobalPermissions
        {
            [JsonProperty("a")]
            public bool Administrator { get; set; }

            [JsonProperty("d")]
            public bool Delete { get; set; }

            [JsonProperty("m")]
            public bool Modify { get; set; }

            [JsonProperty("r")]
            public bool Read { get; set; }
        }

        [JsonProperty("id")]
        public string Id { get; set; }

        [JsonProperty("type")]
        public string Type { get; set; }

        [JsonProperty("clock")]
        public UInt64 Clock { get; set; }

        [JsonProperty("globalPermissions")]
        public CentralGlobalPermissions GlobalPermissions { get; set; }

        [JsonProperty("displayName")]
        public string DisplayName { get; set; }

        [JsonProperty("email")]
        public string Email { get; set; }

        [JsonProperty("smsNumber")]
        public string SmsNumber { get; set; }

        [JsonProperty("tokens")]
        public List<string> Tokens { get; set; }
    }
}
