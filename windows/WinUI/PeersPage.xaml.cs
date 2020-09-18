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
    /// Interaction logic for PeersPage.xaml
    /// </summary>
    public partial class PeersPage : UserControl
    {
        private List<ZeroTierPeer> peersList = new List<ZeroTierPeer>();

        public PeersPage()
        {
            InitializeComponent();

            dataGrid.ItemsSource = peersList;
        }

        public void SetPeers(List<ZeroTierPeer> list)
        {
            if (list == null)
                return;

            foreach (ZeroTierPeer p in list)
            {
                ZeroTierPeer curPeer = peersList.Find(peer => peer.Equals(p));
                if (curPeer == null)
                {
                    peersList.Add(p);
                }
                else
                {
                    curPeer.Update(p);
                }
            }

            dataGrid.Items.Refresh();
        }
    }
}
