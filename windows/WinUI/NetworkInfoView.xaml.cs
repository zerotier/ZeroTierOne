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
        private APIHandler handler;
        private ZeroTierNetwork network;

        public NetworkInfoView(APIHandler handler, ZeroTierNetwork network)
        {
            InitializeComponent();

            this.handler = handler;
            this.network = network;

            UpdateNetworkData();
        }

        private void UpdateNetworkData()
        {
            this.networkId.Text = network.NetworkId;
            this.networkName.Text = network.NetworkName;
            this.networkStatus.Text = network.NetworkStatus;
            this.networkType.Text = network.NetworkType;
            this.macAddress.Text = network.MacAddress;
            this.mtu.Text = network.MTU.ToString();
            this.broadcastEnabled.Text = (network.BroadcastEnabled ? "ENABLED" : "DISABLED");
            this.bridgingEnabled.Text = (network.Bridge ? "ENABLED" : "DISABLED");
            this.deviceName.Text = network.DeviceName;

            string iplist = "";
            for (int i = 0; i < network.AssignedAddresses.Length; ++i)
            {
                iplist += network.AssignedAddresses[i];
                if (i < (network.AssignedAddresses.Length - 1))
                    iplist += "\n";
            }

            this.managedIps.Text = iplist;
        }

        public bool HasNetwork(ZeroTierNetwork network)
        {
            if (this.network.NetworkId.Equals(network.NetworkId))
                return true;

            return false;
        }

        private void leaveButton_Click(object sender, RoutedEventArgs e)
        {
            handler.LeaveNetwork(network.NetworkId);
        }
    }
}
