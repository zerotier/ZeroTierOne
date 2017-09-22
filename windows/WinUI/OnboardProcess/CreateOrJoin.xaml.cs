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

namespace WinUI.OnboardProcess
{
    /// <summary>
    /// Interaction logic for CreateOrJoin.xaml
    /// </summary>
    public partial class CreateOrJoin : UserControl, ISwitchable
    {
        private List<CentralNetwork> networkList = new List<CentralNetwork>();

        public CreateOrJoin()
        {
            InitializeComponent();
            listViewDataBinding.ItemsSource = networkList;

            GetAvailableNetworks();
        }

        public void UtilizeState(object state)
        {
            throw new NotImplementedException();
        }

        private async void GetAvailableNetworks()
        {
            CentralAPI api = CentralAPI.Instance;

            List<CentralNetwork> networks = await api.GetNetworkList();

            foreach (CentralNetwork n in networks)
            {
                networkList.Add(n);
            }

            listViewDataBinding.Items.Refresh();
        }

        public void OnJoinButtonClick(object sender, RoutedEventArgs e)
        {
            Button button = sender as Button;
            string networkId = button.Tag as string;

            APIHandler handler = APIHandler.Instance;

            handler.JoinNetwork(this.Dispatcher, networkId);

            AuthorizeNetworkMember(networkId);
        }

        public void OnCreateButtonClick(object sender, RoutedEventArgs e)
        {
            CreateNewNetwork();
        }

        private async void CreateNewNetwork()
        {
            CentralAPI api = CentralAPI.Instance;

            CentralNetwork newNetwork = await api.CreateNewNetwork();

            APIHandler handler = APIHandler.Instance;

            handler.JoinNetwork(this.Dispatcher, newNetwork.Id);

            AuthorizeNetworkMember(newNetwork.Id);
        }

        private async void AuthorizeNetworkMember(string networkId)
        {
            string nodeId = APIHandler.Instance.NodeAddress();

            bool authorized = await CentralAPI.Instance.AuthorizeNode(nodeId, networkId);

            if (authorized)
            {
                Switcher.Switch(new Finished());
            }
            else
            {

            }
        }
    }
}
