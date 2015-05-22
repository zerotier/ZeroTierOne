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
            this.webContainer.Dock = System.Windows.Forms.DockStyle.Fill;
            this.webContainer.Location = new System.Drawing.Point(0, 0);
            this.webContainer.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.webContainer.MinimumSize = new System.Drawing.Size(18, 16);
            this.webContainer.Name = "webContainer";
            this.webContainer.Size = new System.Drawing.Size(900, 445);
            this.webContainer.TabIndex = 0;
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(8F, 16F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(900, 445);
            this.Controls.Add(this.webContainer);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Margin = new System.Windows.Forms.Padding(3, 2, 3, 2);
            this.Name = "Form1";
            this.Text = "ZeroTier One";
            this.Load += new System.EventHandler(this.Form1_Load);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.WebBrowser webContainer;
    }
}

