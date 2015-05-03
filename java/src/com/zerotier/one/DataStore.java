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
		try {
			FileOutputStream fos = _provider.getOutputFileStream(name);
            fos.write(buffer);
            fos.close();
            return buffer.length;
		} catch (FileNotFoundException fnf) {
			
		} catch (IOException io) {
			
		}
		return 0;
	}

	@Override
	public int onDelete(String name) {
		try {
			_provider.deleteFile(name);
			return 0;
		} catch (IOException ex) {
			return -1;
		}
	}

	@Override
	public long onDataStoreGet(String name, byte[] out_buffer,
			long bufferIndex, long[] out_objectSize) {
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
			return -1;
		} catch (IOException io) {
			return -2;
		}
	}
	

}
