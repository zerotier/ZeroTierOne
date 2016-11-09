using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Timers;
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
using System.Windows.Threading;

namespace WinUI
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class NetworkList : Window
    {
        Regex charRegex = new Regex("[0-9a-fxA-FX]");
        Regex wholeStringRegex = new Regex("^[0-9a-fxA-FX]+$");

        Timer timer = new Timer();

        bool connected = false;

        public NetworkList()
        {
            InitializeComponent();

            updateStatus();

            if (!connected)
            {
                MessageBox.Show("Unable to connect to ZerOTier Service");
                return;
            }

            updateNetworks();

            DataObject.AddPastingHandler(joinNetworkID, OnPaste);

            timer.Elapsed += new ElapsedEventHandler(OnUpdateTimer);
            timer.Interval = 2000;
            timer.Enabled = true;
        }

        private void updateStatus()
        {
            var status = APIHandler.Instance.GetStatus();

            if (status != null)
            {
                connected = true;

                networkId.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new Action(() =>
                {
                    this.networkId.Content = status.Address;
                }));
                versionString.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new Action(() =>
                {
                    this.versionString.Content = status.Version;
                }));
                onlineStatus.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new Action(() =>
                {
                    this.onlineStatus.Content = (status.Online ? "ONLINE" : "OFFLINE");
                }));
            }
            else
            {
                connected = false;

                networkId.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new Action(() =>
                {
                    this.networkId.Content = "";
                }));
                versionString.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new Action(() =>
                {
                    this.versionString.Content = "0";
                }));
                onlineStatus.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new Action(() =>
                {
                    this.onlineStatus.Content = "OFFLINE";
                }));
            }
        }

        private void updateNetworks()
        {
            var networks = APIHandler.Instance.GetNetworks();

            networksPage.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new Action(() =>
            {
                networksPage.setNetworks(networks);
            }));
        }

        private void updatePeers()
        {
            //var peers = handler.GetPeers();

            //peersPage.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new Action(() =>
            //{
            //    peersPage.SetPeers(peers);
            //}));
        }

        private void OnUpdateTimer(object source, ElapsedEventArgs e)
        {
            updateStatus();
            updateNetworks();
            //updatePeers();
        }

        private void joinButton_Click(object sender, RoutedEventArgs e)
        {
            if (joinNetworkID.Text.Length < 16)
            {
                MessageBox.Show("Invalid Network ID");
            }
            else
            {
                APIHandler.Instance.JoinNetwork(joinNetworkID.Text);
            }
        }

        private void OnNetworkEntered(object sender, TextCompositionEventArgs e)
        {
            e.Handled = !charRegex.IsMatch(e.Text);
        }

        private void OnPaste(object sender, DataObjectPastingEventArgs e)
        {
            var isText = e.SourceDataObject.GetDataPresent(DataFormats.UnicodeText, true);
            if (!isText) return;

            var text = e.SourceDataObject.GetData(DataFormats.UnicodeText) as string;

            if (!wholeStringRegex.IsMatch(text))
            {
                e.CancelCommand();
            }
        }
    }
}
