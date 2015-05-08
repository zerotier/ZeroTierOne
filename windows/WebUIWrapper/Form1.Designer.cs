namespace WebUIWrapper
{
    partial class Form1
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

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(Form1));
            this.webContainer = new System.Windows.Forms.WebBrowser();
            this.SuspendLayout();
            // 
            // webContainer
            // 
            this.webContainer.AllowNavigation = false;
            this.webContainer.AllowWebBrowserDrop = false;
            this.webContainer.Dock = System.Windows.Forms.DockStyle.Fill;
            this.webContainer.IsWebBrowserContextMenuEnabled = false;
            this.webContainer.Location = new System.Drawing.Point(0, 0);
            this.webContainer.MinimumSize = new System.Drawing.Size(20, 20);
            this.webContainer.Name = "webContainer";
            this.webContainer.ScriptErrorsSuppressed = true;
            this.webContainer.ScrollBarsEnabled = false;
            this.webContainer.Size = new System.Drawing.Size(1012, 556);
            this.webContainer.TabIndex = 0;
            this.webContainer.Url = new System.Uri("", System.UriKind.Relative);
            this.webContainer.WebBrowserShortcutsEnabled = false;
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(9F, 20F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(1012, 556);
            this.Controls.Add(this.webContainer);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "Form1";
            this.Text = "ZeroTier One";
            this.Load += new System.EventHandler(this.Form1_Load);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.WebBrowser webContainer;
    }
}

