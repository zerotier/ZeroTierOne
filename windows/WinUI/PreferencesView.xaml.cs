using Microsoft.Win32;
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
    /// Interaction logic for PreferencesView.xaml
    /// </summary>
    public partial class PreferencesView : Window
    {
        public static string AppName = "ZeroTier One";
        private RegistryKey rk = Registry.CurrentUser.OpenSubKey("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", true);
        private string AppLocation = System.Reflection.Assembly.GetExecutingAssembly().Location;
        public PreferencesView()
        {
            InitializeComponent();


            string keyValue = rk.GetValue(AppName) as string;

            if (keyValue != null && keyValue.Equals(AppLocation))
            {
                startupCheckbox.IsChecked = true;
            }

            CentralAPI api = CentralAPI.Instance;
            CentralInstanceTextBox.Text = api.Central.ServerURL;
            APIKeyTextBox.Text = api.Central.APIKey;
        }

        private void OKButton_Clicked(object sender, RoutedEventArgs e)
        {
            CentralAPI api = CentralAPI.Instance;

            if (api.Central.ServerURL != CentralInstanceTextBox.Text ||
                api.Central.APIKey != APIKeyTextBox.Text)
            {
                CentralServer newServer = new CentralServer();
                newServer.ServerURL = CentralInstanceTextBox.Text;
                newServer.APIKey = APIKeyTextBox.Text;

                api.Central = newServer;
            }

            if (startupCheckbox.IsChecked.HasValue && (bool)startupCheckbox.IsChecked)
            {
                rk.SetValue(AppName, AppLocation);
            }
            else
            {
                string keyValue = rk.GetValue(AppName) as string;

                if (keyValue != null && keyValue.Equals(AppLocation))
                {
                    rk.DeleteValue(AppName);
                }
            }

            Close();
        }
    }
}
