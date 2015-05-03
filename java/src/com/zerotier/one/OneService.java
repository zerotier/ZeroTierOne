/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */


package com.zerotier.one;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.SocketException;
import java.net.SocketTimeoutException;

import com.zerotier.sdk.Event;
import com.zerotier.sdk.EventListener;
import com.zerotier.sdk.Node;
import com.zerotier.sdk.PacketSender;
import com.zerotier.sdk.ResultCode;
import com.zerotier.sdk.Version;
import com.zerotier.sdk.VirtualNetworkConfig;
import com.zerotier.sdk.VirtualNetworkConfigListener;
import com.zerotier.sdk.VirtualNetworkConfigOperation;
import com.zerotier.sdk.VirtualNetworkFrameListener;

public class OneService extends Thread implements Runnable, PacketSender,
								   EventListener, VirtualNetworkConfigListener, 
								   VirtualNetworkFrameListener {
	private Node _node;
	private int _port;

	private DatagramSocket _udpSocket;
	private ServerSocket _tcpSocket;
	private DataStore _ds;
	private long _nextBackgroundTaskDeadline = 0;
	
	private final Thread _udpReceiveThread = new Thread() {
		@Override
		public void run() {
			try {
				long[] bgtask = new long[1];
				byte[] buffer = new byte[16384];
				while(true) {
	    			
	    			bgtask[0] = 0;
	    			DatagramPacket p = new DatagramPacket(buffer, buffer.length);
	    			
	    			try {
	    				_udpSocket.receive(p);
	    				if(p.getLength() > 0)
		    			{
		    				System.out.println("Got Data From: " + p.getAddress().toString() +":" + p.getPort());
		    				
		    				_node.processWirePacket(System.currentTimeMillis(), new InetSocketAddress(p.getAddress(), p.getPort()), 0, p.getData(), bgtask);
		    				_nextBackgroundTaskDeadline = bgtask[0];
		    			}
	    			} catch (SocketTimeoutException e) {}
				}
			} catch (Exception e) {
				e.printStackTrace();
			}
		}
	};
	
	
	public OneService(DataStoreFileProvider prov, int port) {
		this._port = port;
		this._ds = new DataStore(prov);
		
		try {
			_udpSocket = new DatagramSocket(_port);
			_udpSocket.setSoTimeout(100);
			_tcpSocket = new ServerSocket();
			_tcpSocket.bind(new InetSocketAddress("127.0.0.1", _port));
		} catch (SocketException e) {
			e.printStackTrace();
			return;
		} catch (IOException e) {
			e.printStackTrace();
			return;
		}
		
		_udpReceiveThread.start();
		
		_node = new Node(
				System.currentTimeMillis(),
				_ds,
				_ds,
				this,
				this,
				this,
				this);
	}

	@Override
	public void run() {
		if(_node == null)
			return;
		
		while(true) {
		    try {
		
		        long dl = _nextBackgroundTaskDeadline;
		        long now = System.currentTimeMillis();
		
		        if (dl <= now) {
		            long[] returnDeadline = {0};
		            ResultCode rc = _node.processBackgroundTasks(now, returnDeadline);
		            _nextBackgroundTaskDeadline = returnDeadline[0];
		            
		            if(rc != ResultCode.RESULT_OK) {
		            	System.out.println(rc.toString());
		            }
		        }
		        
		        long delay = (dl > now) ? (dl - now) : 100;
		        Thread.sleep(delay);
		
		    } catch (Exception ex) {
		    	System.out.println("Exception in run loop: " + ex.getMessage());
		    	ex.printStackTrace();
		    }
        }
	}

	@Override
	public int onSendPacketRequested(InetSocketAddress addr,
			int linkDesperation, byte[] packetData) {
		System.out.println("onSendPacketRequested to: " + addr.getHostString() +":"+ addr.getPort() + " ");

    	if(_udpSocket == null)
    		return -1;
    	try {
    		DatagramPacket p = new DatagramPacket(packetData, packetData.length, addr);
    		_udpSocket.send(p);
    		System.out.println("Sent");
    	} catch (Exception e) {
    		System.out.println("Error sending datagram: " + e.getMessage());
    		return -1;
    	}
        return 0;
	}

	@Override
	public void onVirtualNetworkFrame(long nwid, long srcMac, long destMac,
			long etherType, long vlanId, byte[] frameData) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public int onNetworkConfigurationUpdated(long nwid,
			VirtualNetworkConfigOperation op, VirtualNetworkConfig config) {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public void onEvent(Event event) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void onNetworkError(Event event, InetSocketAddress source) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void onOutOfDate(Version newVersion) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void onTrace(String message) {
		// TODO Auto-generated method stub
		
	}
}
