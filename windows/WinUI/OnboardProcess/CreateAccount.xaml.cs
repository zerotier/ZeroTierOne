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
    /// Interaction logic for CreateAccount.xaml
    /// </summary>
    public partial class CreateAccount : UserControl, ISwitchable
    {
        public CreateAccount()
        {
            InitializeComponent();
        }

        public void UtilizeState(object state)
        {
            throw new NotImplementedException();
        }

        public void CreateAccount_Click(object sender, RoutedEventArgs e)
        {
            DoCreateAccount();
        }

        public void BackButton_Click(object sender, RoutedEventArgs e)
        {
            Switcher.Switch(new RegisterOrLogIn());
        }

        public async void DoCreateAccount()
        {
            if (PasswordTextBox1.Password.ToString() != PasswordTextBox2.Password.ToString())
            {
                ErrorText.Content = "Passwords do not match!";
            }
            else
            {
                CentralAPI api = CentralAPI.Instance;
                bool accountCreated = await api.Login(EmailAddressTextBox.Text, 
                    PasswordTextBox1.Password.ToString(), true);

                if (accountCreated)
                {
                    Switcher.Switch(new CreateOrJoin());
                }
                else
                {
                    ErrorText.Content = "An error ocurred while creating your account.";
                }
            }
        }
    }
}
