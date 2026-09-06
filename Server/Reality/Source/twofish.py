from collections.abc import Sequence

UINT32_MASK = 0xFFFFFFFF

def is_uint32(x): return 0 <= x <= 0xFFFFFFFF
def is_uint8(x): return 0 <= x <= 0xFF
def iter_blocks(seq, n): return [seq[i:i+n] for i in range(0, len(seq), n)]
def rotate_left_uint32(x, n): return ((x << n) | (x >> (32 - n))) & UINT32_MASK
def rotate_right_uint32(x, n): return ((x >> n) | (x << (32 - n))) & UINT32_MASK

_NUM_ROUNDS = 16

_MDS_MATRIX = [
	[0x01, 0xEF, 0x5B, 0x5B],
	[0x5B, 0xEF, 0xEF, 0x01],
	[0xEF, 0x5B, 0x01, 0xEF],
	[0xEF, 0x01, 0xEF, 0x5B],
]

_RS_MATRIX = [
	[0x01, 0xA4, 0x55, 0x87, 0x5A, 0x58, 0xDB, 0x9E],
	[0xA4, 0x56, 0x82, 0xF3, 0x1E, 0xC6, 0x68, 0xE5],
	[0x02, 0xA1, 0xFC, 0xC1, 0x47, 0xAE, 0x3D, 0x19],
	[0xA4, 0x55, 0x87, 0x5A, 0x58, 0xDB, 0x9E, 0x03],
]

_Q_SBOXES = [
	[
		[0x8, 0x1, 0x7, 0xD, 0x6, 0xF, 0x3, 0x2, 0x0, 0xB, 0x5, 0x9, 0xE, 0xC, 0xA, 0x4],
		[0xE, 0xC, 0xB, 0x8, 0x1, 0x2, 0x3, 0x5, 0xF, 0x4, 0xA, 0x6, 0x7, 0x0, 0x9, 0xD],
		[0xB, 0xA, 0x5, 0xE, 0x6, 0xD, 0x9, 0x0, 0xC, 0x8, 0xF, 0x3, 0x2, 0x4, 0x7, 0x1],
		[0xD, 0x7, 0xF, 0x4, 0x1, 0x2, 0x6, 0xE, 0x9, 0xB, 0x3, 0x0, 0x8, 0x5, 0xC, 0xA],
	],
	[
		[0x2, 0x8, 0xB, 0xD, 0xF, 0x7, 0x6, 0xE, 0x3, 0x1, 0x9, 0x4, 0x0, 0xA, 0xC, 0x5],
		[0x1, 0xE, 0x2, 0xB, 0x4, 0xC, 0x3, 0x7, 0x6, 0xD, 0xA, 0x5, 0xF, 0x9, 0x0, 0x8],
		[0x4, 0xC, 0x7, 0x5, 0x1, 0x6, 0x9, 0xA, 0x0, 0xE, 0xD, 0x8, 0x2, 0xB, 0x3, 0xF],
		[0xB, 0x9, 0x5, 0x1, 0xC, 0x3, 0xD, 0xE, 0x6, 0x4, 0x7, 0xF, 0x2, 0x0, 0x8, 0xA],
	],
]

_FUNCTION_H_SBOX_SEQUENCE = [
	[1, 0, 1, 0],
	[0, 0, 1, 1],
	[0, 1, 0, 1],
	[1, 1, 0, 0],
	[1, 0, 0, 1],
]

def _field_multiply(x, y, mod):
	z = 0
	for i in reversed(range(8)):
		z = (z << 1) ^ ((z >> 7) * mod)
		z ^= ((y >> i) & 1) * x
	return z

def _rotr4(value, amount):
	return ((value << (4 - amount)) | (value >> amount)) & 0xF

def _uint4(v):
	if 0 <= v < (1 << 4):
		return v
	raise ValueError()

