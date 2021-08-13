using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;
using Newtonsoft.Json;

namespace WinUI
{
    class CentralAPI
    {
        private static volatile CentralAPI instance;
        private static object syncRoot = new Object();

        private CookieContainer cookieContainer;
        private HttpClientHandler clientHandler;
        private HttpClient client;

        private CentralServer server;
        public CentralServer Central
        {
            get
            {
                return this.server;
            }
            set
            {
                this.server = value;
                WriteCentralConfig();
                UpdateRequestHeaders();
            }
        }

        public static CentralAPI Instance
        {
            get
            {
                if (instance == null)
                {
                    lock (syncRoot)
                    {
                        if (instance == null)
                        {
                            instance = new CentralAPI();
                        }
                    }
                }

                return instance;
            }
        }



        private CentralAPI()
        {
#if DEBUG
            ServicePointManager.ServerCertificateValidationCallback += (sender, cert, chain, sslPolicyErrors) => true;
#endif
            cookieContainer = new CookieContainer();
            clientHandler = new HttpClientHandler
            {
                AllowAutoRedirect = true,
                UseCookies = true,
                CookieContainer = cookieContainer
            };

            client = new HttpClient(clientHandler);

            string centralConfigPath = CentralConfigFile();
            if (File.Exists(centralConfigPath))
            {
                byte[] tmp = File.ReadAllBytes(centralConfigPath);
                string json = Encoding.UTF8.GetString(tmp).Trim();
                CentralServer ctmp = JsonConvert.DeserializeObject<CentralServer>(json);
                if (ctmp != null)
                {
                    Central = ctmp;
                } 
                else
                {
                    Central = new CentralServer();
                }
            }
            else
            {
                Central = new CentralServer();
            }
        }

        public bool HasAccessToken()
        {
            if (Central == null)
                return false;

            return !string.IsNullOrEmpty(Central.APIKey);
        }

        private string ZeroTierDir()
        {
            return Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData) + "\\ZeroTier\\One";
        }

        private string CentralConfigFile()
        {
            return ZeroTierDir() + "\\central.conf";
        }

        public void WriteCentralConfig()
        {
            string json = JsonConvert.SerializeObject(Central);
            byte[] tmp = Encoding.UTF8.GetBytes(json);
            if (tmp != null)
            {
                File.WriteAllBytes(CentralConfigFile(), tmp);
            }
        }

        private void UpdateRequestHeaders()
        {
            if (client.DefaultRequestHeaders.Contains("Authorization"))
            {
                client.DefaultRequestHeaders.Remove("Authorization");
            }

            if (!string.IsNullOrEmpty(Central.APIKey))
            {
                client.DefaultRequestHeaders.Add("Authorization", "bearer " + Central.APIKey);
            }
        }

        public async Task<bool> Login(string email, string password, bool isNewUser)
        {
            string postURL = Central.ServerURL + "/api/_auth/local";
            CentralLogin login = new CentralLogin(email, password, isNewUser);
            var content = new StringContent(JsonConvert.SerializeObject(login), Encoding.UTF8, "application/json");
            HttpResponseMessage response = await client.PostAsync(postURL, content);

            if (!response.IsSuccessStatusCode)
            {
                return false;
            }

            string resContent = await response.Content.ReadAsStringAsync();

            CentralUser user = JsonConvert.DeserializeObject<CentralUser>(resContent);

            if (user.Tokens.Count == 0)
            {
                // create token
                user = await CreateAuthToken(user);
            }

            Central.APIKey = user.Tokens[0];

            UpdateRequestHeaders();
            WriteCentralConfig();

            return true;
        }

        public async Task<CentralUser> CreateAuthToken(CentralUser user)
        {
            string randomTokenURL = Central.ServerURL + "/api/randomToken";
            HttpResponseMessage response = await client.GetAsync(randomTokenURL);

            if (!response.IsSuccessStatusCode)
            {
                // TODO: throw an error
                return null;
            }

            string resContent = await response.Content.ReadAsStringAsync();

            CentralToken t = JsonConvert.DeserializeObject<CentralToken>(resContent);

            user.Tokens.Add(t.Token);

            string tokenObj = "{ \"tokens\": " + JsonConvert.SerializeObject(user.Tokens) + " } ";

            string postURL = Central.ServerURL + "/api/user/" + user.Id;
            var postContent = new StringContent(tokenObj, Encoding.UTF8, "application/json");
            response = await client.PostAsync(postURL, postContent);

            if (!response.IsSuccessStatusCode)
            {
                // TODO: thrown an error
                return null;
            }

            resContent = await response.Content.ReadAsStringAsync();
            user = JsonConvert.DeserializeObject<CentralUser>(resContent);

            return user;
        }

        public async Task<List<CentralNetwork>> GetNetworkList()
        {
            string networkURL = Central.ServerURL + "/api/network";

            HttpResponseMessage response = await client.GetAsync(networkURL);

            if (!response.IsSuccessStatusCode)
            {
                // TODO:  Throw Error
                return new List<CentralNetwork>();
            }

            string resContent = await response.Content.ReadAsStringAsync();

            List<CentralNetwork> networkList = JsonConvert.DeserializeObject<List<CentralNetwork>>(resContent);

            return networkList;
        }

        public async Task<CentralNetwork> CreateNewNetwork()
        {
            string networkURL = Central.ServerURL + "/api/network?easy=1";
            CentralNetwork network = new CentralNetwork();
            network.Config = new CentralNetwork.CentralNetworkConfig();
            network.Config.Name = NetworkNameGenerator.GenerateName();
            string jsonNetwork = JsonConvert.SerializeObject(network);
            var postContent = new StringContent(jsonNetwork, Encoding.UTF8, "application/json");
            HttpResponseMessage response = await client.PostAsync(networkURL, postContent);

            if (!response.IsSuccessStatusCode)
            {
                return null;
            }

            string resContent = await response.Content.ReadAsStringAsync();

            CentralNetwork newNetwork = JsonConvert.DeserializeObject<CentralNetwork>(resContent);

            return newNetwork;
        }

        public async Task<bool> AuthorizeNode(string nodeAddress, string networkId)
        {
            string json = "{ \"config\": { \"authorized\": true } }";
            string postURL = Central.ServerURL + "/api/network/" + networkId + "/member/" + nodeAddress;
            var postContent = new StringContent(json, Encoding.UTF8, "application/json");
            HttpResponseMessage response = await client.PostAsync(postURL, postContent);

            if (response.IsSuccessStatusCode)
            {
                return true;
            }

            return false;
        }
    }
}
