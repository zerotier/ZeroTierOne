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
        public ZeroTierNetwork network;

        public NetworkInfoView(APIHandler handler, ZeroTierNetwork network)
        {
            InitializeComponent();

            this.handler = handler;
            this.network = network;

            UpdateNetworkData();

            allowDefault.Checked += AllowDefault_CheckStateChanged;
            allowDefault.Unchecked += AllowDefault_CheckStateChanged;
            allowGlobal.Checked += AllowGlobal_CheckStateChanged;
            allowGlobal.Unchecked += AllowGlobal_CheckStateChanged;
            allowManaged.Checked += AllowManaged_CheckStateChanged;
            allowManaged.Unchecked += AllowManaged_CheckStateChanged;
        }

        private void UpdateNetworkData()
        {

            if (this.networkId.Text != network.NetworkId)
                this.networkId.Text = network.NetworkId;

            if (this.networkName.Text != network.NetworkName)
                this.networkName.Text = network.NetworkName;

            if (this.networkStatus.Text != network.NetworkStatus)
                this.networkStatus.Text = network.NetworkStatus;

            if (this.networkType.Text != network.NetworkType)
                this.networkType.Text = network.NetworkType;

            if (this.macAddress.Text != network.MacAddress)
                this.macAddress.Text = network.MacAddress;

            if (this.mtu.Text != network.MTU.ToString())
                this.mtu.Text = network.MTU.ToString();

            this.broadcastEnabled.Text = (network.BroadcastEnabled ? "ENABLED" : "DISABLED");
            this.bridgingEnabled.Text = (network.Bridge ? "ENABLED" : "DISABLED");

            if (this.deviceName.Text != network.DeviceName)
                this.deviceName.Text = network.DeviceName;

            string iplist = "";
            for (int i = 0; i < network.AssignedAddresses.Length; ++i)
            {
                iplist += network.AssignedAddresses[i];
                if (i < (network.AssignedAddresses.Length - 1))
                    iplist += "\n";
            }

            if (this.managedIps.Text != iplist)
                this.managedIps.Text = iplist;

            this.allowDefault.IsChecked = network.AllowDefault;
            this.allowGlobal.IsChecked = network.AllowGlobal;
            this.allowManaged.IsChecked = network.AllowManaged;
        }

        public bool HasNetwork(ZeroTierNetwork network)
        {
            if (this.network.NetworkId.Equals(network.NetworkId))
                return true;

            return false;
        }

        public void SetNetworkInfo(ZeroTierNetwork network)
        {
            this.network = network;

            UpdateNetworkData();
        }

        private void leaveButton_Click(object sender, RoutedEventArgs e)
        {
            handler.LeaveNetwork(network.NetworkId);
        }

        private void AllowManaged_CheckStateChanged(object sender, RoutedEventArgs e)
        {
            CheckBox cb = sender as CheckBox;
            handler.JoinNetwork(network.NetworkId,
                allowManaged.IsChecked ?? false,
                allowGlobal.IsChecked ?? false,
                allowDefault.IsChecked ?? false);
        }

        private void AllowGlobal_CheckStateChanged(object sender, RoutedEventArgs e)
        {
            CheckBox cb = sender as CheckBox;
            handler.JoinNetwork(network.NetworkId,
                allowManaged.IsChecked ?? false,
                allowGlobal.IsChecked ?? false,
                allowDefault.IsChecked ?? false);
        }

        private void AllowDefault_CheckStateChanged(object sender, RoutedEventArgs e)
        {
            CheckBox cb = sender as CheckBox;
            handler.JoinNetwork(network.NetworkId,
                allowManaged.IsChecked ?? false,
                allowGlobal.IsChecked ?? false,
                allowDefault.IsChecked ?? false);
        }
    }
}
