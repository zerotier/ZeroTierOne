using System;
using System.IO;
using System.Collections.Generic;
using System.Data;
using System.Diagnostics;
using System.ServiceProcess;
using System.Threading;

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
            this.ztKiller = null;
        }

        protected override void OnStart(string[] args)
        {
            startZeroTierDaemon();
        }

        protected override void OnStop()
        {
            stopZeroTierDaemon();
        }

        private void startZeroTierDaemon()
        {
            if (ztService != null)
                return;
            ztService = new Process();
            try
            {
                ztService.StartInfo.UseShellExecute = false;
                ztService.StartInfo.FileName = ztBinary;
                ztService.StartInfo.Arguments = "";
                ztService.StartInfo.CreateNoWindow = true;
                ztService.Exited += ztService_Exited;
                ztService.Start();
            }
            catch (Exception e)
            {
                Console.WriteLine(e.ToString());
                ztService = null;
            }
        }

        private void stopZeroTierDaemon()
        {
            while (ztKiller != null)
                Thread.Sleep(250);

            ztKiller = new Process();
            try
            {
                ztKiller.StartInfo.UseShellExecute = false;
                ztKiller.StartInfo.FileName = ztBinary;
                ztKiller.StartInfo.Arguments = "-q terminate ServiceShutdown";
                ztKiller.StartInfo.CreateNoWindow = true;
                ztKiller.Exited += ztKiller_Exited;
                ztKiller.Start();
            }
            catch (Exception e)
            {
                ztKiller = null;
            }

            int waited = 0;
            while (ztKiller != null)
            {
                Thread.Sleep(250);
                if (++waited > 100)
                    break;
            }

            if (ztService != null)
            {
                ztService.Kill();
                ztService = null;
            }
        }

        // Event generated when ztService exits
        private void ztService_Exited(object sender, System.EventArgs e)
        {
            ztService = null;
        }

        // Event generated when ztKiller is done
        private void ztKiller_Exited(object sender, System.EventArgs e)
        {
            ztKiller = null;
        }

        private string ztHome;
        private string ztUpdatesFolder;
        private string ztBinary;

        private volatile Process ztService;
        private volatile Process ztKiller;
    }
}
