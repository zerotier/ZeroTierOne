using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;
using Hardcodet.Wpf.TaskbarNotification;

namespace WinUI
{
    /// <summary>
    /// Interaction logic for App.xaml
    /// </summary>
    public partial class App : Application
    {
        private TaskbarIcon tb;

        //TODO: this has 0 references right now
        internal void InitApplication()
        {
            tb = (TaskbarIcon)FindResource("NotifyIcon");
            tb.Visibility = Visibility.Visible;
        }
    }
}
