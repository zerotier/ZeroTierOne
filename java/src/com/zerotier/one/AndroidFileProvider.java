package com.zerotier.one;

import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;

import android.content.Context;

public class AndroidFileProvider implements DataStoreFileProvider {
	Context _ctx;
		
	AndroidFileProvider(Context ctx) {
		this._ctx = ctx;
	}

	@Override
	public FileInputStream getInputFileStream(String name)
			throws FileNotFoundException {
		// TODO Auto-generated method stub
		return _ctx.openFileInput(name);
	}

	@Override
	public FileOutputStream getOutputFileStream(String name)
			throws FileNotFoundException {
		// TODO Auto-generated method stub
		return _ctx.openFileOutput(name, Context.MODE_PRIVATE);
	}

	@Override
	public void deleteFile(String name) throws IOException {
		boolean success = _ctx.deleteFile(name);
		if(!success)
		{
			throw new IOException("Unable to delete file.");
		}
	}

}
