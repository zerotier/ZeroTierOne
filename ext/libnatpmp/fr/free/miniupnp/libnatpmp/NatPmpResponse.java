package fr.free.miniupnp.libnatpmp;

public class NatPmpResponse {

    public static final int TYPE_PUBLICADDRESS=0;
    public static final int TYPE_UDPPORTMAPPING=1;
    public static final int TYPE_TCPPORTMAPPING=2;

    /** see TYPE_* constants */
    public short type;
    /** NAT-PMP response code */
    public short resultcode;
    /** milliseconds since start of epoch */
    public long epoch;
    
    /** only defined if type == 0*/
    public int addr;

    /** only defined if type != 0 */
    public int privateport;

    /** only defined if type != 0 */
    public int mappedpublicport;

    /** only defined if type != 0 */
    public long lifetime; //milliseconds

}