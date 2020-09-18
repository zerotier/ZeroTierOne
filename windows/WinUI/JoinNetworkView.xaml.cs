using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;

namespace WinUI
{
    /// <summary>
    /// Interaction logic for JoinNetworkView.xaml
    /// </summary>
    public partial class JoinNetworkView : Window
    {
        private Regex charRegex = new Regex("[0-9a-fxA-FX]");
        private Regex wholeStringRegex = new Regex("^[0-9a-fxA-FX]+$");

        public JoinNetworkView()
        {
            InitializeComponent();

            DataObject.AddPastingHandler(joinNetworkBox, onPaste);
            DataObject.AddCopyingHandler(joinNetworkBox, onCopyCut);
        }

        private void joinNetworkBox_OnTextEntered(object sender, TextCompositionEventArgs e)
        {
            e.Handled = !charRegex.IsMatch(e.Text);

            joinButton.IsEnabled = (joinNetworkBox.Text.Length + e.Text.Length) == 16;
        }

        private void joinNetworkBox_OnKeyDown(object sender, KeyEventArgs e)
        {
            if (Keyboard.IsKeyDown(Key.LeftCtrl) || Keyboard.IsKeyDown(Key.RightCtrl))
            {
                if (e.Key == Key.X && joinNetworkBox.IsSelectionActive)
                {
                    // handle ctrl-x removing characters
                    joinButton.IsEnabled = false;
                }
            }
            else if (e.Key == Key.Delete || e.Key == Key.Back)
            {
                joinButton.IsEnabled = (joinNetworkBox.Text.Length - 1) == 16;
            }
            else
            {
                if ((joinNetworkBox.Text.Length + 1) > 16)
                {
                    e.Handled = true;
                }
            }
        }

        private void onPaste(object sender, DataObjectPastingEventArgs e)
        {
            var isText = e.SourceDataObject.GetDataPresent(DataFormats.UnicodeText, true);
            if (!isText)
            {
                joinButton.IsEnabled = false;
                return;
            }

            var text = e.SourceDataObject.GetData(DataFormats.UnicodeText) as string;

            if (!wholeStringRegex.IsMatch(text))
            {
                e.Handled = true;
                e.CancelCommand();
            }

            if (text.Length == 16 || (joinNetworkBox.Text.Length + text.Length) == 16)
            {
                joinButton.IsEnabled = true;
            }
            else if (text.Length > 16 || (joinNetworkBox.Text.Length + text.Length) > 16)
            {
                e.Handled = true;
                e.CancelCommand();
            }
            else
            {
                joinButton.IsEnabled = false;
            }
        }

        private void onCopyCut(object sender, DataObjectCopyingEventArgs e)
        {
        }

        private void joinButton_Click(object sender, RoutedEventArgs e)
        {
            bool allowDefault = allowDefaultCheckbox.IsChecked.Value;
            bool allowGlobal = allowGlobalCheckbox.IsChecked.Value;
            bool allowManaged = allowManagedCheckbox.IsChecked.Value;
            bool allowDNS = allowDNSCheckbox.IsChecked.Value;

            APIHandler.Instance.JoinNetwork(this.Dispatcher, joinNetworkBox.Text, allowManaged, allowGlobal, allowDefault, allowDNS);

            Close();
        }
    }
}
