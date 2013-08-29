using System;
using System.Collections.Generic;
using System.Linq;
using System.ServiceProcess;
using System.Text;
using System.Threading.Tasks;

namespace Service
{
    static class Program
    {
        static void Main()
        {
            ServiceBase.Run(new ZeroTierOneService());
        }
    }
}
