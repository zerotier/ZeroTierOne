using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WinUI
{
    public class ZeroTierStatus
    {
        public string address;
        public string publicIdentity;
        public bool online;
        public bool tcpFallbackActive;
        public int versionMajor;
        public int versionMinor;
        public int versionRev;
        public string version;
        public UInt64 clock;
    }
}
