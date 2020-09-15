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
        public NetworksPage()
        {
            InitializeComponent();
        }

        public void setNetworks(List<ZeroTierNetwork> networks)
        {
            if (networks == null)
            {
                this.wrapPanel.Children.Clear();
                return;
            }

            foreach (ZeroTierNetwork network in networks)
            {
                NetworkInfoView view = ChildWithNetwork(network);
                if (view != null)
                {
                    view.SetNetworkInfo(network);
                }
                else
                {
                    wrapPanel.Children.Add(
                        new NetworkInfoView(
                            network));
                }
            }

            // remove networks we're no longer joined to.
            List<ZeroTierNetwork> tmpList = GetNetworksFromChildren();
            foreach (ZeroTierNetwork n in networks)
            {
                if (tmpList.Contains(n))
                {
                    tmpList.Remove(n);
                }
            }

            foreach (ZeroTierNetwork n in tmpList)
            {
                NetworkInfoView view = ChildWithNetwork(n);
                if (view != null)
                {
                    wrapPanel.Children.Remove(view);
                }
            }
        }

        private NetworkInfoView ChildWithNetwork(ZeroTierNetwork network)
        {
            List<NetworkInfoView> list = wrapPanel.Children.OfType<NetworkInfoView>().ToList();

            foreach (NetworkInfoView view in list)
            {
                if (view.HasNetwork(network))
                {
                    return view;
                }
            }

            return null;
        }

        private List<ZeroTierNetwork> GetNetworksFromChildren()
        {
            List<ZeroTierNetwork> networks = new List<ZeroTierNetwork>(wrapPanel.Children.Count);

            List<NetworkInfoView> list = wrapPanel.Children.OfType<NetworkInfoView>().ToList();
            foreach (NetworkInfoView n in list)
            {
                networks.Add(n.network);
            }

            return networks;
        }
    }
}
