package com.zerotier.one;

import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;

import com.zerotier.sdk.DataStoreGetListener;
import com.zerotier.sdk.DataStorePutListener;

public class DataStore implements DataStoreGetListener, DataStorePutListener {

	private DataStoreFileProvider _provider;

	DataStore(DataStoreFileProvider provider) {
		this._provider = provider;
	}
	
	@Override
	public int onDataStorePut(String name, byte[] buffer, boolean secure) {
		System.out.println("Writing File: " + name);
		try {
			FileOutputStream fos = _provider.getOutputFileStream(name);
            fos.write(buffer);
            fos.close();
            return buffer.length;
		} catch (FileNotFoundException fnf) {
			fnf.printStackTrace();
			return -1;
		} catch (IOException io) {
			io.printStackTrace();
			return -2;
		}
	}

	@Override
	public int onDelete(String name) {
		System.out.println("Deleting File: " + name);
		try {
			_provider.deleteFile(name);
			return 0;
		} catch (IOException ex) {
			ex.printStackTrace();
			return -1;
		}
	}

	@Override
	public long onDataStoreGet(String name, byte[] out_buffer,
			long bufferIndex, long[] out_objectSize) {
		System.out.println("Reading File: " + name);
		try {
            FileInputStream fin = _provider.getInputFileStream(name);
			out_objectSize[0] = fin.getChannel().size();
            if(bufferIndex > 0)
            {
                fin.skip(bufferIndex);
            }
            int read = fin.read(out_buffer);
            fin.close();
            return read;
		} catch (FileNotFoundException fnf) {
			// Can't read a file that doesn't exist!
			out_objectSize[0] = 0;
			return 0;
		} catch (IOException io) {
			io.printStackTrace();
			return -2;
		}
	}
	

}
