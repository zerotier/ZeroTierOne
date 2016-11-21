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
using System.ComponentModel;

namespace WinUI
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class NetworkListView : Window
    {
        Regex charRegex = new Regex("[0-9a-fxA-FX]");
        Regex wholeStringRegex = new Regex("^[0-9a-fxA-FX]+$");

        public NetworkListView()
        {
            InitializeComponent();

            Closed += onClosed;

            NetworkMonitor.Instance.SubscribeNetworkUpdates(updateNetworks);
        }

        ~NetworkListView()
        {
        }

        protected override void OnClosing(CancelEventArgs e)
        {
            e.Cancel = true;
            Hide();
        }

        private void onClosed(object sender, System.EventArgs e)
        {
            NetworkMonitor.Instance.UnsubscribeNetworkUpdates(updateNetworks);
        }

        private void updateNetworks(List<ZeroTierNetwork> networks)
        {
            if (networks != null)
            {
                networksPage.Dispatcher.BeginInvoke(DispatcherPriority.Normal, new Action(() =>
                {
                    networksPage.setNetworks(networks);
                }));
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
