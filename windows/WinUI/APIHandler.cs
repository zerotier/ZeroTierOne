using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Net;
using System.IO;
using Newtonsoft.Json;

namespace WinUI
{
    

    public class APIHandler
    {
        static string authtoken = "p3ptrzds5jkr2hbx5ipbyf04";  // delete me!

        private string url = null;

        public APIHandler()
        {
            url = "http://127.0.0.1:9993";
        }

        public APIHandler(string host, int port)
        {
            url = "http://" + host + ":" + port;
        }

        public ZeroTierStatus getStatus()
        {
            var request = WebRequest.Create(url + "/status" + "?auth=" + authtoken) as HttpWebRequest;
            if (request != null)
            {
                request.Method = "GET";
                request.ContentType = "application/json";
            }

            var httpResponse = (HttpWebResponse)request.GetResponse();
            using (var streamReader = new StreamReader(httpResponse.GetResponseStream()))
            {
                var responseText = streamReader.ReadToEnd();

                ZeroTierStatus status = JsonConvert.DeserializeObject<ZeroTierStatus>(responseText);
                return status;
            }
        }

        public List<ZeroTierNetwork> getNetworks()
        {
            var request = WebRequest.Create(url + "/network" + "?auth=" + authtoken) as HttpWebRequest;
            if (request != null)
            {
                request.Method = "GET";
                request.ContentType = "application/json";
            }

            var httpResponse = (HttpWebResponse)request.GetResponse();
            using (var streamReader = new StreamReader(httpResponse.GetResponseStream()))
            {
                var responseText = streamReader.ReadToEnd();
                Console.WriteLine(responseText);

                List<ZeroTierNetwork> networkList = JsonConvert.DeserializeObject<List<ZeroTierNetwork>>(responseText);
                return networkList;
            }
        }
    }
}
