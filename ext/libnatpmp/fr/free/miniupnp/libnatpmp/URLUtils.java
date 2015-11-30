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
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;
import java.util.jar.JarEntry;
import java.util.jar.JarInputStream;

public class URLUtils {

    public static URL getResource(Class<?> cl, String path) throws IOException {
        String clp = cl.getName().replace('.', '/') + ".class";
        URL clu = cl.getClassLoader().getResource(clp);
        String s = clu.toString();
        if (s.endsWith(clp))
            return new URL(s.substring(0, s.length() - clp.length()) + path);
        
        if (s.startsWith("jar:")) {
            String[] ss = s.split("!");
            return new URL(ss[1] + "!/" + path);
        }
        return null;
    }
    
    public static List<URL> listFiles(URL directory) throws IOException {
        List<URL> ret = new ArrayList<URL>();
        String s = directory.toString();
        if (s.startsWith("jar:")) {
            String[] ss = s.substring("jar:".length()).split("!");
            String path = ss[1];
            URL target = new URL(ss[0]);
            InputStream tin = target.openStream();
            try {
                JarInputStream jin = new JarInputStream(tin);
                JarEntry je;
                while ((je = jin.getNextJarEntry()) != null) {
                    String p = "/" + je.getName();
                    if (p.startsWith(path) && p.indexOf('/', path.length() + 1) < 0)
                      
                      ret.add(new URL("jar:" + target + "!" + p));
                }
            } finally {
                tin.close();
            }
        } else if (s.startsWith("file:")) {
            File f = new File(directory.getFile());
            File[] ffs = f.listFiles();
            if (ffs != null)
                for (File ff : ffs)
                    ret.add(ff.toURI().toURL());
        } else 
            throw new IOException("Cannot list contents of " + directory);
        
        return ret;
    }
}