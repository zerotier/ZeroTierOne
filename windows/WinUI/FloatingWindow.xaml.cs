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
using System.Windows.Threading;

namespace WinUI
{
    /// <summary>
    /// Interaction logic for FloatingWindow.xaml
    /// </summary>
    public partial class FloatingWindow : Window
    {
        bool registered = false;
        public FloatingWindow()
        {
            InitializeComponent();
            this.Loaded += MyNetworksView_Loaded;
            this.Closing += FloatingWindow_Closing;
            SetLoadingMessageVisibility(true);
        }

        private void FloatingWindow_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            if (registered)
            {
                CentralNetworkMonitor.Instance.UnsubscribeNetworkUpdates(DoUpdate);
                registered = false;
            }
        }

        private void MyNetworksView_Loaded(object sender, RoutedEventArgs e)
        {
            if (!registered)
            {
                CentralNetworkMonitor.Instance.SubscribeNetworkUpdates(DoUpdate);
                registered = true;
            }
        }
        bool Equal(List<CentralNetwork> list)
        {
            if (_datasource == null)
                return false;
            if (list.Count != _datasource.Count)
                return false;
            for (int i = 0; i < list.Count; i++)
            {
                if (!list[i].Equals(_datasource[i]))
                    return false;

                if (list[i].Members.Count != _datasource[i].Members.Count)
                    return false;

                for (int j = 0; j < list[i].Members.Count; j++)
                {
                    if (!list[i].Members[j].Equals(_datasource[i].Members[j]))
                        return false;
                }
            }
            return true;
        }
        List<CentralNetwork> _datasource;
        void DoUpdate(List<CentralNetwork> list)
        {

            if (Equal(list))
                return;
            if (_datasource == null)
                _datasource = new List<CentralNetwork>();
            Dispatcher.BeginInvoke(DispatcherPriority.Normal, new Action(() =>
            {
                treeViewer.ItemsSource = null;
                _datasource.Clear();
                foreach (var n in list)
                    _datasource.Add(CentralNetwork.CopyFrom(n));
                foreach (var d in _datasource)
                    foreach (var m in d.Members)
                        CentralMemberVM.Populate(m);

                treeViewer.ItemsSource = _datasource;
                if (_datasource.Count > 0)
                {
                    SetLoadingMessageVisibility(false);
                    ExpandFirstLevel();
                }

            }));
        }

        private void SetLoadingMessageVisibility(bool show)
        {
            LoadingMsg.Visibility = show ? Visibility.Visible : Visibility.Collapsed;
        }

        #region Custom window handling

        /// <summary>
        /// CloseButton_Clicked
        /// </summary>
        private void CloseButton_Click(object sender, RoutedEventArgs e)
        {
            this.Close();
        }

        /// <summary>
        /// Minimized Button_Clicked
        /// </summary>
        private void MinimizeButton_Click(object sender, RoutedEventArgs e)
        {
            this.WindowState = WindowState.Minimized;
        }


        #endregion

        public void ExpandFirstLevel()
        {
            for (int i = 0; i < treeViewer.Items.Count; i++)
            {
                var t = treeViewer.ItemContainerGenerator.ContainerFromIndex(i) as TreeViewItem;
                t.IsExpanded = true;
            }
        }

        private void Member_MouseRightButtonDown(object sender,
            MouseButtonEventArgs e)
        {
            var item = (sender as TreeView).SelectedItem as CentralMember;

            if (item != null)
            {
                var pMenu = (ContextMenu)Resources["itemMenu"];
                pMenu.DataContext = item;
                pMenu.IsOpen = true;
            }
        }

        private void MenuItemCopyIP_Click(object sender, RoutedEventArgs e)
        {
            string ip = GetIP(sender);
            if (ip != null)
                Clipboard.SetText(ip);
        }
        string GetIP(object sender)
        {
            var item = ((sender as MenuItem).Parent as ContextMenu).DataContext as CentralMember;
            if (item != null)
                if ((item.Config.IpAssignments != null) &&
                    (item.Config.IpAssignments.Count > 0))
                    return item.Config.IpAssignments[0];
            return null;
        }
        private void MenuItemPing_Click(object sender, RoutedEventArgs e)
        {
            string ip = GetIP(sender);
            if (ip != null)
            {
                string cmd = $"-t {ip}";
                System.Diagnostics.Process.Start("ping", cmd);
            }
        }
    }
}
