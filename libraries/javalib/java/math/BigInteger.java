/*
 * Java core library component.
 *
 * Copyright (c) 1997, 1998
 *      Transvirtual Technologies, Inc.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file.
 */

package java.math;

import java.util.Random;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.IOException;
import kaffe.util.Ptr;

public class BigInteger extends Number implements Comparable {

private static final long serialVersionUID = -8287574255936472291L;
private Ptr number;

public static final BigInteger ZERO;
public static final BigInteger ONE;

static {
	System.loadLibrary("math");
	initialize0();
	/* ZERO and ONE must be defined here, not in their
	   declarations, otherwise they'd be initialized before the
	   libmath is loaded and initialized.  Moving their
	   declarations after this block doesn't help.  */
	ZERO = new BigInteger();
	ONE = new BigInteger(1L);
}

public BigInteger(byte val[]) {
	this();
	if (val.length == 0)
		throw new NumberFormatException("val.length == 0");
	int signum = (val[0] & 128) == 0 ? 1 : -1;
	if (signum == -1) {
		for(int i = val.length; i-- > 0; ) {
			val[i] = (byte)~val[i]; // adjust two's complement
		}
	}
	assignBytes0(signum, val);
	if (signum == -1) {
		add0(this, ONE); // adjust two's complement
	}
}

public BigInteger(int signum, byte magnitude[]) {
	this();
	switch (signum) {
	case -1:
	case 0:
	case 1:
		break;
	default:
		throw new NumberFormatException("signum < -1 || signum > 1");
	}
	if (magnitude.length != 0) {
		assignBytes0(signum, magnitude);
		if (signum == 0 && cmp0(this, ZERO) != 0)
			throw new NumberFormatException("signum == 0 && magnitude[i] != 0");
	}
}

public BigInteger(String val, int radix) {
	this();
	assignString0(val, radix);
}

public BigInteger(String val) {
	this(val, 10);
}

public BigInteger(int numBits, Random rndSrc) {
        this(1, randBytes(numBits, rndSrc));
}

private static byte[] randBytes(int numBits, Random rndSrc) {
	if (numBits < 0)
		throw new IllegalArgumentException("numBits < 0");
	int extra = numBits % 8;
	byte[] ret = new byte[numBits/8 + (extra>0 ? 1 : 0)];
	rndSrc.nextBytes(ret);
	if (extra > 0)
		ret[0] &= ~((~0) << (8-extra));
	return (ret);
}

public BigInteger(int bitLength, int certainty, Random rnd) {
	this();
	if (bitLength < 2)
		throw new ArithmeticException("bitLength < 2");
	byte [] bytes = new byte[(bitLength+7)/8];
	int zeroes = 8 - bitLength%8;
	/* andval is used to zero out unused bits in the most
           significant byte.  */
	byte andval = (byte)(~((~0) << zeroes));
	/* orval is used to set the most significant bit.  */
	byte orval = (byte)(0x100 >> zeroes);
	BigInteger two = new BigInteger(2L);
 rerand:
	for(;;) {
		/* There must be a more efficient algorithm! */
		rnd.nextBytes(bytes);
		/* Ensure that we have the requested length.  */
		bytes[0] = (byte)((bytes[0] & andval) | orval);
		if (bitLength > 2) // 2 is prime, do not discard it
			bytes[bytes.length-1] |= 1;
		assignBytes0(1, bytes);
		if (probablyPrime0(certainty) == 1)
			break;
		/* Only test whether the length has changed when
		   testLength is becomes zero.  */
		long testLength = longValue()-1; // make it even
		if (bitLength < 64)
			testLength |= ~0L << bitLength;
		do {
			add0(this, two);
			testLength += 2;
			if (testLength == 0
			    && bitLength0() > bitLength)
				continue rerand;
		} while (probablyPrime0(certainty) == 0);
		break;
	}
}

private BigInteger(long val) {
	this();
	assignLong0(val);
}

private BigInteger() {
	init0();
}

public static BigInteger valueOf(long val) {
	return (new BigInteger(val));
}

public BigInteger add(BigInteger val) {
	BigInteger r = new BigInteger();
	r.add0(this, val);
	return (r);
}

public BigInteger subtract(BigInteger val) {
	BigInteger r = new BigInteger();
	r.sub0(this, val);
	return (r);
}

public BigInteger multiply(BigInteger val) {
	BigInteger r = new BigInteger();
	r.mul0(this, val);
	return (r);
}

public BigInteger divide(BigInteger val) {
	BigInteger r = new BigInteger();
	r.div0(this, val);
	return (r);
}

public BigInteger remainder(BigInteger val) {
	BigInteger r = new BigInteger();
	r.rem0(this, val);
	return (r);
}

public BigInteger[] divideAndRemainder(BigInteger val) {
	BigInteger q = new BigInteger();
	BigInteger r = new BigInteger();
	divrem0(q, r, this, val);
	return (new BigInteger[]{ q, r });
}

public BigInteger pow(int exponent) {
	BigInteger r = new BigInteger();
	r.pow0(this, exponent);
	return (r);
}

public BigInteger gcd(BigInteger val) {
	BigInteger r = new BigInteger();
	r.gcd0(this, val);
	return (r);
}

public BigInteger abs() {
	BigInteger r = new BigInteger();
	r.abs0(this);
	return (r);
}

public BigInteger negate() {
	BigInteger r = new BigInteger();
	r.neg0(this);
	return (r);
}

public int signum() {
	return (compareTo(ZERO));
}

public BigInteger mod(BigInteger mod) {
	BigInteger r = new BigInteger();
	r.mod0(this, mod);
	return (r);
}

public BigInteger modPow(BigInteger exponent, BigInteger mod) {
	BigInteger r = new BigInteger();
	r.modpow0(this, exponent, mod);
	return (r);
}

public BigInteger modInverse(BigInteger mod) {
	BigInteger r = new BigInteger();
	r.modinv0(this, mod);
	return (r);
}

public BigInteger shiftLeft(int n) {
	BigInteger s = new BigInteger();
	s.setbit0(s, n);
	s.mul0(this, s);
	return (s);
}

public BigInteger shiftRight(int n) {
	BigInteger s = new BigInteger();
	s.setbit0(s, n);
	s.div0(this, s);
	return (s);
}

public BigInteger and(BigInteger val) {
	BigInteger r = new BigInteger();
	r.and0(this, val);
	return (r);
}

public BigInteger or(BigInteger val) {
	BigInteger r = new BigInteger();
	r.or0(this, val);
	return (r);
}

public BigInteger xor(BigInteger val) {
	BigInteger r = new BigInteger();
	r.xor0(this, val);
	return (r);
}

public BigInteger not() {
	BigInteger r = new BigInteger();
	r.not0(this);
	return (r);
}

public BigInteger andNot(BigInteger val) {
	BigInteger r = new BigInteger();
	r.and0(this, val);
	r.not0(r);
	return (r);
}

public boolean testBit(int n) {
	BigInteger b = new BigInteger();
	b.setbit0(this, n);
	if (cmp0(b, this) == 0) {
		return (true);
	}
	else {
		return (false);
	}
}

public BigInteger setBit(int n) {
	BigInteger r = new BigInteger();
	r.setbit0(this, n);
	return (r);
}

public BigInteger clearBit(int n) {
	BigInteger r = new BigInteger();
	r.clrbit0(this, n);
	return (r);
}

public BigInteger flipBit(int n) {
	BigInteger r = new BigInteger();
	r.setbit0(r, n);
	r.xor0(r, this);
	return (r);
}

public int getLowestSetBit() {
	return (scansetbit0());
}

public int bitLength() {
	return bitLength0();
}

public int bitCount() {
	if (compareTo(ZERO) < 0)
		return (negate().hamDist0(ZERO));
	else
		return (hamDist0(ZERO));
}

public boolean isProbablePrime(int certainty) {
	if (probablyPrime0(certainty) == 0) {
		return (false);
	}
	return (true);
}

public int compareTo(Object obj) {
	return compareTo((BigInteger)obj);
}

public int compareTo(BigInteger val) {
	int r = cmp0(this, val);

	// compute sign since JDK spec asks us to return -1/0/1, 
	// but the GMP doc does not guarantee that.
	return (r == 0) ? 0 : (r < 0) ? -1 : 1;
}

public boolean equals(Object o) {
	return (o instanceof BigInteger) && compareTo((BigInteger)o) == 0;
}

public BigInteger min(BigInteger val) {
	int r = compareTo(val);
	if (r > 0) {
		return (val);
	}
	else {
		return (this);
	}
}

public BigInteger max(BigInteger val) {
	int r = compareTo(val);
	if (r < 0) {
		return (val);
	}
	else {
		return (this);
	}
}

public int hashCode() {
	// It probably isn't this but I don't know what it's suppose to be.
	return (super.hashCode());
}

public String toString(int radix) {
	return (toString0(radix));
}

public String toString() {
	return (toString(10));
}

public byte[] toByteArray() {
	byte[] ret = new byte[1 + bitLength0()/8];
	BigInteger copy = abs(), divisor = new BigInteger();
	divisor.setbit0(divisor, 32); // prepare to shift right
	int sign = cmp0(this, ZERO);
	if (sign < 0) {
		sub0(copy, ONE); // adjust two's complement
	}
	int i = ret.length; // we know it's >= 1
	while (i > 4) {
		int num = copy.toInt0();
		ret[--i] = (byte)num;
		ret[--i] = (byte)(num>>8);
		ret[--i] = (byte)(num>>16);
		ret[--i] = (byte)(num>>24);
		div0(copy, divisor);
	}
	{
		int num = copy.toInt0();
		switch (i) {
		case 4:
			ret[--i] = (byte)num;
			num >>= 8;
			// fall through
		case 3:
			ret[--i] = (byte)num;
			num >>= 8;
			// fall through
		case 2:
			ret[--i] = (byte)num;
			num >>= 8;
			// fall through
		case 1:
			ret[--i] = (byte)num;
		}
	}
	if (sign < 0) {
		i = ret.length;
		while (i-- > 0) {
			ret[i] = (byte)~ret[i]; // adjust two's complement
		}
	}
	return (ret);
}

public int intValue() {
	return (toInt0());
}

public long longValue() {
	return ((long)toInt0() | shiftRight(32).toInt0());
}

public float floatValue() {
	return ((float)doubleValue());
}

public double doubleValue() {
	return (toDouble0());
}

protected void finalize() throws Throwable {
	finalize0();
	super.finalize();
}

/**
 * inner class for Sun compatible default serialization
 */
class DefaultSerialization {

