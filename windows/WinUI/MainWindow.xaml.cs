using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
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
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        APIHandler handler = new APIHandler();
        Regex charRegex = new Regex("[0-9a-fxA-FX]");
        Regex wholeStringRegex = new Regex("^[0-9a-fxA-FX]+$");

        public MainWindow()
        {
            InitializeComponent();

            updateStatus();
            updateNetworks();

            DataObject.AddPastingHandler(joinNetworkID, OnPaste);
        }

        private void updateStatus()
        {
            var status = handler.GetStatus();

            this.networkId.Content = status.Address;
            this.versionString.Content = status.Version;
            this.onlineStatus.Content = (status.Online ? "ONLINE" : "OFFLINE");
        }

        private void updateNetworks()
        {
            var networks = handler.GetNetworks();

            networksPage.setNetworks(networks);
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