def _function_q(x, sboxes):
	a0 = _uint4(x >> 4)
	b0 = _uint4(x & 0xF)
	a1 = _uint4(a0 ^ b0)
	b1 = _uint4(a0 ^ _rotr4(b0, 1) ^ ((a0 << 3) & 0xF))
	a2 = _uint4(sboxes[0][a1])
	b2 = _uint4(sboxes[1][b1])
	a3 = _uint4(a2 ^ b2)
	b3 = _uint4(a2 ^ _rotr4(b2, 1) ^ ((a2 << 3) & 0xF))
	a4 = _uint4(sboxes[2][a3])
	b4 = _uint4(sboxes[3][b3])
	return (b4 << 4) | a4

def _function_h(x, l):
	xs = x.to_bytes(4, "little")
	def sub_bytes(bs, sboxindexes):
		return bytes(_function_q(b, _Q_SBOXES[i]) for (b, i) in zip(bs, sboxindexes))
	def xor_bytes(bs, word):
		return bytes((b ^ wb) for (b, wb) in zip(bs, word.to_bytes(4, "little")))
	for i in reversed(range(len(l))):
		xs = sub_bytes(xs, _FUNCTION_H_SBOX_SEQUENCE[i + 1])
		xs = xor_bytes(xs, l[i])
	xs = sub_bytes(xs, _FUNCTION_H_SBOX_SEQUENCE[0])
	zs = bytearray()
	for row in _MDS_MATRIX:
		z = 0
		for (cell, xb) in zip(row, xs):
			z ^= _field_multiply(cell, xb, 0x169)
		zs.append(z)
	return int.from_bytes(zs, "little")

def _pseudo_hadamard_transform(a, b):
	return (
		(a + b) & UINT32_MASK,
		(a + 2 * b) & UINT32_MASK,
	)

def _expand_key_schedule(key):
	paddedkey = bytearray(key)
	while len(paddedkey) not in (16, 24, 32):
		paddedkey.append(0x00)
	keywords = [int.from_bytes(bs, "little") for bs in iter_blocks(paddedkey, 4)]
	keywordseven = keywords[0 : : 2]
	keywordsodd = keywords[1 : : 2]
	s = []
	for bs in iter_blocks(paddedkey, 8):
		temp = bytearray()
		for row in _RS_MATRIX:
			sum_val = 0
			for (cell, bb) in zip(row, bs):
				sum_val ^= _field_multiply(cell, bb, 0x14D)
			temp.append(sum_val)
		s.append(int.from_bytes(temp, "little"))
	s.reverse()
	expandedkey = []
	for i in range(_NUM_ROUNDS + 4):
		rho = 0x01010101
		a = _function_h(((2 * i + 0) * rho) & UINT32_MASK, keywordseven)
		b = _function_h(((2 * i + 1) * rho) & UINT32_MASK, keywordsodd)
		b = rotate_left_uint32(b, 8)
		a, b = _pseudo_hadamard_transform(a, b)
		expandedkey.append(a)
		expandedkey.append(rotate_left_uint32(b, 9))
	return (tuple(expandedkey), tuple(s))

def _feistel_function(r0, r1, subkey0, subkey1, s):
	t0 = _function_h(r0, s)
	t1 = _function_h(rotate_left_uint32(r1, 8), s)
	t0, t1 = _pseudo_hadamard_transform(t0, t1)
	t0 = (t0 + subkey0) & UINT32_MASK
	t1 = (t1 + subkey1) & UINT32_MASK
	return (t0, t1)

