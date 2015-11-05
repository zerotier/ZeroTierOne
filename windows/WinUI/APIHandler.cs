using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Net;
using System.IO;
using System.Windows;
using Newtonsoft.Json;

namespace WinUI
{
    

    public class APIHandler
    {
        private string authtoken;

        private string url = null;

        public APIHandler()
        {
            url = "http://127.0.0.1:9993";
        }

        public APIHandler(int port, string authtoken)
        {
            url = "http://localhost:" + port;
            this.authtoken = authtoken;
        }

        public ZeroTierStatus GetStatus()
        {
            var request = WebRequest.Create(url + "/status" + "?auth=" + authtoken) as HttpWebRequest;
            if (request != null)
            {
                request.Method = "GET";
                request.ContentType = "application/json";
            }

            try
            {
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
            catch (System.Net.Sockets.SocketException)
            {
                return null;
            }
            catch (System.Net.WebException)
            {
                return null;
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

            try
            {
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
            catch (System.Net.Sockets.SocketException)
            {
                return null;
            }
            catch (System.Net.WebException)
            {
                return null;
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

            try
            {
                var httpResponse = (HttpWebResponse)request.GetResponse();

                if (httpResponse.StatusCode != HttpStatusCode.OK)
                {
                    Console.WriteLine("Error sending join network message");
                }
            }
            catch (System.Net.Sockets.SocketException)
            {
                MessageBox.Show("Error Joining Network: Cannot connect to ZeroTier service.");
            }
            catch (System.Net.WebException)
            {
                MessageBox.Show("Error Joining Network: Cannot connect to ZeroTier service.");
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

            try
            {
                var httpResponse = (HttpWebResponse)request.GetResponse();

                if (httpResponse.StatusCode != HttpStatusCode.OK)
                {
                    Console.WriteLine("Error sending leave network message");
                }
            }
            catch (System.Net.Sockets.SocketException)
            {
                MessageBox.Show("Error Leaving Network: Cannot connect to ZeroTier service.");
            }
            catch (System.Net.WebException)
            {
                MessageBox.Show("Error Leaving Network: Cannot connect to ZeroTier service.");
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

            try
            {
                var httpResponse = (HttpWebResponse)request.GetResponse();
                using (var streamReader = new StreamReader(httpResponse.GetResponseStream()))
                {
                    var responseText = streamReader.ReadToEnd();
                    //Console.WriteLine(responseText);
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
            catch (System.Net.Sockets.SocketException)
            {
                return null;
            }
            catch (System.Net.WebException)
            {
                return null;
            }
        }
    }
}
