using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO;
using System.Net;
using System.Net.Sockets;

namespace WebUIWrapper
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            String ztDir = Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData) + "\\ZeroTier\\One";
            String authToken = "";
            Int32 port = 9993;
            try
            {
                byte[] tmp = File.ReadAllBytes(ztDir + "\\authtoken.secret");
                authToken = System.Text.Encoding.ASCII.GetString(tmp).Trim();
            } catch {
                MessageBox.Show("Unable to read ZeroTier One authtoken.secret from:\r\n" + ztDir,"ZeroTier One");
                this.Close();
            }
            if ((authToken == null)||(authToken.Length <= 0))
            {
                MessageBox.Show("Unable to read ZeroTier One authtoken.secret from:\r\n" + ztDir, "ZeroTier One");
                this.Close();
            }
            try
            {
                byte[] tmp = File.ReadAllBytes(ztDir + "\\zerotier-one.port");
                port = Int32.Parse(System.Text.Encoding.ASCII.GetString(tmp).Trim());
                if ((port <= 0) || (port > 65535))
                    port = 9993;
            }
            catch
            {
            }
            try
            {
                TcpClient tc = new TcpClient();
                try
                {
                    tc.Connect("127.0.0.1", port);
                    tc.Close();
                }
                catch
                {
                    MessageBox.Show("ZeroTier One service does not appear to be running at local port " + port.ToString(),"ZeroTier One");
                    this.Close();
                    return;
                }
                webContainer.Url = new System.Uri("http://127.0.0.1:" + port.ToString() + "/index.html?authToken=" + authToken);
            }
            catch
            {
                MessageBox.Show("Unable to open service control panel.", "ZeroTier One");
                this.Close();
            }
        }
    }
}
