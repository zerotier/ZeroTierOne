using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace WinUI
{
    class NetworkMonitor
    {
        public delegate void NetworkListCallback(List<ZeroTierNetwork> networks);
        public delegate void StatusCallback(ZeroTierStatus status);

        private Thread runThread;
        private NetworkListCallback _nwCb;
        private StatusCallback _stCb;


        private List<ZeroTierNetwork> _knownNetworks = new List<ZeroTierNetwork>();

        private static NetworkMonitor instance;
        private static object syncRoot = new object();

        public static NetworkMonitor Instance
        {
            get
            {
                if (instance == null)
                {
                    lock (syncRoot)
                    {
                        if (instance == null)
                        {
                            instance = new NetworkMonitor();
                        }
                    }
                }

                return instance;
            }
        }

        private NetworkMonitor()
        {
            runThread = new Thread(new ThreadStart(run));
            loadNetworks();

            runThread.Start();
        }

        ~NetworkMonitor()
        {
            runThread.Interrupt();
        }

        private void loadNetworks()
        {
            String dataPath = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData) + "\\ZeroTier\\One";
            String dataFile = Path.Combine(dataPath, "networks.dat");

            if (File.Exists(dataFile))
            {
                List<ZeroTierNetwork> netList;

                using (Stream stream = File.Open(dataFile, FileMode.Open))
                {
                    var bformatter = new System.Runtime.Serialization.Formatters.Binary.BinaryFormatter();
                    netList = (List<ZeroTierNetwork>)bformatter.Deserialize(stream);
                    stream.Close();
                }

                lock (_knownNetworks)
                {
                    _knownNetworks = netList;
                }
            }
        }

        private void writeNetworks()
        {
            String dataPath = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData) + "\\ZeroTier\\One";
            String dataFile = Path.Combine(dataPath, "networks.dat");

            if (!Directory.Exists(dataPath))
            {
                Directory.CreateDirectory(dataPath);
            }

            using (Stream stream = File.Open(dataFile, FileMode.OpenOrCreate))
            {
                lock (_knownNetworks)
                {
                    var bformatter = new System.Runtime.Serialization.Formatters.Binary.BinaryFormatter();
                    bformatter.Serialize(stream, _knownNetworks);
                    stream.Flush();
                    stream.Close();
                }
            }
        }

        private void apiNetworkCallback(List<ZeroTierNetwork> networks)
        {
            if (networks == null)
            {
                return;
            }

            lock (_knownNetworks)
            {
                _knownNetworks = _knownNetworks.Union(networks, new NetworkEqualityComparer()).ToList();

                foreach (ZeroTierNetwork n in _knownNetworks)
                {
                    if (networks.Contains(n))
                    {
                        n.IsConnected = true;
                    }
                    else
                    {
                        n.IsConnected = false;
                    }
                }

                _knownNetworks.Sort();
                _nwCb(_knownNetworks);
            }

            writeNetworks();
        }

        private void apiStatusCallback(ZeroTierStatus status)
        {
            _stCb(status);
        }

        private void run()
        {
            try
            {
                while (runThread.IsAlive)
                {
                    APIHandler handler = APIHandler.Instance;

                    if (handler != null)
                    {
                        handler.GetNetworks(apiNetworkCallback);
                        handler.GetStatus(apiStatusCallback);
                    }

                    Thread.Sleep(2000);
                }
            }
            catch (Exception e)
            {
                Console.WriteLine("Monitor Thread Exception: " + "\n" + e.StackTrace);
            }
            Console.WriteLine("Monitor Thread Ended");
        }

        public void SubscribeStatusUpdates(StatusCallback cb)
        {
            _stCb += cb;
        }

        public void UnsubscribeStatusUpdates(StatusCallback cb)
        {
            _stCb -= cb;
        }

        public void SubscribeNetworkUpdates(NetworkListCallback cb)
        {
            _nwCb += cb;
        }

        public void UnsubscribeNetworkUpdates(NetworkListCallback cb)
        {
            _nwCb -= cb;
        }

        public void RemoveNetwork(String networkID)
        {
            lock (_knownNetworks)
            {
                foreach (ZeroTierNetwork n in _knownNetworks)
                {
                    if (n.NetworkId.Equals(networkID))
                    {
                        _knownNetworks.Remove(n);
                        writeNetworks();
                        break;
                    }
                }
            }
        }

        public void StopMonitor()
        {
            runThread.Abort();
        }
    }


    class CentralNetworkMonitor
    {
        int POLL_INTERVAL = 5000;
        public delegate void NetworkChangesCallback(List<CentralNetwork> networks);
        private Thread runThread;
        private NetworkChangesCallback _callback;
        private List<CentralNetwork> _knownNetworks = new List<CentralNetwork>();

        private static object syncRoot = new object();
        #region Instance
        private static CentralNetworkMonitor instance;
        public static CentralNetworkMonitor Instance
        {
            get
            {
                if (instance == null)
                {
                    lock (syncRoot)
                    {
                        if (instance == null)
                        {
                            instance = new CentralNetworkMonitor();
                        }
                    }
                }

                return instance;
            }
        }
        #endregion
        private CentralNetworkMonitor()
        {
            runThread = new Thread(new ThreadStart(run));
            runThread.Start();
        }

        ~CentralNetworkMonitor()
        {
            runThread.Interrupt();
        }

        private  void run()
        {
            try
            {
                while (runThread.IsAlive)
                {
                    lock (syncRoot)
                    {
                        UpdateList().ConfigureAwait(false);
                        _callback?.Invoke(_knownNetworks);
                        Thread.Sleep(POLL_INTERVAL);
                    }
                }
            }
            catch (Exception e)
            {
                Console.WriteLine("Monitor Thread Exception: " + "\n" + e.StackTrace);
            }
            Console.WriteLine("Monitor Thread Ended");
        }

        private async Task UpdateList()
        {
            CentralAPI handler = CentralAPI.Instance;
            var l = await handler.GetNetworkList();
            foreach (var n in l)
            {
                var members = await handler.GetMembersList(n.Id);
                n.Members.Clear();
                foreach (var m in members)
                    n.Members.Add(m);
            }
            _knownNetworks.Clear();
            _knownNetworks.AddRange(l);
        }

        public void SubscribeNetworkUpdates(NetworkChangesCallback cb)
        {
            _callback += cb;
        }

        public void UnsubscribeNetworkUpdates(NetworkChangesCallback cb)
        {
            _callback -= cb;
        }

        public void StopMonitor()
        {
            runThread.Abort();
        }

    }
}
