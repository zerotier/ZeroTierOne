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
    /// Interaction logic for EnterToken.xaml
    /// </summary>
    public partial class EnterToken : UserControl, ISwitchable
    {
        public EnterToken()
        {
            InitializeComponent();

            if (!string.IsNullOrEmpty(CentralAPI.Instance.Central.APIKey))
            {
                APITokenInput.Text = CentralAPI.Instance.Central.APIKey;
            }
        }

        public void UtilizeState(object staqte)
        {

        }

        private void Next_Click(object sender, RoutedEventArgs e)
        {
            CentralAPI api = CentralAPI.Instance;

            if (api.Central.APIKey != APITokenInput.Text)
            {
                CentralServer server = new CentralServer();
                server.APIKey = APITokenInput.Text;
                api.Central = server;
            }

            Switcher.Switch(new CreateOrJoin());
        }

        private void BackButton_Click(object sender, RoutedEventArgs e)
        {
            Switcher.Switch(new RegisterOrLogIn());
        }
    }
}
