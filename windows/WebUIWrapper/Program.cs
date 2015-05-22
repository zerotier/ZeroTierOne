using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using System.ComponentModel;

namespace WebUIWrapper
{
    [ComImport, GuidAttribute("79EAC9EE-BAF9-11CE-8C82-00AA004BA90B"),
    InterfaceTypeAttribute(ComInterfaceType.InterfaceIsIUnknown)]
    public interface IInternetSecurityManager
    {
        [return: MarshalAs(UnmanagedType.I4)]
        [PreserveSig]
        int SetSecuritySite([In] IntPtr pSite);

        [return: MarshalAs(UnmanagedType.I4)]
        [PreserveSig]
        int GetSecuritySite([Out] IntPtr pSite);

        [return: MarshalAs(UnmanagedType.I4)]
        [PreserveSig]
        int MapUrlToZone([In, MarshalAs(UnmanagedType.LPWStr)] string pwszUrl, out UInt32 pdwZone, UInt32 dwFlags);

        [return: MarshalAs(UnmanagedType.I4)]
        [PreserveSig]
        int GetSecurityId([MarshalAs(UnmanagedType.LPWStr)] string pwszUrl, [MarshalAs(UnmanagedType.LPArray)] byte[] pbSecurityId, ref UInt32 pcbSecurityId, uint dwReserved);

        [return: MarshalAs(UnmanagedType.I4)]
        [PreserveSig]
        int ProcessUrlAction([In, MarshalAs(UnmanagedType.LPWStr)] string pwszUrl, UInt32 dwAction, out byte pPolicy, UInt32 cbPolicy, byte pContext, UInt32 cbContext, UInt32 dwFlags, UInt32 dwReserved);

        [return: MarshalAs(UnmanagedType.I4)]
        [PreserveSig]
        int QueryCustomPolicy([In, MarshalAs(UnmanagedType.LPWStr)] string pwszUrl, ref Guid guidKey, ref byte ppPolicy, ref UInt32 pcbPolicy, ref byte pContext, UInt32 cbContext, UInt32 dwReserved);

        [return: MarshalAs(UnmanagedType.I4)]
        [PreserveSig]
        int SetZoneMapping(UInt32 dwZone, [In, MarshalAs(UnmanagedType.LPWStr)] string lpszPattern, UInt32 dwFlags);

        [return: MarshalAs(UnmanagedType.I4)]
        [PreserveSig]
        int GetZoneMappings(UInt32 dwZone, out UCOMIEnumString ppenumString, UInt32 dwFlags);
    }

    static class Program
    {
        // constants from urlmon.h
        public const UInt32 URLZONE_LOCAL_MACHINE = 0;
        public const UInt32 URLZONE_INTRANET = URLZONE_LOCAL_MACHINE + 1;
        public const UInt32 URLZONE_TRUSTED = URLZONE_INTRANET + 1;
        public const UInt32 URLZONE_INTERNET = URLZONE_TRUSTED + 1;
        public const UInt32 URLZONE_UNTRUSTED = URLZONE_INTERNET + 1;
        public const UInt32 URLZONE_ESC_FLAG = 0x100;
        public const UInt32 SZM_CREATE = 0;
        public const UInt32 SZM_DELETE = 0x1;

        public static Guid CLSID_InternetSecurityManager = new Guid("7b8a2d94-0ac9-11d1-896c-00c04fb6bfc4");
        public static Guid IID_IInternetSecurityManager = new Guid("79eac9ee-baf9-11ce-8c82-00aa004ba90b");

        /// <summary>
        /// The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main()
        {
            try
            {
                Type t = Type.GetTypeFromCLSID(CLSID_InternetSecurityManager);
                object securityManager = Activator.CreateInstance(t);
                if (securityManager != null)
                {
                    IInternetSecurityManager ism = (IInternetSecurityManager)securityManager;
                    ism.SetZoneMapping(URLZONE_TRUSTED, "http://127.0.0.1", SZM_CREATE);
                    ism.SetZoneMapping(URLZONE_INTRANET, "http://127.0.0.1", SZM_CREATE);
                    ism.SetZoneMapping(URLZONE_ESC_FLAG | URLZONE_TRUSTED, "http://127.0.0.1", SZM_CREATE);
                    ism.SetZoneMapping(URLZONE_ESC_FLAG | URLZONE_INTRANET, "http://127.0.0.1", SZM_CREATE);
                }
            }
            catch
            {
                // Okay to continue if adding URL to trusted zone doesn't work...
            }

            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new Form1());
        }
    }
}
