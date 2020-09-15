using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Net.Http;
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
using Microsoft.Win32;

namespace WinUI
{
    /// <summary>
    /// Interaction logic for ToolbarItem.xaml
    /// </summary>
    public partial class ToolbarItem : Window, INotifyPropertyChanged
    {
        internal APIHandler handler = APIHandler.Instance;
        private Point netListLocation = new Point(0, 0);
        private Point joinNetLocation = new Point(0, 0);
        private Point aboutViewLocation = new Point(0, 0);
        private Point prefsViewLocation = new Point(0, 0);
        private NetworkListView netListView = new NetworkListView();
        private JoinNetworkView joinNetView = null;
        private AboutView aboutView = null;
        private PreferencesView prefsView = null;
        private NetworkMonitor mon = NetworkMonitor.Instance;

        public ObservableCollection<MenuItem> NetworkCollection { get; set; } = new ObservableCollection<MenuItem>();

        private string nodeId;

        public ToolbarItem()
        {
            InitializeComponent();

            mon.SubscribeNetworkUpdates(updateNetworks);
            mon.SubscribeStatusUpdates(updateStatus);

            SystemEvents.DisplaySettingsChanged += SystemEvents_DisplaySettingsChanged;
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
                Dispatcher.BeginInvoke(DispatcherPriority.Normal, new Action(() =>
                {
                    NetworkCollection.Clear();
                    foreach (ZeroTierNetwork n in networks)
                    {
                        MenuItem item = new MenuItem
                        {
                            Header = n.Title.Replace("_", "__"),
                            DataContext = n,
                            IsChecked = n.IsConnected
                        };
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
                    nodeId = status.Address;

                    newNetworkItem.IsEnabled = CentralAPI.Instance.HasAccessToken();
                }));
            }
        }

        private void ToolbarItem_NodeIDClicked(object sender, System.Windows.RoutedEventArgs e)
        {
            try
            {
                Clipboard.SetDataObject(nodeId);
            }
            catch (ArgumentNullException)
            {
                // tried to copy a null nodeId
                Console.WriteLine("ArgumentNullException");
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.ToString());
            }
        }

        private void ToolbarItem_ShowNetworksClicked(object sender, System.Windows.RoutedEventArgs e)
        {
            if (netListView == null)
            {
                netListView = new WinUI.NetworkListView();
                netListView.Closed += ShowNetworksClosed;
            }

            bool netListNeedsMoving = true;
            if (netListLocation.X > 0 && netListLocation.Y > 0)
            {
                netListView.Left = netListLocation.X;
                netListView.Top = netListLocation.Y;
                netListNeedsMoving = false;
            }

            netListView.Show();

            if (netListNeedsMoving)
            {
                setWindowPosition(netListView);
                netListLocation.X = netListView.Left;
                netListLocation.Y = netListView.Top;
            }

            netListView.Activate();
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

                bool needsMove = true;
                if (joinNetLocation.X > 0 && joinNetLocation.Y > 0)
                {
                    joinNetView.Left = joinNetLocation.X;
                    joinNetView.Top = joinNetLocation.Y;
                    needsMove = false;
                }

                joinNetView.Show();

                if (needsMove)
                {
                    setWindowPosition(joinNetView);
                    joinNetLocation.X = joinNetView.Left;
                    joinNetLocation.Y = joinNetView.Top;
                }
            }
            else
            {
                joinNetView.Activate();
            }
        }

        private void JoinNetworkClosed(object sender, System.EventArgs e)
        {
            joinNetView = null;
        }

        private void ToolbarItem_CentralClicked(object sender, System.EventArgs e)
        {
            Process.Start("https://my.zerotier.com");
        }

        private void ToolbarItem_AboutClicked(object sender, System.EventArgs e)
        {
            if (aboutView == null)
            {
                aboutView = new AboutView();
                aboutView.Closed += AboutClosed;

                bool needsMove = true;
                if (aboutViewLocation.X > 0 && aboutViewLocation.Y > 0)
                {
                    aboutView.Left = aboutViewLocation.X;
                    aboutView.Top = aboutViewLocation.Y;
                    needsMove = false;
                }

                aboutView.Show();

                if (needsMove)
                {
                    setWindowPosition(aboutView);
                    aboutViewLocation.X = aboutView.Left;
                    aboutViewLocation.Y = aboutView.Top;
                }
            }
            else
            {
                aboutView.Activate();
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

                bool needsMove = true;
                if (prefsViewLocation.X > 0 && prefsViewLocation.Y > 0)
                {
                    prefsView.Left = prefsViewLocation.X;
                    prefsView.Top = prefsViewLocation.Y;
                    needsMove = false;
                }

                prefsView.Show();

                if (needsMove)
                {
                    setWindowPosition(prefsView);
                    prefsViewLocation.X = prefsView.Left;
                    prefsViewLocation.Y = prefsView.Top;
                }
            }
            else
            {
                prefsView.Activate();
            }
        }

        private void PreferencesClosed(object sender, System.EventArgs e)
        {
            prefsView = null;
        }

        private void ToolbarItem_QuitClicked(object sender, System.EventArgs e)
        {
            NetworkMonitor.Instance.StopMonitor();
            Close();
            Application.Current.Shutdown();
        }

        private void ToolbarItem_NetworkClicked(object sender, System.Windows.RoutedEventArgs e)
        {
            if (sender.GetType() == typeof(MenuItem))
            {
                MenuItem item = e.Source as MenuItem;
                if (item.DataContext != null)
                {
                    ZeroTierNetwork network = item.DataContext as ZeroTierNetwork;
                    if (item.IsChecked)
                    {
                        APIHandler.Instance.LeaveNetwork(Dispatcher, network.NetworkId);
                    }
                    else
                    {
                        APIHandler.Instance.JoinNetwork(Dispatcher, network.NetworkId, network.AllowManaged, network.AllowGlobal, network.AllowDefault);
                    }
                }
            }
        }

        private async void ToolbarItem_NewNetwork(object sender, System.Windows.RoutedEventArgs e)
        {
            if (CentralAPI.Instance.HasAccessToken())
            {
                CentralAPI api = CentralAPI.Instance;
                CentralNetwork newNetwork = await api.CreateNewNetwork().ConfigureAwait(true);

                APIHandler handler = APIHandler.Instance;
                handler.JoinNetwork(this.Dispatcher, newNetwork.Id);

                string nodeId = APIHandler.Instance.NodeAddress();
                bool authorized = await CentralAPI.Instance.AuthorizeNode(nodeId, newNetwork.Id).ConfigureAwait(true);
            }
        }

        private void setWindowPosition(Window w)
        {
            double width = w.ActualWidth;
            double height = w.ActualHeight;

            double screenHeight = SystemParameters.PrimaryScreenHeight;
            double screenWidth = SystemParameters.PrimaryScreenWidth;

            double top = screenHeight - height - 40;
            double left = screenWidth - width - 20;

            w.Top = top;
            w.Left = left;
        }

        private void SystemEvents_DisplaySettingsChanged(object sender, EventArgs e)
        {
            // reset cached locations to (0, 0) when display size changes
            netListLocation.X = 0;
            netListLocation.Y = 0;
            joinNetLocation.X = 0;
            joinNetLocation.Y = 0;
            aboutViewLocation.X = 0;
            aboutViewLocation.Y = 0;
            prefsViewLocation.X = 0;
            prefsViewLocation.Y = 0;
        }
    }
}
