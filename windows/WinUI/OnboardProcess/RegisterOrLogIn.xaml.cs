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
    /// Interaction logic for RegisterOrLogIn.xaml
    /// </summary>
    public partial class RegisterOrLogIn : UserControl, ISwitchable
    {
        public RegisterOrLogIn()
        {
            InitializeComponent();
        }

        public void UtilizeState(object state)
        {
            throw new NotImplementedException();
        }

        public void CreateAccountButton_Click(object sender, System.Windows.RoutedEventArgs e)
        {
            Switcher.Switch(new CreateAccount());
        }

        private void LogInButton_Click(object sender, RoutedEventArgs e)
        {
            Switcher.Switch(new LogIn());
        }

        public void APIToken_Click(object sender, RoutedEventArgs e)
        {
            Switcher.Switch(new EnterToken());
        }
    }
}
