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
        public ZeroTierNetwork network;

        public NetworkInfoView(ZeroTierNetwork network)
        {
            InitializeComponent();

            this.network = network;

            UpdateNetworkData();

            allowDefault.Checked += AllowDefault_CheckStateChanged;
            allowDefault.Unchecked += AllowDefault_CheckStateChanged;
            allowGlobal.Checked += AllowGlobal_CheckStateChanged;
            allowGlobal.Unchecked += AllowGlobal_CheckStateChanged;
            allowManaged.Checked += AllowManaged_CheckStateChanged;
            allowManaged.Unchecked += AllowManaged_CheckStateChanged;
            allowDNS.Checked += AllowDNS_CheckStateChanged;
            allowDNS.Unchecked += AllowDNS_CheckStateChanged;
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
            this.allowDNS.IsChecked = network.AllowDNS;

            this.connectedCheckBox.Checked -= connectedCheckBox_Checked;
            this.connectedCheckBox.Unchecked -= connectedCheckbox_Unchecked;

            this.connectedCheckBox.IsChecked = network.IsConnected;

            this.connectedCheckBox.Checked += connectedCheckBox_Checked;
            this.connectedCheckBox.Unchecked += connectedCheckbox_Unchecked;
        }

        public bool HasNetwork(ZeroTierNetwork network)
        {
            return this.network.NetworkId.Equals(network.NetworkId);
        }

        public void SetNetworkInfo(ZeroTierNetwork network)
        {
            this.network = network;

            UpdateNetworkData();
        }

        private void deleteButton_Click(object sender, RoutedEventArgs e)
        {
            APIHandler.Instance.LeaveNetwork(this.Dispatcher, network.NetworkId);
            NetworkMonitor.Instance.RemoveNetwork(network.NetworkId);
        }

        private void AllowManaged_CheckStateChanged(object sender, RoutedEventArgs e)
        {
            CheckBox cb = sender as CheckBox;
            APIHandler.Instance.JoinNetwork(this.Dispatcher, network.NetworkId,
                allowManaged.IsChecked ?? false,
                allowGlobal.IsChecked ?? false,
                allowDefault.IsChecked ?? false,
                allowDNS.IsChecked ?? false);
        }

        private void AllowGlobal_CheckStateChanged(object sender, RoutedEventArgs e)
        {
            CheckBox cb = sender as CheckBox;
            APIHandler.Instance.JoinNetwork(this.Dispatcher, network.NetworkId,
                allowManaged.IsChecked ?? false,
                allowGlobal.IsChecked ?? false,
                allowDefault.IsChecked ?? false,
                allowDNS.IsChecked ?? false);
        }

        private void AllowDefault_CheckStateChanged(object sender, RoutedEventArgs e)
        {
            CheckBox cb = sender as CheckBox;
            APIHandler.Instance.JoinNetwork(this.Dispatcher, network.NetworkId,
                allowManaged.IsChecked ?? false,
                allowGlobal.IsChecked ?? false,
                allowDefault.IsChecked ?? false,
                allowDNS.IsChecked ?? false);
        }

        private void AllowDNS_CheckStateChanged(object sender, RoutedEventArgs e)
        {
            CheckBox cb = sender as CheckBox;
            APIHandler.Instance.JoinNetwork(this.Dispatcher, network.NetworkId,
                allowManaged.IsChecked ?? false,
                allowGlobal.IsChecked ?? false,
                allowDefault.IsChecked ?? false,
                allowDNS.IsChecked ?? false);
        }

        private void connectedCheckBox_Checked(object sender, RoutedEventArgs e)
        {
            onConnectedCheckboxUpdated(true);
        }

        private void connectedCheckbox_Unchecked(object sender, RoutedEventArgs e)
        {
            onConnectedCheckboxUpdated(false);
        }

        private void onConnectedCheckboxUpdated(bool isChecked)
        {
            if (isChecked)
            {
                bool global = allowGlobal.IsChecked.Value;
                bool managed = allowManaged.IsChecked.Value;
                bool defRoute = allowDefault.IsChecked.Value;
                bool dns = allowDNS.IsChecked.Value;

                APIHandler.Instance.JoinNetwork(this.Dispatcher, networkId.Text, managed, global, defRoute, dns);
            }
            else
            {
                APIHandler.Instance.LeaveNetwork(this.Dispatcher, networkId.Text);
            }
        }
    }
}
