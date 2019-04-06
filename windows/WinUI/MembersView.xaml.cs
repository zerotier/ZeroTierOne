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

namespace WinUI
{
    /// <summary>
    /// Interaction logic for MembersView.xaml
    /// </summary>
    public partial class MembersView : Window
    {
        public MembersView(string nid)
        {
            InitializeComponent();
            network = nid;
            this.Loaded += MembersView_Loaded;
        }

        private void MembersView_Loaded(object sender, RoutedEventArgs e)
        {
            GetContent().ConfigureAwait(false);
        }

        string network;

        private async Task GetContent()
        {
            var members = await CentralAPI.Instance.GetMembersList(network);
            lstMembers.ItemsSource = members;
        }
    }
}
