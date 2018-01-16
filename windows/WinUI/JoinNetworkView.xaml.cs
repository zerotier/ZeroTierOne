using System;
using System.Collections.Generic;
using System.ComponentModel;
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
using System.Windows.Shapes;

namespace WinUI
{
    /// <summary>
    /// Interaction logic for JoinNetworkView.xaml
    /// </summary>
    public partial class JoinNetworkView : Window, INotifyPropertyChanged
    {
        Regex wholeStringRegex = new Regex("^[0-9a-fxA-FX]+$");

        public JoinNetworkView()
        {
            InitializeComponent();
        }

        public event PropertyChangedEventHandler PropertyChanged;

        private string networkID = "";

        public string NetworkID
        {
            get
            {
                return networkID;
            }
            set
            {
                networkID = value;
                JoinEnabled = networkID.Length == 16 && wholeStringRegex.IsMatch(networkID);
            }
        }
        public bool AllowDefault { get; set; } = false;
        public bool AllowGlobal { get; set; } = false;
        public bool AllowManaged { get; set; } = true;
        public bool JoinEnabled { get; set; } = false;

        private void joinButton_Click(object sender, RoutedEventArgs e)
        {
            APIHandler.Instance.JoinNetwork(Dispatcher, joinNetworkBox.Text, AllowManaged, AllowGlobal, AllowDefault);

            Close();
        }
    }
}
