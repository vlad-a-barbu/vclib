package main

import "core:log"
import "core:net"

serve :: proc(port: int, backlog: int) -> net.Network_Error {
	server_socket := net.listen_tcp(net.Endpoint{port = port, address = net.IP4_Any}) or_return
	net.set_blocking(server_socket, false) or_return
	defer net.close(server_socket)
	log.infof("listening on port %d\n", port)

	clients := [dynamic]net.TCP_Socket{}
	defer delete(clients)
	buff := [0x100]byte{}
	for {
		accepted_client, _, accept_err := net.accept_tcp(server_socket)
		switch accept_err {
		case net.Accept_Error.Would_Block:
			for client, index in clients {
				n, recv_err := net.recv_tcp(client, buff[:])
				switch recv_err {
				case net.TCP_Recv_Error.Timeout:
					continue
				case nil:
					if n == 0 {
						net.close(client)
						unordered_remove(&clients, index)
					} else {
						if _, err := net.send_tcp(client, buff[:n]); err != nil {
							log.errorf("send err: %v\n", recv_err)
							net.close(client)
							unordered_remove(&clients, index)
						}
					}
					break
				case:
					log.errorf("recv err: %v\n", recv_err)
					net.close(client)
					unordered_remove(&clients, index)
				}
			}
		case nil:
			append(&clients, accepted_client)
		case:
			log.errorf("accept err: %v\n", accept_err)
		}
	}
}

main :: proc() {
	cl := log.create_console_logger()
	defer log.destroy_console_logger(cl)
	context.logger = cl

	if err := serve(4200, 10); err != nil {
		log.errorf("serve err: %v\n", err)
	}
}