    	/* serialized form is
	 * int bitCount
	 *
	 *   The bitCount of this BigInteger, as returned by 
	 *   bitCount(), or -1 (either value is acceptable).
	 *
	 * int bitLength
	 *
	 *   The bitLength of this BigInteger, as returned by bitLength(), 
	 *   or -1 (either value is acceptable).
	 *
	 * int firstNonzeroByteNum
	 *
	 *   The byte-number of the lowest-order nonzero byte in the 
	 *   magnitude of this BigInteger, or -2 (either value is acceptable). 
	 *   The least significant byte has byte-number 0, the next byte in 
	 *   order of increasing significance has byte-number 1, and so forth.
	 *
	 * int lowestSetBit
	 *
	 *   The lowest set bit of this BigInteger, as returned by 
	 *   getLowestSetBit(), or -2 (either value is acceptable).
	 *
	 * byte[] magnitude
	 *
	 *   The magnitude of this BigInteger, in big-endian byte-order: 
	 *   the zeroth element of this array is the most-significant byte 
	 *   of the magnitude. The magnitude must be "minimal" in that the 
	 *   most-significant byte (magnitude[0]) must be non-zero.  This is 
	 *   necessary to ensure that there is exactly one representation for 
	 *   each BigInteger value. Note that this implies that the BigInteger 
	 *   zero has a zero-length magnitude array.
	 *
	 * int signum
	 *
         *   The signum of this BigInteger: -1 for negative, 0 for zero, 
	 *   or 1 for positive.  Note that the BigInteger zero must have a 
	 *   signum of 0. This is necessary to ensures that there is exactly 
	 *   one representation for each BigInteger value.
	 */

