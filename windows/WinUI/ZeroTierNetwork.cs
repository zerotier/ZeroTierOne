using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WinUI
{
    public class ZeroTierNetwork
    {
        public string nwid;
        public string mac;
        public string name;
        public string status;
        public string type;
        public int mtu;
        public bool dhcp;
        public bool bridge;
        public bool broadcastEnabled;
        public int portError;
        public int netconfRevision;
        public string[] multicastSubscriptions;
        public string[] assignedAddresses;
        public string portDeviceName;
    }
}
