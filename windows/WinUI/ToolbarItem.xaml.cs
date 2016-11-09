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

        NetworkListView netList = null;

        public ToolbarItem()
        {
            InitializeComponent();
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
            if (netList == null)
            {
                netList = new WinUI.NetworkListView();
                netList.Closed += ShowNetworksClosed;
                netList.Show();
            }
        }

        private void ShowNetworksClosed(object sender, System.EventArgs e)
        {
            netList = null;
        }
    }
}
