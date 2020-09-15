using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Net;
using System.IO;
using System.Windows;
using Newtonsoft.Json;
using System.Diagnostics;
using System.Windows.Threading;

namespace WinUI
{
    public class APIHandler
    {
        private readonly string authtoken;

        private readonly string url = null;

        private static volatile APIHandler instance;
        private static readonly object syncRoot = new Object();

        public delegate void NetworkListCallback(List<ZeroTierNetwork> networks);
        public delegate void StatusCallback(ZeroTierStatus status);

        private string ZeroTierAddress = "";

        public static APIHandler Instance
        {
            get
            {
                if (instance == null)
                {
                    lock (syncRoot)
                    {
                        if (instance == null)
                        {
                            if (!initHandler())
                            {
                                return null;
                            }
                        }
                    }
                }

                return instance;
            }
        }

        private static bool initHandler(bool resetToken = false)
        {
            String localZtDir = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData) + "\\ZeroTier\\One";
            String globalZtDir = Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData) + "\\ZeroTier\\One";
            if (resetToken)
            {
                instance = null;
                if (File.Exists(localZtDir + "\\authtoken.secret"))
                {
                    File.Delete(localZtDir + "\\authtoken.secret");
                }

                if (File.Exists(localZtDir + "\\zerotier-one.port"))
                {
                    File.Delete(localZtDir + "\\zerotier-one.port");
                }
            }

            if (!File.Exists(localZtDir + "\\authtoken.secret") || !File.Exists(localZtDir + "\\zerotier-one.port"))
            {
                // launch external process to copy file into place
                String curPath = System.Reflection.Assembly.GetEntryAssembly().Location;
                int index = curPath.LastIndexOf("\\");
                curPath = curPath.Substring(0, index);
                ProcessStartInfo startInfo = new ProcessStartInfo(curPath + "\\copyutil.exe", "\"" + globalZtDir + "\" \"" + localZtDir + "\"")
                {
                    Verb = "runas"
                };

                var process = Process.Start(startInfo);
                process.WaitForExit();
            }

            string authToken = readAuthToken(localZtDir + "\\authtoken.secret");

            if ((authToken == null) || (authToken.Length < 1))
            {
                MessageBox.Show("Unable to read ZeroTier One authtoken", "ZeroTier One");
                return false;
            }

