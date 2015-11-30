package fr.free.miniupnp.libnatpmp;

/** I (Leah X Schmidt) copied this code from jnaerator, because
JNAerator's extractor requires you to buy into the whole JNA
concept.

JNAErator is
Copyright (c) 2009 Olivier Chafik, All Rights Reserved

JNAerator is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

JNAerator is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with JNAerator.  If not, see <http://www.gnu.org/licenses/>.

*/

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URL;
import java.net.URLConnection;
import java.net.URLDecoder;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

public class LibraryExtractor {

    private static boolean libPathSet = false;

    public static String getLibraryPath(String libraryName, boolean extractAllLibraries, Class<?> cl) {
        try {
            String customPath = System.getProperty("library." + libraryName);
            if (customPath == null)
                customPath = System.getenv(libraryName.toUpperCase() + "_LIBRARY");
            if (customPath != null) {
                File f = new File(customPath);
                if (!f.exists())
                    System.err.println("Library file '" + customPath + "' does not exist !");
                else
                    return f.getAbsolutePath();
            }
            //ClassLoader cl = LibraryExtractor.class.getClassLoader();
            String prefix = "(?i)" + (isWindows() ? "" : "lib") + libraryName + "[^A-Za-z_].*";
            String libsuffix = "(?i).*\\.(so|dll|dylib|jnilib)";
            //String othersuffix = "(?i).*\\.(pdb)";

            URL sourceURL = null;
            List<URL> otherURLs = new ArrayList<URL>();


            String arch = getCurrentOSAndArchString();
            //System.out.println("libURL = " + libURL);
            List<URL> list = URLUtils.listFiles(URLUtils.getResource(cl, "libraries/" + arch)),
                noArchList = URLUtils.listFiles(URLUtils.getResource(cl, "libraries/noarch"));

            Set<String> names = new HashSet<String>();
            for (URL url : list) {
                String name = getFileName(url);
                names.add(name);
            }
            for (URL url : noArchList) {
                String name = getFileName(url);
                if (names.add(name))
                    list.add(url);
            }

            for (File f : new File(".").listFiles())
                if (f.isFile())
                    list.add(f.toURI().toURL());

            for (URL url : list) {
                String name = getFileName(url);
                boolean pref = name.matches(prefix), suff = name.matches(libsuffix);
                if (pref && suff)
                    sourceURL = url;
                else //if (suff || fileName.matches(othersuffix))
                    otherURLs.add(url);
            }
            List<File> files = new ArrayList<File>();
            if (extractAllLibraries) {
                for (URL url : otherURLs)
                    files.add(extract(url));
            }

            if (System.getProperty("javawebstart.version") != null) {
                if (isWindows()) {
                    //File f = new File("c:\\Windows\\" + (Platform.is64Bit() ? "SysWOW64\\" : "System32\\") + libraryName + ".dll");
                    File f = new File("c:\\Windows\\" + "System32\\" + libraryName + ".dll");
                    if (f.exists())
                        return f.toString();
                } else if (isMac()) {
                    File f = new File("/System/Library/Frameworks/" + libraryName + ".framework/" + libraryName);
                    if (f.exists())
                        return f.toString();
                }
            }

            if (sourceURL == null)
                return libraryName;
            else {
                File file = extract(sourceURL);
                files.add(file);

                int lastSize;
                do {
                    lastSize = files.size();
                    for (Iterator<File> it = files.iterator(); it.hasNext();) {
                        File f = it.next();
                        if (!f.getName().matches(libsuffix))
                            continue;

                        try {
                            System.load(f.toString());
                            it.remove();
                        } catch (Throwable ex) {
                            System.err.println("Loading " + f.getName() + " failed (" + ex + ")");
                        }
                    }
                } while (files.size() < lastSize);

                return file.getCanonicalPath();
            }
        } catch (Throwable ex) {
            System.err.println("ERROR: Failed to extract library " + libraryName);
            ex.printStackTrace();
            return libraryName;
        }
    }

    public static final boolean isWindows() {
        String osName = System.getProperty("os.name");
        return osName.startsWith("Windows");
    }

    public static final boolean isMac() {
        String osName = System.getProperty("os.name");
        return osName.startsWith("Mac") || osName.startsWith("Darwin");
    }

    //this code is from JNA, but JNA has a fallback to some native
    //stuff in case this doesn't work.  Since sun.arch.data.model is
    //defined for Sun and IBM, this should work nearly everywhere.
    public static final boolean is64Bit() {
        String model = System.getProperty("sun.arch.data.model",
                                          System.getProperty("com.ibm.vm.bitmode"));
        if (model != null) {
            return "64".equals(model);
        }
        String arch = System.getProperty("os.arch").toLowerCase();
        if ("x86_64".equals(arch)
            || "ia64".equals(arch)
            || "ppc64".equals(arch)
            || "sparcv9".equals(arch)
            || "amd64".equals(arch)) {
            return true;
        }

        return false;
    }

    public static String getCurrentOSAndArchString() {
        String os = System.getProperty("os.name"), arch = System.getProperty("os.arch");
        if (os.equals("Mac OS X")) {
            os = "darwin";
            arch = "fat";
        } else if (os.startsWith("Windows")) {
            return "win" + (is64Bit() ? "64" : "32");
        } else if (os.matches("SunOS|Solaris"))
            os = "solaris";
        return os + "-" + arch;
    }

    private static File extract(URL url) throws IOException {
        File localFile;
        if ("file".equals(url.getProtocol()))
            localFile = new File(URLDecoder.decode(url.getFile(), "UTF-8"));
        else {
            File f = new File(System.getProperty("user.home"));
            f = new File(f, ".jnaerator");
            f = new File(f, "extractedLibraries");
            if (!f.exists())
                f.mkdirs();

            if (!libPathSet) {
                String path = System.getProperty("java.library.path");
                if (path == null) {
                    System.setProperty("java.library.path", f.toString());
                } else {
                    System.setProperty("java.library.path", path + ":" + f);
                }
                
                libPathSet = true;
            }
            localFile = new File(f, new File(url.getFile()).getName());
			URLConnection c = url.openConnection();
			if (localFile.exists() && localFile.lastModified() > c.getLastModified()) {
                            c.getInputStream().close();
			} else {
                            System.out.println("Extracting " + url);
                            InputStream in = c.getInputStream();
                            OutputStream out = new FileOutputStream(localFile);
                            int len;
                            byte[] b = new byte[1024];
                            while ((len = in.read(b)) > 0)
                                out.write(b, 0, len);
                            out.close();
                            in.close();
			}
        }
        return localFile;
    }

    private static String getFileName(URL url) {
        return new File(url.getFile()).getName();
    }

    public static void main(String[] args) {
        System.out.println(getCurrentOSAndArchString());
    }
}