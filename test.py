import socket

HOST = '127.0.0.1'
PORT = 8080

def start_mock_server():
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind((HOST, PORT))
    server_socket.listen(1)
    print(f"[*] Mock BitTorrent server listening on {HOST}:{PORT}...")

    conn, addr = server_socket.accept()
    print(f"[+] Client connected from {addr[0]}:{addr[1]}")

    try:
        # 1. Read exact 68-byte handshake payload
        data = bytearray()
        while len(data) < 68:
            chunk = conn.recv(68 - len(data))
            if not chunk:
                print("[-] Client disconnected prematurely.")
                return
            data.extend(chunk)

        print(f"[+] Handshake received from C++ client.")

        # 2. Echo handshake response
        client_info_hash = data[28:48]
        server_peer_id = b"-PY0001-987654321098"
        
        response = bytearray([19]) + b"BitTorrent protocol" + (b"\x00" * 8) + client_info_hash + server_peer_id
        conn.sendall(response)

        # 3. Send Bitfield Message (ID = 5)
        msg_payload = b"\xFF\x00\xF0\x0F"
        msg_length = (1 + len(msg_payload)).to_bytes(4, byteorder='big')
        msg_id = b"\x05"
        conn.sendall(msg_length + msg_id + msg_payload)

        # 4. Receive Interested Message from C++ client
        client_msg = bytearray()
        while len(client_msg) < 5:
            chunk = conn.recv(5 - len(client_msg))
            if not chunk: break
            client_msg.extend(chunk)

        if client_msg == b"\x00\x00\x00\x01\x02":
            # Send Unchoke Message (Length = 1, ID = 1)
            unchoke_msg = (1).to_bytes(4, byteorder='big') + b"\x01"
            conn.sendall(unchoke_msg)
            print("[+] Sent Unchoke message to client.")

        # 5. Serve piece blocks requested by client (4 requests for 64 KiB piece)
        for _ in range(4):
            req_buffer = bytearray()
            while len(req_buffer) < 17:
                chunk = conn.recv(17 - len(req_buffer))
                if not chunk: break
                req_buffer.extend(chunk)

            if len(req_buffer) < 17:
                break

            req_length = int.from_bytes(req_buffer[0:4], byteorder='big')
            req_id = req_buffer[4]
            idx = int.from_bytes(req_buffer[5:9], byteorder='big')
            begin = int.from_bytes(req_buffer[9:13], byteorder='big')
            length = int.from_bytes(req_buffer[13:17], byteorder='big')

            print(f"[+] Received Request: Index={idx}, Begin={begin}, Length={length}")

            if req_id == 6:
                # Send Piece Message Response (ID = 7)
                dummy_data = b"A" * length
                piece_payload = idx.to_bytes(4, byteorder='big') + begin.to_bytes(4, byteorder='big') + dummy_data
                piece_msg = (1 + len(piece_payload)).to_bytes(4, byteorder='big') + b"\x07" + piece_payload
                conn.sendall(piece_msg)
                print(f"[+] Transmitted Piece block ({len(dummy_data)} bytes).")

    except Exception as e:
        print(f"[-] Error handling client: {e}")
    finally:
        conn.close()
        server_socket.close()
        print("[*] Server shut down cleanly.")

if __name__ == "__main__":
    start_mock_server()