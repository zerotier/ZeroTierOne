package com.zerotier.one;

import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;

import android.content.Context;
import android.util.Log;

public class AndroidFileProvider implements DataStoreFileProvider {
	private static final String TAG = "AndroidFileProvider";

	Context _ctx;
		
	public AndroidFileProvider(Context ctx) {
		this._ctx = ctx;
	}

	@Override
	public FileInputStream getInputFileStream(String name)
			throws FileNotFoundException {
		Log.d(TAG, "Returning FileInputStream for: " + name);
		return _ctx.openFileInput(name);
	}

	@Override
	public FileOutputStream getOutputFileStream(String name)
			throws FileNotFoundException {
		Log.d(TAG, "Returning FileOutputStream for: " + name);
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
