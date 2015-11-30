package fr.free.miniupnp.libnatpmp;

import java.nio.ByteBuffer;


public class NatPmp {
       private static final String JNA_LIBRARY_NAME = LibraryExtractor.getLibraryPath("jninatpmp", true, NatPmp.class);

    static {
        String s = JNA_LIBRARY_NAME;
        startup();
    }

    public ByteBuffer natpmp;

    public NatPmp() {
        init(0, 0);
    }

    public NatPmp(int forcedgw) {
        init(1, forcedgw);
    }

    /** Cleans up the native resources used by this object.
     Attempting to use the object after this has been called
     will lead to crashes.*/
    public void dispose() {
        free();
    }


    protected void finalize() {
        if (natpmp != null)
            free();
    }

    private native void init(int forcegw, int forcedgw);
    private native void free();

    private static native void startup();

    public native int sendPublicAddressRequest();
    public native int sendNewPortMappingRequest(int protocol, int privateport, int publicport, int lifetime);

    //returns a number of milliseconds, in accordance with Java convention
    public native long getNatPmpRequestTimeout();

    public native int readNatPmpResponseOrRetry(NatPmpResponse response);

}
