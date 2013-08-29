using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Linq;
using System.ServiceProcess;
using System.Text;
using System.Threading.Tasks;

namespace Service
{
    public partial class ZeroTierOneService : ServiceBase
    {
        public ZeroTierOneService()
        {
            InitializeComponent();
        }

        protected override void OnStart(string[] args)
        {
        }

        protected override void OnStop()
        {
        }

        private void zeroTierProcess_Exited(object sender, EventArgs e)
        {
        }

        private void zeroTierProcess_ErrorDataReceived(object sender, DataReceivedEventArgs e)
        {
        }

        private void zeroTierProcess_OutputDataReceived(object sender, DataReceivedEventArgs e)
        {
        }

        private void checkForUpdatesTimer_Tick(object sender, EventArgs e)
        {
        }
    }
}
