package com.zerotier.one;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;

public class JavaFileProvider implements DataStoreFileProvider {
	private String _path;

	public JavaFileProvider(String path) {
		this._path = path;
	}
	
	@Override
	public FileInputStream getInputFileStream(String name)
			throws FileNotFoundException {
		File f = new File(_path + File.pathSeparator + name);
		return new FileInputStream(f);
	}

	@Override
	public FileOutputStream getOutputFileStream(String name)
			throws FileNotFoundException {
		File f = new File(_path + File.pathSeparator + name);
		return new FileOutputStream(f);
	}

	@Override
	public void deleteFile(String name) throws IOException {
		File f = new File(_path + File.pathSeparator + name);
		boolean success = f.delete();
		if(!success) {
			throw new IOException("Unable to delete file: " + _path + File.pathSeparator + name);
		}
	}
}
