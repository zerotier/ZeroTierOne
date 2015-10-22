using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace WinUI
{
    /// <summary>
    /// Interaction logic for NetworkInfoView.xaml
    /// </summary>
    public partial class NetworkInfoView : UserControl
    {
        ZeroTierNetwork network;

        public NetworkInfoView(ZeroTierNetwork network)
        {
            InitializeComponent();
            this.network = network;

            updateNetworkData();
        }

        private void updateNetworkData()
        {
            this.networkId.Text = network.nwid;
            this.networkName.Text = network.name;
            this.networkStatus.Text = network.status;
            this.networkType.Text = network.type;
            this.macAddress.Text = network.mac;
            this.mtu.Text = network.mtu.ToString();
            this.broadcastEnabled.Text = (network.broadcastEnabled ? "ENABLED" : "DISABLED");
            this.bridgingEnabled.Text = (network.bridge ? "ENABLED" : "DISABLED");
            this.deviceName.Text = network.portDeviceName;

            string iplist = "";
            for (int i = 0; i < network.assignedAddresses.Length; ++i)
            {
                iplist += network.assignedAddresses[i];
                if (i < (network.assignedAddresses.Length - 1))
                    iplist += "\n";
            }

            this.managedIps.Text = iplist;
        }

        public bool hasNetwork(ZeroTierNetwork network)
        {
            if (this.network.nwid.Equals(network.nwid))
                return true;

            return false;
        }
    }
}
