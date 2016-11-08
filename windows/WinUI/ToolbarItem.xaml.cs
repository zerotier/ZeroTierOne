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
using System.Text.RegularExpressions;
using System.Timers;
using System.Windows.Threading;
using System.IO;
using System.Diagnostics;

namespace WinUI
{
    /// <summary>
    /// Interaction logic for ToolbarItem.xaml
    /// </summary>
    public partial class ToolbarItem : Window
    {
        APIHandler handler;

        public ToolbarItem()
        {
            InitializeComponent();

            if (InitAPIHandler())
            {

            }
            else
            {
                MessageBox.Show("ZeroTier API Initialization Failed");
            }
        }

        private String readAuthToken(String path)
        {
            String authToken = "";

            if (File.Exists(path))
            {
                try
                {
                    byte[] tmp = File.ReadAllBytes(path);
                    authToken = System.Text.Encoding.UTF8.GetString(tmp).Trim();
                }
                catch
                {
                    MessageBox.Show("Unable to read ZeroTier One Auth Token from:\r\n" + path, "ZeroTier One");
                }
            }

            return authToken;
        }

        private Int32 readPort(String path)
        {
            Int32 port = 9993;

            try
            {
                byte[] tmp = File.ReadAllBytes(path);
                port = Int32.Parse(System.Text.Encoding.ASCII.GetString(tmp).Trim());
                if ((port <= 0) || (port > 65535))
                    port = 9993;
            }
            catch
            {
            }

            return port;
        }

        private bool InitAPIHandler()
        {
            String localZtDir = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData) + "\\ZeroTier\\One";
            String globalZtDir = Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData) + "\\ZeroTier\\One";

            String authToken = "";
            Int32 port = 9993;

            if (!File.Exists(localZtDir + "\\authtoken.secret") || !File.Exists(localZtDir + "\\zerotier-one.port"))
            {
                // launch external process to copy file into place
                String curPath = System.Reflection.Assembly.GetEntryAssembly().Location;
                int index = curPath.LastIndexOf("\\");
                curPath = curPath.Substring(0, index);
                ProcessStartInfo startInfo = new ProcessStartInfo(curPath + "\\copyutil.exe", globalZtDir + " " + localZtDir);
                startInfo.Verb = "runas";


                var process = Process.Start(startInfo);
                process.WaitForExit();
            }

            authToken = readAuthToken(localZtDir + "\\authtoken.secret");

            if ((authToken == null) || (authToken.Length <= 0))
            {
                MessageBox.Show("Unable to read ZeroTier One authtoken", "ZeroTier One");
                this.Close();
                return false;
            }

            port = readPort(localZtDir + "\\zerotier-one.port");
            handler = new APIHandler(port, authToken);
            return true;
        }

        private void ToolbarItem_TrayContextMenuOpen(object sender, System.Windows.RoutedEventArgs e)
        {
            Console.WriteLine("TrayContextMenuOpen");
        }

        private void ToolbarItem_PreviewTrayContextMenuOpen(object sender, System.Windows.RoutedEventArgs e)
        {
            Console.WriteLine("PreviewTrayContextMenuOpen");
        }
    }
}