            int port = readPort(localZtDir + "\\zerotier-one.port");
            instance = new APIHandler(port, authToken);
            return true;
        }

        private static String readAuthToken(String path)
        {
            String authToken = "";

            if (File.Exists(path))
            {
                try
                {
                    byte[] tmp = File.ReadAllBytes(path);
                    authToken = System.Text.Encoding.UTF8.GetString(tmp).Trim();
                }
                catch
                {
                    MessageBox.Show("Unable to read ZeroTier One Auth Token from:\r\n" + path, "ZeroTier One");
                }
            }

            return authToken;
        }

        private static Int32 readPort(String path)
        {
            Int32 port = 9993;

            try
            {
                byte[] tmp = File.ReadAllBytes(path);
                port = Int32.Parse(System.Text.Encoding.ASCII.GetString(tmp).Trim());
                if ((port <= 0) || (port > 65535))
                    port = 9993;
            }
            catch
            {
            }

            return port;
        }

        private APIHandler()
        {
            url = "http://127.0.0.1:9993";
        }

        public APIHandler(int port, string authtoken)
        {
            url = "http://127.0.0.1:" + port;
            this.authtoken = authtoken;
        }

        public void GetStatus(StatusCallback cb)
        {
            var request = WebRequest.Create(url + "/status?auth=" + authtoken) as HttpWebRequest;
            if (request != null)
            {
                request.Method = "GET";
                request.ContentType = "application/json";
            }

            try
            {
                var httpResponse = (HttpWebResponse)request.GetResponse();
                if (httpResponse.StatusCode == HttpStatusCode.OK)
                {
                    using (var streamReader = new StreamReader(httpResponse.GetResponseStream()))
                    {
                        var responseText = streamReader.ReadToEnd();

                        ZeroTierStatus status = null;
                        try
                        {
                            status = JsonConvert.DeserializeObject<ZeroTierStatus>(responseText);

                            if (ZeroTierAddress != status.Address)
                            {
                                ZeroTierAddress = status.Address;
                            }
                        }
                        catch (JsonReaderException e)
                        {
                            Console.WriteLine(e.ToString());
                        }
                        cb(status);
                    }
                }
                else if (httpResponse.StatusCode == HttpStatusCode.Unauthorized)
                {
                    APIHandler.initHandler(true);
                }
            }
            catch (System.Net.Sockets.SocketException)
            {
                cb(null);
            }
            catch (System.Net.WebException e)
            {
                HttpWebResponse res = (HttpWebResponse)e.Response;
                if (res?.StatusCode == HttpStatusCode.Unauthorized)
                {
                    APIHandler.initHandler(true);
                }
                else
                {
                    cb(null);
                }
            }
        }

        public void GetNetworks(NetworkListCallback cb)
        {
            var request = WebRequest.Create(url + "/network?auth=" + authtoken) as HttpWebRequest;
            if (request == null)
            {
                cb(null);
            }

            request.Method = "GET";
            request.ContentType = "application/json";
            request.Timeout = 10000;

            try
            {
                var httpResponse = (HttpWebResponse)request.GetResponse();

                if (httpResponse.StatusCode == HttpStatusCode.OK)
                {
                    using (var streamReader = new StreamReader(httpResponse.GetResponseStream()))
                    {
                        var responseText = streamReader.ReadToEnd();

                        List<ZeroTierNetwork> networkList = null;
                        try
                        {
                            networkList = JsonConvert.DeserializeObject<List<ZeroTierNetwork>>(responseText);
                            foreach (ZeroTierNetwork n in networkList)
                            {
                                // all networks received via JSON are connected by definition
                                n.IsConnected = true;
                            }
                        }
                        catch (JsonReaderException e)
                        {
                            Console.WriteLine(e.ToString());
                        }
                        cb(networkList);
                    }
                }
                else if (httpResponse.StatusCode == HttpStatusCode.Unauthorized)
                {
                    APIHandler.initHandler(true);
                }
            }
            catch (System.Net.Sockets.SocketException)
            {
                cb(null);
            }
            catch (System.Net.WebException e)
            {
                HttpWebResponse res = (HttpWebResponse)e.Response;
                if (res?.StatusCode == HttpStatusCode.Unauthorized)
                {
                    APIHandler.initHandler(true);
                }
                else
                {
                    cb(null);
                }
            }
        }

        public void JoinNetwork(Dispatcher d, string nwid, bool allowManaged = true, bool allowGlobal = false, bool allowDefault = false)
        {
            Task.Factory.StartNew(() =>
            {
                if (!(WebRequest.Create(url + "/network/" + nwid + "?auth=" + authtoken) is HttpWebRequest request))
                {
                    return;
                }

                request.Method = "POST";
                request.ContentType = "applicaiton/json";
                request.Timeout = 30000;
                try
                {
                    using (var streamWriter = new StreamWriter(((HttpWebRequest)request).GetRequestStream()))
                    {
                        string json = "{\"allowManaged\":" + (allowManaged ? "true" : "false") + "," +
                                "\"allowGlobal\":" + (allowGlobal ? "true" : "false") + "," +
                                "\"allowDefault\":" + (allowDefault ? "true" : "false") + "}";
                        streamWriter.Write(json);
                        streamWriter.Flush();
                        streamWriter.Close();
                    }
                }
                catch (System.Net.WebException)
                {
                    d.BeginInvoke(DispatcherPriority.Normal, new Action(() => MessageBox.Show("Error Joining Network: Cannot connect to ZeroTier service.")));
                    return;
                }

                try
                {
                    var httpResponse = (HttpWebResponse)request.GetResponse();

                    if (httpResponse.StatusCode == HttpStatusCode.Unauthorized)
                    {
                        APIHandler.initHandler(true);
                    }
                    else if (httpResponse.StatusCode != HttpStatusCode.OK)
                    {
                        Console.WriteLine("Error sending join network message");
                    }
                }
                catch (System.Net.Sockets.SocketException)
                {
                    d.BeginInvoke(DispatcherPriority.Normal, new Action(() => MessageBox.Show("Error Joining Network: Cannot connect to ZeroTier service.")));
                }
                catch (System.Net.WebException e)
                {
                    HttpWebResponse res = (HttpWebResponse)e.Response;
                    if (res?.StatusCode == HttpStatusCode.Unauthorized)
                    {
                        APIHandler.initHandler(true);
                    }
                    d.BeginInvoke(DispatcherPriority.Normal, new Action(() => MessageBox.Show("Error Joining Network: Cannot connect to ZeroTier service.")));
                }
            });
        }

        public void LeaveNetwork(Dispatcher d, string nwid)
        {
            Task.Factory.StartNew(() =>
            {
                if (!(WebRequest.Create(url + "/network/" + nwid + "?auth=" + authtoken) is HttpWebRequest request))
                {
                    return;
                }

                request.Method = "DELETE";
                request.Timeout = 30000;

                try
                {
                    var httpResponse = (HttpWebResponse)request.GetResponse();

                    if (httpResponse.StatusCode == HttpStatusCode.Unauthorized)
                    {
                        APIHandler.initHandler(true);
                    }
                    else if (httpResponse.StatusCode != HttpStatusCode.OK)
                    {
                        Console.WriteLine("Error sending leave network message");
                    }
                }
                catch (System.Net.Sockets.SocketException)
                {
                    d.BeginInvoke(DispatcherPriority.Normal, new Action(() => MessageBox.Show("Error Leaving Network: Cannot connect to ZeroTier service.")));
                }
                catch (System.Net.WebException e)
                {
                    HttpWebResponse res = (HttpWebResponse)e.Response;
                    if (res?.StatusCode == HttpStatusCode.Unauthorized)
                    {
                        APIHandler.initHandler(true);
                    }
                    d.BeginInvoke(DispatcherPriority.Normal, new Action(() => MessageBox.Show("Error Leaving Network: Cannot connect to ZeroTier service.")));
                }
                catch
                {
                    Console.WriteLine("Error leaving network: Unknown error");
                }
            });
        }

        public delegate void PeersCallback(List<ZeroTierPeer> peers);

        public void GetPeers(PeersCallback cb)
        {
            var request = WebRequest.Create(url + "/peer?auth=" + authtoken) as HttpWebRequest;
            if (request == null)
            {
                cb(null);
            }

            request.Method = "GET";
            request.ContentType = "application/json";

            try
            {
                var httpResponse = (HttpWebResponse)request.GetResponse();
                if (httpResponse.StatusCode == HttpStatusCode.OK)
                {
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
                        cb(peerList);
                    }
                }
                else if (httpResponse.StatusCode == HttpStatusCode.Unauthorized)
                {
                    APIHandler.initHandler(true);
                }
            }
            catch (System.Net.Sockets.SocketException)
            {
                cb(null);
            }
            catch (System.Net.WebException e)
            {
                HttpWebResponse res = (HttpWebResponse)e.Response;
                if (res?.StatusCode == HttpStatusCode.Unauthorized)
                {
                    APIHandler.initHandler(true);
                }
                else
                {
                    cb(null);
                }
            }
        }

        public string NodeAddress()
        {
            return ZeroTierAddress;
        }
    }
}
