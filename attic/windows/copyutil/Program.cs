using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace copyutil
{
    class Program
    {
        static void Main(string[] args)
        {
            if (args.Length != 2)
            {
                Console.WriteLine("Not enough arguments");
                return;
            }

            if (!Directory.Exists(args[0]))
            {
                Console.WriteLine("Source directory doesn't exist!");
                return;
            }

            Console.WriteLine("Creating: " + args[1]);
            DirectoryInfo di = Directory.CreateDirectory(args[1]);

            String authTokenSrc = args[0] + "\\authtoken.secret";
            String authTokenDest = args[1] + "\\authtoken.secret";

            String portSrc = args[0] + "\\zerotier-one.port";
            String portDest = args[1] + "\\zerotier-one.port";

            File.Copy(authTokenSrc, authTokenDest, true);
            File.Copy(portSrc, portDest, true);
        }
    }
}
 