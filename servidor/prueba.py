import socket
import struct

# Replica exacta del ChatPacket:
# uint8_t command     → 1 byte  → B
# uint16_t payload_len → 2 bytes → H
# char sender[32]     → 32 bytes → 32s
# char target[32]     → 32 bytes → 32s
# char payload[957]   → 957 bytes → 957s
# TOTAL = 1024 bytes

FORMAT = '=BH32s32s957s'  # '=' = sin padding del compilador (como packed)

CMD_REGISTER  = 1
CMD_BROADCAST = 2
CMD_LIST      = 4
CMD_LOGOUT    = 7

def make_packet(cmd, sender='', target='', payload=''):
    return struct.pack(FORMAT,
        cmd,
        len(payload.encode()),
        sender.encode()[:32].ljust(32, b'\x00'),
        target.encode()[:32].ljust(32, b'\x00'),
        payload.encode()[:957].ljust(957, b'\x00')
    )

def parse_packet(data):
    cmd, plen, sender, target, payload = struct.unpack(FORMAT, data)
    return {
        'command': cmd,
        'sender':  sender.rstrip(b'\x00').decode(),
        'target':  target.rstrip(b'\x00').decode(),
        'payload': payload[:plen].decode() if plen > 0 else ''
    }

# Conectar 
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(('127.0.0.1', 8080))

# Registrarse 
s.sendall(make_packet(CMD_REGISTER, sender='alice', payload='alice'))
resp = parse_packet(s.recv(1024))
print('REGISTRO:', resp)

# Pedir lista de usuarios 
s.sendall(make_packet(CMD_LIST, sender='alice'))
resp = parse_packet(s.recv(1024))
print('LISTA:', resp)

# Broadcast 
s.sendall(make_packet(CMD_BROADCAST, sender='alice', payload='Hola mundo!'))
resp = parse_packet(s.recv(1024))
print('BROADCAST (eco):', resp)

# Logout 
s.sendall(make_packet(CMD_LOGOUT, sender='alice'))
resp = parse_packet(s.recv(1024))
print('LOGOUT:', resp)

s.close()
