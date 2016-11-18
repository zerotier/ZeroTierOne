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

        public PreferencesView()
        {
            InitializeComponent();


            string keyValue = rk.GetValue(AppName) as string;

            if (keyValue != null && keyValue.Equals(System.Reflection.Assembly.GetExecutingAssembly().Location))
            {
                startupCheckbox.IsChecked = true;
            }
        }

        private void startupCheckbox_Checked(object sender, RoutedEventArgs e)
        {
            rk.SetValue(AppName, System.Reflection.Assembly.GetExecutingAssembly().Location);
        }

        private void startupCheckbox_Unchecked(object sender, RoutedEventArgs e)
        {
            rk.DeleteValue(AppName);
        }

    }
}
