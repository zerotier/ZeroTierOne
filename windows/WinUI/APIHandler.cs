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

        public ZeroTierStatus GetStatus()
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

                ZeroTierStatus status = null;
                try
                {
                    status = JsonConvert.DeserializeObject<ZeroTierStatus>(responseText);
                }
                catch (JsonReaderException e)
                {
                    Console.WriteLine(e.ToString());
                }
                return status;
            }
        }

        public List<ZeroTierNetwork> GetNetworks()
        {
            var request = WebRequest.Create(url + "/network" + "?auth=" + authtoken) as HttpWebRequest;
            if (request == null)
            {
                return null;
            }

            request.Method = "GET";
            request.ContentType = "application/json";

            var httpResponse = (HttpWebResponse)request.GetResponse();
            using (var streamReader = new StreamReader(httpResponse.GetResponseStream()))
            {
                var responseText = streamReader.ReadToEnd();

                List<ZeroTierNetwork> networkList = null;
                try
                {
                    networkList = JsonConvert.DeserializeObject<List<ZeroTierNetwork>>(responseText);
                }
                catch (JsonReaderException e)
                {
                    Console.WriteLine(e.ToString());
                }
                return networkList;
            }
        }

        public void JoinNetwork(string nwid)
        {
            var request = WebRequest.Create(url + "/network/" + nwid + "?auth=" + authtoken) as HttpWebRequest;
            if (request == null)
            {
                return;
            }

            request.Method = "POST";

            var httpResponse = (HttpWebResponse)request.GetResponse();

            if (httpResponse.StatusCode != HttpStatusCode.OK)
            {
                Console.WriteLine("Error sending join network message");
            }
        }

        public void LeaveNetwork(string nwid)
        {
            var request = WebRequest.Create(url + "/network/" + nwid + "?auth=" + authtoken) as HttpWebRequest;
            if (request == null)
            {
                return;
            }

            request.Method = "DELETE";

            var httpResponse = (HttpWebResponse)request.GetResponse();

            if (httpResponse.StatusCode != HttpStatusCode.OK)
            {
                Console.WriteLine("Error sending leave network message");
            }
        }

        public List<ZeroTierPeer> GetPeers()
        {
            var request = WebRequest.Create(url + "/peer" + "?auth=" + authtoken) as HttpWebRequest;
            if (request == null)
            {
                return null;
            }

            request.Method = "GET";
            request.ContentType = "application/json";

            var httpResponse = (HttpWebResponse)request.GetResponse();
            using (var streamReader = new StreamReader(httpResponse.GetResponseStream()))
            {
                var responseText = streamReader.ReadToEnd();

                List<ZeroTierPeer> peerList = null;
                try
                {
                    peerList = JsonConvert.DeserializeObject<List<ZeroTierPeer>>(responseText);
                }
                catch (JsonReaderException e)
                {
                    Console.WriteLine(e.ToString());
                }
                return peerList;
            }
        }
    }
}