	private int bitCount;
	private int bitLength;
	private int firstNonzeroByteNum;
	private int lowestSetBit;
	private int signum;
	private byte [] magnitude;

	private void readDefaultObject() {
		BigInteger.this.init0();
		BigInteger.this.assignBytes0(signum, magnitude);
	}

	private void writeDefaultObject() {
		bitCount = -1;
		bitLength = -1;
		firstNonzeroByteNum = -2;
		lowestSetBit = BigInteger.this.getLowestSetBit();
		signum = BigInteger.this.signum();
		/* XXX not implemented */
		magnitude = BigInteger.this.toByteArray();   
	}
}

private native void init0();
private native void finalize0();

private native void assignBytes0(int s, byte[] m);
private native void assignString0(String v, int i);
private native void assignLong0(long v);

private native void add0(BigInteger s1, BigInteger s2);
private native void sub0(BigInteger s1, BigInteger s2);
private native void mul0(BigInteger s1, BigInteger s2);
private native void div0(BigInteger s1, BigInteger s2);
private native void rem0(BigInteger s1, BigInteger s2);
private native void abs0(BigInteger s);
private native void neg0(BigInteger s);
private native void pow0(BigInteger s, int p);
private native void gcd0(BigInteger s1, BigInteger s2);
private native void mod0(BigInteger s1, BigInteger s2);
private native void modpow0(BigInteger s1, BigInteger s2, BigInteger s3);
private native void modinv0(BigInteger s1, BigInteger s2);

private native void and0(BigInteger s1, BigInteger s2);
private native void or0(BigInteger s1, BigInteger s2);
private native void xor0(BigInteger s1, BigInteger s2);
private native void not0(BigInteger s);

private native void clrbit0(BigInteger s, int n);
private native void setbit0(BigInteger s, int n);
private native int scansetbit0();
private native int probablyPrime0(int cert);
private native int bitLength0();
private native int hamDist0(BigInteger s);

private native static int cmp0(BigInteger s1, BigInteger s2);

private native static void initialize0();
private native static void divrem0(BigInteger r, BigInteger q, BigInteger s1, BigInteger s2);
private native String toString0(int base);
private native double toDouble0();
private native int toInt0();

}
