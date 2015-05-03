package com.zerotier.one;

import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;

public interface DataStoreFileProvider {
	FileInputStream getInputFileStream(String name) throws FileNotFoundException;
	FileOutputStream getOutputFileStream(String name) throws FileNotFoundException;
	void deleteFile(String name) throws IOException;
}
