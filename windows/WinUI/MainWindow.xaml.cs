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

        public MainWindow()
        {
            InitializeComponent();

            updateStatus();
            updateNetworks();
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

        }

        private void OnNetworkEntered(object sender, TextCompositionEventArgs e)
        {
            Regex regex = new Regex("[0-9a-fxA-FX]");
            e.Handled = !regex.IsMatch(e.Text);
        }
    }
}
