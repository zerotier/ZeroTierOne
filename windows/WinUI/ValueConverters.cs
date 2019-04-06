using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Data;

namespace WinUI
{
    public class IPListConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
        {
            List<string> o = value as List<string>;
            if (o == null)
                return null;

            StringBuilder sb = new StringBuilder();
            for (int i = 0; i < o.Count; i++)
                sb.AppendLine(o[i]);
            return sb.ToString();
        }
        public object ConvertBack(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }

    public class NumberToDateConverter : IValueConverter
    {

        public static DateTimeOffset UnixTimeStampToDateTime(double timeStampMilliseconds)
        {
            // Java timestamp is milliseconds past epoch
            return new DateTimeOffset(1970, 1, 1, 0, 0, 0, 0, TimeSpan.FromMilliseconds(timeStampMilliseconds));
        }
        public object Convert(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
        {
            ulong o = (ulong)value;
            var c = UnixTimeStampToDateTime((long)o);
            var d = c.LocalDateTime;
            return $"{d.ToLongDateString()} - {d.ToLongTimeString()}";
        }
        public object ConvertBack(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }

    public class NumberToDifferenceConverter : IValueConverter
    {
        public static string DoConvert(ulong o)
        {
            var c = NumberToDateConverter.UnixTimeStampToDateTime((long)o);
            var d = c.LocalDateTime;
            var diff = DateTime.Now - d;

            StringBuilder sb = new StringBuilder();
            if (diff.Days >= 1)
                sb.Append($"{diff.Days}d ");
            if (diff.TotalHours >= 1)
                sb.Append($"{diff.Hours}h ");
            if (diff.TotalMinutes >= 1)
                sb.Append($"{diff.Minutes}m ");
            if (sb.Length == 0)
                sb.Append($"{diff.Seconds}s");
            return sb.ToString();
        }
        public object Convert(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
        {
            ulong o = (ulong)value;
            return DoConvert(o);
        }
        public object ConvertBack(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }

    public class OnlinePresenceToStringConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
        {
            var oclass = value as CentralMember.OnlinePresenceClass;
            if (oclass.Online)
                return "ONLINE";
            return NumberToDifferenceConverter.DoConvert(oclass.LastOnline);
        }
        public object ConvertBack(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }

    public abstract class BoolToColorConverter : IValueConverter
    {
        protected System.Windows.Media.Brush IfTrue;
        protected System.Windows.Media.Brush IfFalse;
        public object Convert(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
        {
            var oclass = (bool)value;
            return oclass ? IfTrue : IfFalse;
        }
        public object ConvertBack(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }
    public class OnlinePresenceToColorConverter : BoolToColorConverter
    {
        public OnlinePresenceToColorConverter()
        {
            IfTrue = new System.Windows.Media.SolidColorBrush(System.Windows.Media.Colors.Green);
            IfFalse = new System.Windows.Media.SolidColorBrush(System.Windows.Media.Colors.Red);
        }
    }

}
