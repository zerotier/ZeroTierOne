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
    public partial class MainWindow : Window
    {
        APIHandler handler;
        Regex charRegex = new Regex("[0-9a-fxA-FX]");
        Regex wholeStringRegex = new Regex("^[0-9a-fxA-FX]+$");

        Timer timer = new Timer();

        bool connected = false;

        public MainWindow()
        {
            InitializeComponent();

            if (InitAPIHandler())
            {
                networksPage.SetAPIHandler(handler);

                updateStatus();
                if (!connected)
                {
                    MessageBox.Show("Unable to connect to ZeroTier Service.");
                }

                updateNetworks();
                //updatePeers();

                DataObject.AddPastingHandler(joinNetworkID, OnPaste);

                timer.Elapsed += new ElapsedEventHandler(OnUpdateTimer);
                timer.Interval = 2000;
                timer.Enabled = true;
            }
        }


        private String readAuthToken(String path)
        {
            String authToken = "";

            if (File.Exists(path))
            {
                try
                {
                    byte[] tmp = File.ReadAllBytes(path);
                    authToken = System.Text.Encoding.UTF8.GetString(tmp).Trim();
                }
                catch
                {
                    MessageBox.Show("Unable to read ZeroTier One Auth Token from:\r\n" + path, "ZeroTier One");
                }
            }

            return authToken;
        }

        private Int32 readPort(String path)
        {
            Int32 port = 9993;

            try
            {
                byte[] tmp = File.ReadAllBytes(path);
                port = Int32.Parse(System.Text.Encoding.ASCII.GetString(tmp).Trim());
                if ((port <= 0) || (port > 65535))
                    port = 9993;
            }
            catch
            {
            }

            return port;
        }

        private bool InitAPIHandler()
        {
            String localZtDir = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData) + "\\ZeroTier\\One";
            String globalZtDir = Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData) + "\\ZeroTier\\One";

            String authToken = "";
            Int32 port = 9993;

            if (!File.Exists(localZtDir + "\\authtoken.secret") || !File.Exists(localZtDir + "\\zerotier-one.port"))
            {
                // launch external process to copy file into place
                String curPath = System.Reflection.Assembly.GetEntryAssembly().Location;
                int index = curPath.LastIndexOf("\\");
                curPath = curPath.Substring(0, index);
                ProcessStartInfo startInfo = new ProcessStartInfo(curPath + "\\copyutil.exe", globalZtDir + " " + localZtDir);
                startInfo.Verb = "runas";


                var process = Process.Start(startInfo);
                process.WaitForExit();
            }

            authToken = readAuthToken(localZtDir + "\\authtoken.secret");

            if ((authToken == null) || (authToken.Length <= 0))
            {
                MessageBox.Show("Unable to read ZeroTier One authtoken", "ZeroTier One");
                this.Close();
                return false;
            }

            port = readPort(localZtDir + "\\zerotier-one.port");
            handler = new APIHandler(port, authToken);
            return true;
        }

        private void updateStatus()
        {
            var status = handler.GetStatus();

            if (status != null)
            {
                connected = true;

                networkId.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new Action(() =>
                {
                    this.networkId.Text = status.Address;
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
                    this.networkId.Text = "";
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
            var networks = handler.GetNetworks();

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
                handler.JoinNetwork(joinNetworkID.Text);
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
