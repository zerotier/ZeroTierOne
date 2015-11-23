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
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace WinUI
{
    /// <summary>
    /// Interaction logic for NetworksPage.xaml
    /// </summary>
    public partial class NetworksPage : UserControl
    {
        private APIHandler handler;

        public NetworksPage()
        {
            InitializeComponent();
        }

        public void SetAPIHandler(APIHandler handler)
        {
            this.handler = handler;
        }

        public void setNetworks(List<ZeroTierNetwork> networks)
        {
            this.wrapPanel.Children.Clear();
            if (networks == null)
            {
                return;
            }

            for (int i = 0; i < networks.Count; ++i)
            {
                this.wrapPanel.Children.Add(
                    new NetworkInfoView(
                        handler,
                        networks.ElementAt<ZeroTierNetwork>(i)));
            }
        }
    }
}
