using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace WinUI
{
    internal sealed class NetworkMonitor
    {
        public delegate void NetworkListCallback(List<ZeroTierNetwork> networks);
        public delegate void StatusCallback(ZeroTierStatus status);

        private readonly Thread runThread;
        private NetworkListCallback _nwCb;
        private StatusCallback _stCb;

        private List<ZeroTierNetwork> _knownNetworks = new List<ZeroTierNetwork>();

        private static NetworkMonitor instance;
        private static readonly object syncRoot = new object();

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
                    n.IsConnected = networks.Contains(n);
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
                Console.WriteLine("Monitor Thread Exception: \n" + e.StackTrace);
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
}
