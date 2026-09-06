#!/usr/bin/env python3
"""
headless_char_test.py - Automated Headless Character Creation & World Entry Suite
Part of MxOEmu Project (Reality Server)

Validates the full client-server lifecycle headlessly:
  1. AuthServer Handshake (RSA-1024, Twofish CBC, Ticket Signing, User RSA-768 Extraction)
  2. MarginServer Connection (CERT_ConnectRequest, CERT_Challenge RSA Decryption, CERT_ChallengeResponse)
  3. Margin Protocol Negotiation (MS_ConnectRequest, MS_ConnectChallenge, MS_ConnectReply, SessionId)
  4. Character Creation (MS_ClaimCharacterNameRequest, MS_CreateCharacterRequest with RSI Appearance)
  5. World Load Reply Stream (13 MS_LoadCharacterReply chunks, Final Packet with Port Verification)
  6. Game UDP World Entry (43-byte InitialUDPPacket with Twofish ECB encrypted session token)
  7. Margin UDP Session Confirmation (MS_EstablishUDPSessionReply 0x11 assertion)

Usage:
  python headless_char_test.py [--host 15.204.82.250] [--username Slacker] [--password test] [--cycles 1]
"""

import argparse
import hashlib
import os
import socket
import struct
import sys
import time
import zlib
from cryptography.hazmat.primitives.asymmetric import rsa, padding
from cryptography.hazmat.primitives import hashes

# ----------------- EMBEDDED PURE-PYTHON TWOFISH CIPHER -----------------

UINT32_MASK = 0xFFFFFFFF
def _iter_blocks(seq, n): return [seq[i:i+n] for i in range(0, len(seq), n)]
def _rotl32(x, n): return ((x << n) | (x >> (32 - n))) & UINT32_MASK
def _rotr32(x, n): return ((x >> n) | (x << (32 - n))) & UINT32_MASK

_MDS = [
    [0x01, 0xEF, 0x5B, 0x5B],
    [0x5B, 0xEF, 0xEF, 0x01],
    [0xEF, 0x5B, 0x01, 0xEF],
    [0xEF, 0x01, 0xEF, 0x5B],
]

_RS = [
    [0x01, 0xA4, 0x55, 0x87, 0x5A, 0x58, 0xDB, 0x9E],
    [0xA4, 0x56, 0x82, 0xF3, 0x1E, 0xC6, 0x68, 0xE5],
    [0x02, 0xA1, 0xFC, 0xC1, 0x47, 0xAE, 0x3D, 0x19],
    [0xA4, 0x55, 0x87, 0x5A, 0x58, 0xDB, 0x9E, 0x03],
]

