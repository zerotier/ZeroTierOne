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
using System.Windows.Threading;

namespace WinUI
{
    /// <summary>
    /// Interaction logic for FloatingWindow.xaml
    /// </summary>
    public partial class FloatingWindow : Window
    {
        bool registered = false;
        public FloatingWindow()
        {
            InitializeComponent();
            this.Loaded += MyNetworksView_Loaded;
            this.Closing += FloatingWindow_Closing;
        }

        private void FloatingWindow_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            if (registered)
            {
                CentralNetworkMonitor.Instance.UnsubscribeNetworkUpdates(DoUpdate);
                registered = false;
            }
        }

        private void MyNetworksView_Loaded(object sender, RoutedEventArgs e)
        {
            if (!registered)
            {
                CentralNetworkMonitor.Instance.SubscribeNetworkUpdates(DoUpdate);
                registered = true;
            }
        }

        void DoUpdate(List<CentralNetwork> list)
        {
            Dispatcher.BeginInvoke(DispatcherPriority.Normal, new Action(() =>
            {
                treeViewer.ItemsSource = null;
                treeViewer.ItemsSource = list;
            }));
        }

    }
}
