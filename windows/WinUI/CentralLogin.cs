using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Newtonsoft.Json;

namespace WinUI
{
    internal class CentralLogin
    {

        public CentralLogin(string email, string password, bool isNew)
        {
            Login = email;
            Password = password;
            IsNew = isNew;
        }

        [JsonProperty("login")]
        public string Login { get; set; }

        [JsonProperty("password")]
        public string Password { get; set; }

        [JsonProperty("register")]
        public bool IsNew { get; set; }
    }
}