def encrypt_block(block: bytes, key: bytes) -> bytes:
	assert len(block) == 16
	bws = [int.from_bytes(bs, "little") for bs in iter_blocks(bytes(block), 4)]
	keyschedule, s = _expand_key_schedule(key)
	bws[0] ^= keyschedule[0]
	bws[1] ^= keyschedule[1]
	bws[2] ^= keyschedule[2]
	bws[3] ^= keyschedule[3]
	for subkeys in iter_blocks(keyschedule[8:], 2):
		temp0, temp1 = _feistel_function(bws[0], bws[1], subkeys[0], subkeys[1], s)
		bws[2] = rotate_right_uint32(bws[2] ^ temp0, 1)
		bws[3] = rotate_left_uint32(bws[3], 1) ^ temp1
		bws = [bws[2], bws[3], bws[0], bws[1]]
	bws = [bws[2], bws[3], bws[0], bws[1]]
	bws[0] ^= keyschedule[4]
	bws[1] ^= keyschedule[5]
	bws[2] ^= keyschedule[6]
	bws[3] ^= keyschedule[7]
	return b"".join(x.to_bytes(4, "little") for x in bws)

def decrypt_block(block: bytes, key: bytes) -> bytes:
	assert len(block) == 16
	bws = [int.from_bytes(bs, "little") for bs in iter_blocks(bytes(block), 4)]
	keyschedule, s = _expand_key_schedule(key)
	bws[0] ^= keyschedule[4]
	bws[1] ^= keyschedule[5]
	bws[2] ^= keyschedule[6]
	bws[3] ^= keyschedule[7]
	for subkeys in reversed(list(iter_blocks(keyschedule[8:], 2))):
		temp0, temp1 = _feistel_function(bws[0], bws[1], subkeys[0], subkeys[1], s)
		bws[2] = rotate_left_uint32(bws[2], 1) ^ temp0
		bws[3] = rotate_right_uint32(bws[3] ^ temp1, 1)
		bws = [bws[2], bws[3], bws[0], bws[1]]
	bws = [bws[2], bws[3], bws[0], bws[1]]
	bws[0] ^= keyschedule[0]
	bws[1] ^= keyschedule[1]
	bws[2] ^= keyschedule[2]
	bws[3] ^= keyschedule[3]
	return b"".join(x.to_bytes(4, "little") for x in bws)

# CBC mode helper (MxOEmu uses Twofish in CBC mode or ECB mode)
def decrypt_cbc(data: bytes, key: bytes, iv: bytes = b'\x00'*16) -> bytes:
	out = bytearray()
	prev = iv
	for i in range(0, len(data), 16):
		block = data[i:i+16]
		dec = decrypt_block(block, key)
		plain = bytes(a ^ b for a, b in zip(dec, prev))
		out.extend(plain)
		prev = block
	return bytes(out)

def encrypt_cbc(data: bytes, key: bytes, iv: bytes = b'\x00'*16) -> bytes:
	out = bytearray()
	prev = iv
	for i in range(0, len(data), 16):
		block = data[i:i+16]
		xored = bytes(a ^ b for a, b in zip(block, prev))
		enc = encrypt_block(xored, key)
		out.extend(enc)
		prev = enc
	return bytes(out)

def encrypt_ecb(data: bytes, key: bytes) -> bytes:
	out = bytearray()
	for i in range(0, len(data), 16):
		block = data[i:i+16]
		enc = encrypt_block(block, key)
		out.extend(enc)
	return bytes(out)

def decrypt_ecb(data: bytes, key: bytes) -> bytes:
	out = bytearray()
	for i in range(0, len(data), 16):
		block = data[i:i+16]
		dec = decrypt_block(block, key)
		out.extend(dec)
	return bytes(out)

if __name__ == "__main__":
	# Self test: 128-bit key
	key = b'\x00'*16
	plain = b'\x00'*16
	ct = encrypt_block(plain, key)
	pt = decrypt_block(ct, key)
	assert pt == plain
	print(f"Twofish self-test PASSED! Ciphertext: {ct.hex()}")
	# Test CBC
	data = b"Hello, MegaCity! This is a test."
	ct_cbc = encrypt_cbc(data, key)
	pt_cbc = decrypt_cbc(ct_cbc, key)
	assert pt_cbc == data
	print("Twofish CBC test PASSED!")
