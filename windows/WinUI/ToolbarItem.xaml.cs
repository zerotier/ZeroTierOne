using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Runtime.CompilerServices;
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
    public partial class ToolbarItem : Window, INotifyPropertyChanged
    {
        private APIHandler handler = APIHandler.Instance;

        private NetworkListView netListView = null;
        private JoinNetworkView joinNetView = null;
        private AboutView aboutView = null;
        private PreferencesView prefsView = null;

        private NetworkMonitor mon = NetworkMonitor.Instance;

        private ObservableCollection<MenuItem> _networkCollection = new ObservableCollection<MenuItem>();

        public ObservableCollection<MenuItem> NetworkCollection
        {
            get { return _networkCollection; }
            set { _networkCollection = value; }
        }

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

        public event PropertyChangedEventHandler PropertyChanged;

        protected void NotifyPropertyChanged([CallerMemberName] string propertyName = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }

        private void updateNetworks(List<ZeroTierNetwork> networks)
        {
            if (networks != null)
            {
                this.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new Action(() =>
                {
                    NetworkCollection.Clear();
                    foreach (ZeroTierNetwork n in networks)
                    {
                        MenuItem item = new MenuItem();
                        item.Header = n.Title;
                        item.DataContext = n;
                        item.IsChecked = n.IsConnected;
                        item.Click += ToolbarItem_NetworkClicked;

                        NetworkCollection.Add(item);
                    }
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
            // TODO: Copy Node ID to clipboard
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
            if (joinNetView == null)
            {
                joinNetView = new JoinNetworkView();
                joinNetView.Closed += JoinNetworkClosed;
                joinNetView.Show();
            }
        }

        private void JoinNetworkClosed(object sender, System.EventArgs e)
        {
            joinNetView = null;
        }

        private void ToolbarItem_AboutClicked(object sender, System.EventArgs e)
        {
            if (aboutView == null)
            {
                aboutView = new AboutView();
                aboutView.Closed += AboutClosed;
                aboutView.Show();
            }
        }

        private void AboutClosed(object sender, System.EventArgs e)
        {
            aboutView = null;
        }

        private void ToolbarItem_PreferencesClicked(object sender, System.EventArgs e)
        {
            if (prefsView == null)
            {
                prefsView = new PreferencesView();
                prefsView.Closed += PreferencesClosed;
                prefsView.Show();
            }
        }

        private void PreferencesClosed(object sender, System.EventArgs e)
        {
            prefsView = null;
        }

        private void ToolbarItem_QuitClicked(object sender, System.EventArgs e)
        {
            NetworkMonitor.Instance.StopMonitor();
            this.Close();
            Application.Current.Shutdown();
        }

        private void ToolbarItem_NetworkClicked(object sender, System.Windows.RoutedEventArgs e)
        {
            if(sender.GetType() == typeof(MenuItem))
            {
                MenuItem item = e.Source as MenuItem;
                if (item.DataContext != null)
                {
                    ZeroTierNetwork network = item.DataContext as ZeroTierNetwork;
                    if (item.IsChecked)
                    {
                        APIHandler.Instance.LeaveNetwork(network.NetworkId);
                    }
                    else
                    {
                        APIHandler.Instance.JoinNetwork(network.NetworkId, network.AllowManaged, network.AllowGlobal, network.AllowDefault);
                    }
                }   
            }
        }
    }
}
