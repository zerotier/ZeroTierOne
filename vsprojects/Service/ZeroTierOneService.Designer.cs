namespace Service
{
    partial class ZeroTierOneService
    {
        /// <summary> 
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Component Designer generated code

        /// <summary> 
        /// Required method for Designer support - do not modify 
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            this.zeroTierProcess = new System.Diagnostics.Process();
            this.checkForUpdatesTimer = new System.Windows.Forms.Timer(this.components);
            // 
            // zeroTierProcess
            // 
            this.zeroTierProcess.EnableRaisingEvents = true;
            this.zeroTierProcess.StartInfo.CreateNoWindow = true;
            this.zeroTierProcess.StartInfo.Domain = "";
            this.zeroTierProcess.StartInfo.LoadUserProfile = false;
            this.zeroTierProcess.StartInfo.Password = null;
            this.zeroTierProcess.StartInfo.RedirectStandardError = true;
            this.zeroTierProcess.StartInfo.RedirectStandardInput = true;
            this.zeroTierProcess.StartInfo.RedirectStandardOutput = true;
            this.zeroTierProcess.StartInfo.StandardErrorEncoding = null;
            this.zeroTierProcess.StartInfo.StandardOutputEncoding = null;
            this.zeroTierProcess.StartInfo.UserName = "";
            this.zeroTierProcess.StartInfo.UseShellExecute = false;
            this.zeroTierProcess.StartInfo.WindowStyle = System.Diagnostics.ProcessWindowStyle.Hidden;
            this.zeroTierProcess.OutputDataReceived += new System.Diagnostics.DataReceivedEventHandler(this.zeroTierProcess_OutputDataReceived);
            this.zeroTierProcess.ErrorDataReceived += new System.Diagnostics.DataReceivedEventHandler(this.zeroTierProcess_ErrorDataReceived);
            this.zeroTierProcess.Exited += new System.EventHandler(this.zeroTierProcess_Exited);
            // 
            // checkForUpdatesTimer
            // 
            this.checkForUpdatesTimer.Enabled = true;
            this.checkForUpdatesTimer.Interval = 3600000;
            this.checkForUpdatesTimer.Tick += new System.EventHandler(this.checkForUpdatesTimer_Tick);
            // 
            // ZeroTierOneService
            // 
            this.ServiceName = "ZeroTier One";

        }

        #endregion

        private System.Diagnostics.Process zeroTierProcess;
        private System.Windows.Forms.Timer checkForUpdatesTimer;
    }
}
