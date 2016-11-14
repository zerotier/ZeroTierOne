using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
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
using System.Windows.Shapes;
using System.Text.RegularExpressions;
using System.Timers;
using System.Windows.Threading;
using System.IO;
using System.Diagnostics;

namespace WinUI
{
    /// <summary>
    /// Interaction logic for ToolbarItem.xaml
    /// </summary>
    public partial class ToolbarItem : Window
    {
        private APIHandler handler = APIHandler.Instance;

        private NetworkListView netListView = null;
        private List<ZeroTierNetwork> networkList = null;

        private NetworkMonitor mon = NetworkMonitor.Instance;

        private ObservableCollection<ZeroTierNetwork> _networkCollection = new ObservableCollection<ZeroTierNetwork>();

        public ObservableCollection<ZeroTierNetwork> NetworkCollection
        {
            get { return _networkCollection; }
            set { _networkCollection = value; }
        }

        private Timer timer = null;

        public ToolbarItem()
        {
            InitializeComponent();

            mon.SubscribeNetworkUpdates(updateNetworks);
            mon.SubscribeStatusUpdates(updateStatus);
        }

        ~ToolbarItem()
        {
            mon.UnsubscribeNetworkUpdates(updateNetworks);
            mon.UnsubscribeStatusUpdates(updateStatus);
        }

        private void updateNetworks(List<ZeroTierNetwork> networks)
        {
            if (networks != null)
            {
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new Action(() =>
                {
                    foreach (ZeroTierNetwork n in networks)
                    {
                        int index = _networkCollection.IndexOf(n);

                        if (index == -1)
                        {
                            _networkCollection.Add(n);
                        }
                        else
                        {
                            _networkCollection[index] = n;
                        }
                    }

                    this.networkList = networks;
                }));
            }
        }

        private void updateStatus(ZeroTierStatus status) 
        {
            if (status != null)
            {
                Dispatcher.BeginInvoke(DispatcherPriority.Normal, new Action(() =>
                {
                    nodeIdMenuItem.Header = "Node ID: " + status.Address;
                    nodeIdMenuItem.IsEnabled = true;
                }));
            }
        }

        private void ToolbarItem_TrayContextMenuOpen(object sender, System.Windows.RoutedEventArgs e)
        {
            Console.WriteLine("TrayContextMenuOpen");
        }

        private void ToolbarItem_PreviewTrayContextMenuOpen(object sender, System.Windows.RoutedEventArgs e)
        {
            Console.WriteLine("PreviewTrayContextMenuOpen");
        }

        private void ToolbarItem_NodeIDClicked(object sender, System.Windows.RoutedEventArgs e)
        {

        }

        private void ToolbarItem_ShowNetworksClicked(object sender, System.Windows.RoutedEventArgs e)
        {
            if (netListView == null)
            {
                netListView = new WinUI.NetworkListView();
                netListView.Closed += ShowNetworksClosed;
                netListView.Show();
            }
        }

        private void ShowNetworksClosed(object sender, System.EventArgs e)
        {
            netListView = null;
        }

        private void ToolbarItem_JoinNetworkClicked(object sender, System.EventArgs e)
        {

        }

        private void JoinNetworkClosed(object sender, System.EventArgs e)
        {

        }
    }
}
