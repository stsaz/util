/** phiola/Android: config
2024, Simon Zolin */

package com.github.stsaz.phiola;

import java.util.Arrays;

class Conf {
	Conf(Phiola phi) {}

	private int[] fields; // {off, len, number}...
	private byte[] data;
	String value(int i) {
		return new String(Arrays.copyOfRange(data, fields[i*3], fields[i*3] + fields[i*3 + 1]));
	}
	int number(int i) { return fields[i*3 + 2]; }
	boolean enabled(int i) { return fields[i*3 + 2] == 1; }
	void reset() {
		fields = null;
		data = null;
	}

	static final int
		CODEPAGE				= 0,
		CONV_AAC_Q				= CODEPAGE + 1,
		...
		;
	native boolean confRead(String filepath);
	native boolean confWrite(String filepath, byte[] data);
}
