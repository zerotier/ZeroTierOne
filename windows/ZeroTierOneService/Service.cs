using System;
using System.IO;
using System.Collections.Generic;
using System.Data;
using System.Diagnostics;
using System.ServiceProcess;

namespace ZeroTierOneService
{
    public partial class Service : ServiceBase
    {
        public Service()
        {
            InitializeComponent();

            this.ztHome = Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData) + Path.DirectorySeparatorChar + "ZeroTier" + Path.DirectorySeparatorChar + "One";
            this.ztUpdatesFolder = this.ztHome + Path.DirectorySeparatorChar + "updates.d";
            this.ztBinary = this.ztHome + Path.DirectorySeparatorChar + (Environment.Is64BitOperatingSystem ? "zerotier-one_x64.exe" : "zerotier-one_x86.exe");

            this.ztService = null;
        }

        protected override void OnStart(string[] args)
        {
            startZeroTierService();
        }

        protected override void OnStop()
        {
            stopZeroTierService();
        }

        private void startZeroTierService()
        {
        }

        private void stopZeroTierService()
        {
            if (ztService != null)
            {
                ztService.Kill();
                ztService = null;
            }
        }

        private void ztService_Exited(object sender, System.EventArgs e)
        {
            ztService = null;
        }

        private string ztHome;
        private string ztUpdatesFolder;
        private string ztBinary;

        private Process ztService;
    }
}