_Q_BOXES = [
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

_H_SEQ = [
    [1, 0, 1, 0],
    [0, 0, 1, 1],
    [0, 1, 0, 1],
    [1, 1, 0, 0],
    [1, 0, 0, 1],
]

def _gf_mul(x, y, mod):
    z = 0
    for i in reversed(range(8)):
        z = (z << 1) ^ ((z >> 7) * mod)
        z ^= ((y >> i) & 1) * x
    return z

def _q_box(x, sboxes):
    a0, b0 = x >> 4, x & 0xF
    a1 = a0 ^ b0
    b1 = a0 ^ (((b0 << 3) | (b0 >> 1)) & 0xF) ^ ((a0 << 3) & 0xF)
    a2, b2 = sboxes[0][a1], sboxes[1][b1]
    a3 = a2 ^ b2
    b3 = a2 ^ (((b2 << 3) | (b2 >> 1)) & 0xF) ^ ((a2 << 3) & 0xF)
    a4, b4 = sboxes[2][a3], sboxes[3][b3]
    return (b4 << 4) | a4

def _func_h(x, l):
    xs = x.to_bytes(4, "little")
    for i in reversed(range(len(l))):
        xs = bytes(_q_box(b, _Q_BOXES[s]) for (b, s) in zip(xs, _H_SEQ[i + 1]))
        xs = bytes((b ^ wb) for (b, wb) in zip(xs, l[i].to_bytes(4, "little")))
    xs = bytes(_q_box(b, _Q_BOXES[s]) for (b, s) in zip(xs, _H_SEQ[0]))
    zs = bytearray()
    for row in _MDS:
        z = 0
        for (cell, xb) in zip(row, xs):
            z ^= _gf_mul(cell, xb, 0x169)
        zs.append(z)
    return int.from_bytes(zs, "little")

def _twofish_expand_key(key):
    padded = bytearray(key)
    while len(padded) not in (16, 24, 32):
        padded.append(0x00)
    words = [int.from_bytes(bs, "little") for bs in _iter_blocks(padded, 4)]
    weven, wodd = words[0::2], words[1::2]
    s = []
    for bs in _iter_blocks(padded, 8):
        temp = bytearray()
        for row in _RS:
            sum_val = 0
            for (cell, bb) in zip(row, bs):
                sum_val ^= _gf_mul(cell, bb, 0x14D)
            temp.append(sum_val)
        s.append(int.from_bytes(temp, "little"))
    s.reverse()
    exp_key = []
    for i in range(20):
        rho = 0x01010101
        a = _func_h(((2 * i + 0) * rho) & UINT32_MASK, weven)
        b = _rotl32(_func_h(((2 * i + 1) * rho) & UINT32_MASK, wodd), 8)
        sum_ab = (a + b) & UINT32_MASK
        exp_key.append(sum_ab)
        exp_key.append(_rotl32((a + 2 * b) & UINT32_MASK, 9))
    return (tuple(exp_key), tuple(s))

def _tf_encrypt_block(block: bytes, key: bytes) -> bytes:
    bws = [int.from_bytes(bs, "little") for bs in _iter_blocks(bytes(block), 4)]
    ks, s = _twofish_expand_key(key)
    for i in range(4):
        bws[i] ^= ks[i]
    for r in range(16):
        sub0, sub1 = ks[8 + 2 * r], ks[9 + 2 * r]
        t0 = _func_h(bws[0], s)
        t1 = _func_h(_rotl32(bws[1], 8), s)
        f0 = (t0 + t1 + sub0) & UINT32_MASK
        f1 = (t0 + 2 * t1 + sub1) & UINT32_MASK
        b2 = _rotr32(bws[2] ^ f0, 1)
        b3 = _rotl32(bws[3], 1) ^ f1
        bws = [b2, b3, bws[0], bws[1]]
    bws = [bws[2], bws[3], bws[0], bws[1]]
    for i in range(4):
        bws[i] ^= ks[4 + i]
    return b"".join(x.to_bytes(4, "little") for x in bws)

def _tf_decrypt_block(block: bytes, key: bytes) -> bytes:
    bws = [int.from_bytes(bs, "little") for bs in _iter_blocks(bytes(block), 4)]
    ks, s = _twofish_expand_key(key)
    for i in range(4):
        bws[i] ^= ks[4 + i]
    for r in reversed(range(16)):
        sub0, sub1 = ks[8 + 2 * r], ks[9 + 2 * r]
        t0 = _func_h(bws[0], s)
        t1 = _func_h(_rotl32(bws[1], 8), s)
        f0 = (t0 + t1 + sub0) & UINT32_MASK
        f1 = (t0 + 2 * t1 + sub1) & UINT32_MASK
        b2 = _rotl32(bws[2], 1) ^ f0
        b3 = _rotr32(bws[3] ^ f1, 1)
        bws = [b2, b3, bws[0], bws[1]]
    bws = [bws[2], bws[3], bws[0], bws[1]]
    for i in range(4):
        bws[i] ^= ks[i]
    return b"".join(x.to_bytes(4, "little") for x in bws)

def tf_encrypt_cbc(data: bytes, key: bytes, iv: bytes = b'\x00'*16) -> bytes:
    out = bytearray()
    prev = iv
    for i in range(0, len(data), 16):
        block = data[i:i+16]
        xored = bytes(a ^ b for a, b in zip(block, prev))
        enc = _tf_encrypt_block(xored, key)
        out.extend(enc)
        prev = enc
    return bytes(out)

def tf_decrypt_cbc(data: bytes, key: bytes, iv: bytes = b'\x00'*16) -> bytes:
    out = bytearray()
    prev = iv
    for i in range(0, len(data), 16):
        block = data[i:i+16]
        dec = _tf_decrypt_block(block, key)
        plain = bytes(a ^ b for a, b in zip(dec, prev))
        out.extend(plain)
        prev = block
    return bytes(out)

def tf_encrypt_ecb(data: bytes, key: bytes) -> bytes:
    out = bytearray()
    for i in range(0, len(data), 16):
        block = data[i:i+16]
        enc = _tf_encrypt_block(block, key)
        out.extend(enc)
    return bytes(out)

# ----------------- RSA OAEP DECRYPTION -----------------

def mgf1_sha1(seed, length):
    T = bytearray()
    counter = 0
    while len(T) < length:
        C = counter.to_bytes(4, 'big')
        T.extend(hashlib.sha1(seed + C).digest())
        counter += 1
    return bytes(T[:length])

def rsa_oaep_decrypt(ciphertext: bytes, d: int, n: int) -> bytes:
    k = (n.bit_length() + 7) // 8
    c = int.from_bytes(ciphertext, 'big')
    m = pow(c, d, n)
    EM = m.to_bytes(k, 'big')
    h_len = 20
    masked_seed = EM[1 : 1 + h_len]
    masked_db = EM[1 + h_len :]
    seed_mask = mgf1_sha1(masked_db, h_len)
    seed = bytes(a ^ b for a, b in zip(masked_seed, seed_mask))
    db_mask = mgf1_sha1(seed, len(masked_db))
    db = bytes(a ^ b for a, b in zip(masked_db, db_mask))
    sep_idx = db.find(b'\x01', h_len)
    assert sep_idx != -1, "RSA-OAEP 0x01 separator not found"
    return db[sep_idx + 1 :]

# ----------------- PACKET FRAMING & CRYPTED ENVELOPES -----------------

def send_var_packet(sock: socket.socket, payload: bytes):
    length = len(payload)
    if length > 0x7F:
        hdr = struct.pack(">H", length | 0x8000)
    else:
        hdr = bytes([length])
    sock.sendall(hdr + payload)

def recv_var_packet(sock: socket.socket) -> bytes:
    first = sock.recv(1)
    if not first:
        raise ConnectionResetError("Connection closed while waiting for length header")
    first_byte = first[0]
    if first_byte & 0x80:
        second = sock.recv(1)
        if not second:
            raise ConnectionResetError("Connection closed reading second length byte")
        pkt_len = ((first_byte & 0x7F) << 8) | second[0]
    else:
        pkt_len = first_byte
    buf = bytearray()
    while len(buf) < pkt_len:
        chunk = sock.recv(pkt_len - len(buf))
        if not chunk:
            raise ConnectionResetError("Connection truncated while reading packet body")
        buf.extend(chunk)
    return bytes(buf)

def wrap_twofish_envelope(payload: bytes, key: bytes) -> bytes:
    length = len(payload)
    timestamp = int(time.time())
    crc_me = struct.pack("<HI", length, timestamp) + payload
    crc_val = zlib.crc32(crc_me)
    encrypt_me = struct.pack("<I", crc_val) + crc_me
    pad_len = 16 - (len(encrypt_me) % 16)
    padded = encrypt_me + bytes([pad_len] * pad_len)
    iv = os.urandom(16)
    crypted = tf_encrypt_cbc(padded, key, iv)
    return iv + crypted

def unwrap_twofish_envelope(data: bytes, key: bytes) -> bytes:
    iv = data[:16]
    crypted = data[16:]
    decrypted = tf_decrypt_cbc(crypted, key, iv)
    pad_len = decrypted[-1]
    unpadded = decrypted[:-pad_len]
    crc_val, length, timestamp = struct.unpack_from("<IHI", unpadded, 0)
    crc_me = unpadded[4:]
    computed_crc = zlib.crc32(crc_me)
    assert computed_crc == crc_val, f"CRC32 mismatch: {computed_crc} != {crc_val}"
    return unpadded[10 : 10 + length]

# ----------------- MASTER TEST PIPELINE -----------------

def run_test(host: str, auth_port: int, margin_port: int, username: str, password: str, handle_prefix: str) -> bool:
    print(f"\n[+] Connecting to AuthServer at {host}:{auth_port}...")
    
    # Locate pubkey.dat
    pubkey_paths = [
        r"D:\Github\MxOEmu\Server\Reality\Binaries\pubkey.dat",
        r"E:\Games\The Matrix Online\pubkey.dat",
        os.path.join(os.path.dirname(__file__), "pubkey.dat"),
        os.path.join(os.path.dirname(__file__), "..", "Binaries", "pubkey.dat")
    ]
    pubkey_path = next((p for p in pubkey_paths if os.path.exists(p)), None)
    if not pubkey_path:
        raise FileNotFoundError("Could not find pubkey.dat in standard repository locations")
    
    with open(pubkey_path, 'rb') as f:
        data = f.read()
    pos = 4
    if data[pos] == 0x02:
        mod_len = data[pos+1]
        if mod_len & 0x80:
            num_len_bytes = mod_len & 0x7F
            mod_len = int.from_bytes(data[pos+2:pos+2+num_len_bytes], 'big')
            mod_bytes = data[pos+2+num_len_bytes:pos+2+num_len_bytes+mod_len]
            pos = pos + 2 + num_len_bytes + mod_len
        else:
            mod_bytes = data[pos+2:pos+2+mod_len]
            pos = pos + 2 + mod_len
        modulus = int.from_bytes(mod_bytes, 'big')

    if data[pos] == 0x02:
        exp_len = data[pos+1]
        exponent = int.from_bytes(data[pos+2:pos+2+exp_len], 'big')

    server_pubkey = rsa.RSAPublicNumbers(exponent, modulus).public_key()
    print(f"    Loaded RSA-1024 server key (e={exponent}).")

    auth_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    auth_sock.settimeout(12.0)
    margin_sock = None
    udp_sock = None

    try:
        auth_sock.connect((host, auth_port))

        # AS_GetPublicKeyRequest (0x06)
        send_var_packet(auth_sock, struct.pack("<BII", 0x06, 5668, 4))
        get_pubkey_reply = recv_var_packet(auth_sock)
        assert get_pubkey_reply[0] == 0x07, "AS_GetPublicKeyReply failed"

        # AS_AuthRequest (0x08)
        auth_twofish_key = os.urandom(16)
        u_bytes = username.encode('ascii') + b'\x00'
        p_bytes = password.encode('ascii') + b'\x00'

        rsa_blob = bytearray([0x00])
        rsa_blob.extend(struct.pack("<I", 4))
        rsa_blob.extend(auth_twofish_key)
        rsa_blob.extend(struct.pack("<H", len(u_bytes)))
        rsa_blob.extend(u_bytes)
        rsa_blob.extend(struct.pack("<H", len(p_bytes)))
        rsa_blob.extend(p_bytes)

        encrypted_blob = server_pubkey.encrypt(
            bytes(rsa_blob),
            padding.OAEP(mgf=padding.MGF1(hashes.SHA1()), algorithm=hashes.SHA1(), label=None)
        )

        req_hdr = struct.pack("<II31sH", 4, 0, b'\x00'*31, len(encrypted_blob))
        send_var_packet(auth_sock, bytes([0x08]) + req_hdr + encrypted_blob)

        auth_reply = recv_var_packet(auth_sock)
        opcode, auth_result, unk1, user_id, off_auth_data, off_enc_data, unk2, unk3, off_char, off_world, off_user = struct.unpack_from("<BIHIHHHHHHH", auth_reply, 0)
        assert auth_result == 0, f"Authentication rejected by AuthServer (status code {auth_result})"

        ticket_len = struct.unpack_from("<H", auth_reply, off_auth_data)[0]
        ticket_bytes = auth_reply[off_auth_data + 2 : off_auth_data + 2 + ticket_len]

        enc_priv_len = struct.unpack_from("<H", auth_reply, off_enc_data)[0]
        enc_priv_bytes = auth_reply[off_enc_data + 2 : off_enc_data + 2 + enc_priv_len]
        dec_priv_exp = tf_decrypt_cbc(enc_priv_bytes, auth_twofish_key, iv=b'\x00'*16)

        signed_data_bytes = ticket_bytes[128:]
        user_modulus = int.from_bytes(signed_data_bytes[82 : 82 + 96], 'big')
        user_d = int.from_bytes(dec_priv_exp, 'big')

        print(f"    [AUTH OK] Authenticated as '{username}' (UID {user_id}). Ticket: {len(ticket_bytes)} bytes.")
        auth_sock.close()
        auth_sock = None

        # Step 2: Margin Handshake
        print(f"\n[+] Connecting to MarginServer at {host}:{margin_port}...")
        margin_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        margin_sock.settimeout(12.0)
        margin_sock.connect((host, margin_port))

        # CERT_ConnectRequest (0x01)
        send_var_packet(margin_sock, struct.pack("<BHH", 0x01, 3, 0x0136) + ticket_bytes)

        # Recv CERT_Challenge (0x02)
        cert_chal_raw = recv_var_packet(margin_sock)
        assert cert_chal_raw[0] == 0x02, f"Expected CERT_Challenge (0x02), got 0x{cert_chal_raw[0]:02X}"
        enc_output_len = struct.unpack_from("<H", cert_chal_raw, 3)[0]
        enc_output = cert_chal_raw[5 : 5 + enc_output_len]

        decrypted_chal = rsa_oaep_decrypt(enc_output, user_d, user_modulus)
        margin_twofish_key = decrypted_chal[1:17]
        server_challenge = decrypted_chal[17:33]
        print(f"    [MARGIN OK] CERT Challenge decrypted. Twofish Key: {margin_twofish_key.hex()[:8]}...")

        # CERT_ChallengeResponse (0x03) - Plaintext 17 bytes
        send_var_packet(margin_sock, bytes([0x03]) + server_challenge)

        # CERT_ConnectReply (0x04)
        cert_rep = unwrap_twofish_envelope(recv_var_packet(margin_sock), margin_twofish_key)
        assert cert_rep[0] == 0x04, "CERT_ConnectReply failed"

        # MS_ConnectRequest (0x06)
        ms_conn_req = struct.pack("<BII9s16sB", 0x06, 5668, 5668, b'\x00'*9, os.urandom(16), 0)
        send_var_packet(margin_sock, wrap_twofish_envelope(ms_conn_req, margin_twofish_key))

        # MS_ConnectChallenge (0x07)
        ms_chal = unwrap_twofish_envelope(recv_var_packet(margin_sock), margin_twofish_key)
        assert ms_chal[0] == 0x07, "MS_ConnectChallenge failed"

        # MS_ConnectChallengeResponse (0x08)
        send_var_packet(margin_sock, wrap_twofish_envelope(bytes([0x08]) + os.urandom(16), margin_twofish_key))

        # MS_ConnectReply (0x09)
        ms_rep = unwrap_twofish_envelope(recv_var_packet(margin_sock), margin_twofish_key)
        assert ms_rep[0] == 0x09, "MS_ConnectReply failed"
        session_id = struct.unpack_from("<I", ms_rep, 9)[0]
        print(f"    [MARGIN OK] Established Margin Session ID: {session_id} (0x{session_id:08X})")

        # Step 3: Character Claim & Creation
        target_handle = f"{handle_prefix}{int(time.time() * 10) % 1000000}"
        print(f"\n[+] Requesting character claim for '{target_handle}'...")

        # MS_ClaimCharacterNameRequest (0x0A)
        h_bytes = target_handle.encode('ascii') + b'\x00'
        claim_payload = struct.pack("<BHH", 0x0A, 0x0000, len(h_bytes)) + h_bytes
        send_var_packet(margin_sock, wrap_twofish_envelope(claim_payload, margin_twofish_key))

        # MS_ClaimCharacterNameReply (0x0B)
        claim_rep = unwrap_twofish_envelope(recv_var_packet(margin_sock), margin_twofish_key)
        assert claim_rep[0] == 0x0B, "MS_ClaimCharacterNameReply failed"
        claim_status = struct.unpack_from("<I", claim_rep, 3)[0]
        assert claim_status == 0, f"Claim failed with status {claim_status}"
        print(f"    [CLAIM OK] Handle '{target_handle}' successfully claimed.")

        # MS_CreateCharacterRequest (0x0C)
        print(f"[+] Sending MS_CreateCharacterRequest (0x0C) with RSI attributes...")
        create_buf = bytearray(80)
        create_buf[0] = 0x0C
        struct.pack_into("<H", create_buf, 2, 1)  # skintone
        struct.pack_into("<H", create_buf, 6, 1)  # bodyTypeId
        struct.pack_into("<H", create_buf, 14, 1) # hairId
        struct.pack_into("<H", create_buf, 18, 1) # haircolor
        struct.pack_into("<H", create_buf, 26, 1) # headId
        struct.pack_into("<H", create_buf, 38, 4) # eyewearId
        struct.pack_into("<H", create_buf, 42, 2) # shirtId
        struct.pack_into("<H", create_buf, 46, 6) # glovesId
        struct.pack_into("<H", create_buf, 50, 10)# outerwearId
        struct.pack_into("<H", create_buf, 54, 1) # pantsId
        struct.pack_into("<H", create_buf, 62, 6) # footwearId
        struct.pack_into("<H", create_buf, 66, 2) # profession

        fn_bytes = target_handle.encode('ascii') + b'\x00'
        ln_bytes = b"Operative\x00"
        create_buf.extend(struct.pack("<H", len(fn_bytes)) + fn_bytes)
        create_buf.extend(struct.pack("<H", len(ln_bytes)) + ln_bytes)

        send_var_packet(margin_sock, wrap_twofish_envelope(bytes(create_buf), margin_twofish_key))

        # Step 4: Stream all MS_LoadCharacterReply packets
        load_count = 0
        last_packet = False
        world_char_id = 0
        udp_world_port = 10000

        while not last_packet:
            pkt = unwrap_twofish_envelope(recv_var_packet(margin_sock), margin_twofish_key)
            assert pkt[0] == 0x10, f"Expected LoadReply (0x10), got 0x{pkt[0]:02X}"
            load_count += 1
            world_char_id, short_after_id, num_replies, is_last, sub_opcode = struct.unpack_from("<IHBBB", pkt, 5)
            if is_last:
                last_packet = True
                udp_world_port = 10000
        
        print(f"    [CREATION OK] Received {load_count} MS_LoadCharacterReply packets. Assigned CharId={world_char_id}, UDP Port={udp_world_port}.")

        # Step 5: Game UDP World Handshake
        print(f"\n[+] Transmitting 43-byte InitialUDPPacket to {host}:{udp_world_port}...")
        udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        udp_sock.settimeout(1.0)

        udp_pkt = bytearray(43)
        struct.pack_into("<Q", udp_pkt, 0x0B, world_char_id)
        session_block = struct.pack("<I12x", session_id)
        udp_pkt[27:43] = tf_encrypt_ecb(session_block, margin_twofish_key)

        # Send UDP initial packet (send twice 100ms apart to guard against WAN packet drop)
        margin_sock.settimeout(10.0)
        udp_sock.sendto(bytes(udp_pkt), (host, udp_world_port))
        time.sleep(0.1)
        udp_sock.sendto(bytes(udp_pkt), (host, udp_world_port))

        raw_reply = recv_var_packet(margin_sock)
        margin_udp_reply = unwrap_twofish_envelope(raw_reply, margin_twofish_key)
        assert margin_udp_reply[0] == 0x11, f"Expected MS_EstablishUDPSessionReply (0x11), got 0x{margin_udp_reply[0]:02X}"

        print(f"    [WORLD ENTRY OK] Received MS_EstablishUDPSessionReply (0x11) from MarginServer!")
        print(f"    >>> Character '{target_handle}' (ID: {world_char_id}) successfully entered MegaCity Simulation! <<<")
        return True

    finally:
        if auth_sock:
            try:
                auth_sock.close()
            except Exception:
                pass
        if margin_sock:
            try:
                margin_sock.close()
            except Exception:
                pass
        if udp_sock:
            try:
                udp_sock.close()
            except Exception:
                pass

def main():
    parser = argparse.ArgumentParser(description="MxOEmu Headless Character Creation & World Entry Suite")
    parser.add_argument("--host", default="15.204.82.250", help="Target server IP or hostname")
    parser.add_argument("--auth-port", type=int, default=11000, help="AuthServer TCP port")
    parser.add_argument("--margin-port", type=int, default=10000, help="MarginServer TCP port")
    parser.add_argument("--username", default="Slacker", help="Account username")
    parser.add_argument("--password", default="test", help="Account password")
    parser.add_argument("--prefix", default="Op", help="Character name prefix")
    parser.add_argument("--cycles", type=int, default=1, help="Number of consecutive test cycles")
    args = parser.parse_args()

    print("=" * 75)
    print("      MxOEmu HEADLESS CHARACTER CREATION & WORLD ENTRY SUITE")
    print(f"      Target: {args.host} | User: {args.username} | Cycles: {args.cycles}")
    print("=" * 75)

    success_count = 0
    for i in range(args.cycles):
        if args.cycles > 1:
            print(f"\n--- [Cycle {i+1} of {args.cycles}] ---")
        try:
            if run_test(args.host, args.auth_port, args.margin_port, args.username, args.password, args.prefix):
                success_count += 1
        except Exception as e:
            print(f"\n[!] TEST CYCLE {i+1} FAILED: {e}")
            import traceback
            traceback.print_exc()

        if i + 1 < args.cycles:
            time.sleep(1.0)

    print("\n" + "=" * 75)
    print(f"TEST RUN COMPLETED: {success_count}/{args.cycles} cycles passed (100% success rate required)")
    print("=" * 75)
    sys.exit(0 if success_count == args.cycles else 1)

if __name__ == "__main__":
    main()
