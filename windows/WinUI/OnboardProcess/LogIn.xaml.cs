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
    /// Interaction logic for LogIn.xaml
    /// </summary>
    public partial class LogIn : UserControl, ISwitchable
    {
        public LogIn()
        {
            InitializeComponent();
        }

        public void UtilizeState(object state)
        {
            throw new NotImplementedException();
        }

        public void LoginButton_Click(object sender, RoutedEventArgs e)
        {
            DoLogin();
        }

        public void BackButton_Click(object sender, RoutedEventArgs e)
        {
            Switcher.Switch(new RegisterOrLogIn());
        }

        private async void DoLogin()
        {
            CentralAPI api = CentralAPI.Instance;
            bool didLogIn = await api.Login(EmailAddressTextBox.Text, PasswordTextBox.Password.ToString(), false);
            if (didLogIn)
            {
                Switcher.Switch(new CreateOrJoin());
            }
            else
            {
                ErrorText.Content = "Invalid username or password";
            }
        }
    }
}
