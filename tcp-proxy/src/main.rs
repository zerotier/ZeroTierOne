use std::{
    net::{Ipv6Addr, SocketAddrV6},
    time::Duration,
};

use tokio::net::TcpListener;

const ZT_TCP_PROXY_CONNECTION_TIMEOUT_SECONDS: Duration = Duration::from_secs(300);
const ZT_TCP_PROXY_TCP_PORT: u16 = 443;

#[tokio::main]
async fn main() {
    let listener =
        socket2::Socket::new(socket2::Domain::IPV6, socket2::Type::STREAM, None).unwrap();
    listener.set_only_v6(false).expect("error setting V6ONLY");
    let _ = listener.set_linger(None);
    listener
        .bind(&SocketAddrV6::new(Ipv6Addr::UNSPECIFIED, ZT_TCP_PROXY_TCP_PORT, 0, 0).into())
        .expect("error binding to port");
    listener.listen(1024).expect("error listening");
    let listener = TcpListener::from_std(listener.into()).unwrap();

    loop {
        if let Ok((stream, from_address)) = listener.accept().await {
            tokio::task::spawn(async move {
                //
            });
        }
    }
}

/*
use std::collections::HashMap;
use std::net::{IpAddr, Ipv4Addr, SocketAddr};
use std::sync::Arc;
use std::time::{Duration, Instant};

use tokio::io::AsyncReadExt;
use tokio::net::{TcpListener, TcpStream, UdpSocket};
use tokio::sync::Mutex;

const ZT_TCP_PROXY_CONNECTION_TIMEOUT_SECONDS: u64 = 300;
const ZT_TCP_PROXY_TCP_PORT: u16 = 443;

struct Client {
    tcp_read_buf: Vec<u8>,
    tcp_write_buf: Vec<u8>,
    tcp_write_ptr: usize,
    tcp_read_ptr: usize,
    tcp: TcpStream,
    udp: Arc<UdpSocket>,
    last_activity: Instant,
    new_version: bool,
}

struct TcpProxyService {
    udp_port_counter: Arc<Mutex<u16>>,
    clients: Arc<Mutex<HashMap<SocketAddr, Client>>>,
}

impl TcpProxyService {
    async fn get_unused_udp(&self) -> Option<Arc<UdpSocket>> {
        for _ in 0..65535 {
            let mut port = self.udp_port_counter.lock().await;
            *port += 1;
            if *port > 0xfffe {
                *port = 1024;
            }
            let addr = SocketAddr::new(IpAddr::V4(Ipv4Addr::new(0, 0, 0, 0)), *port);
            match UdpSocket::bind(addr).await {
                Ok(udp) => return Some(Arc::new(udp)),
                Err(_) => continue,
            }
        }
        None
    }

    async fn handle_udp(&self, addr: SocketAddr, data: &[u8]) {
        if data.len() < 16 || data.len() >= 2048 {
            return;
        }

        let mut clients = self.clients.lock().await;
        if let Some(client) = clients.get_mut(&addr) {
            client.last_activity = Instant::now();

            let mlen = if client.new_version {
                data.len() + 7
            } else {
                data.len()
            };

            if client.tcp_write_ptr + 5 + mlen <= client.tcp_write_buf.len() {
                client.tcp_write_buf[client.tcp_write_ptr..client.tcp_write_ptr + 5]
                    .copy_from_slice(&[0x17, 0x03, 0x03, (mlen >> 8) as u8, mlen as u8]);
                client.tcp_write_ptr += 5;

                if client.new_version {
                    client.tcp_write_buf[client.tcp_write_ptr] = 4; // IPv4
                    client.tcp_write_ptr += 1;
                    if let IpAddr::V4(ip) = addr.ip() {
                        client.tcp_write_buf[client.tcp_write_ptr..client.tcp_write_ptr + 4]
                            .copy_from_slice(&ip.octets());
                        client.tcp_write_ptr += 4;
                    }
                    client.tcp_write_buf[client.tcp_write_ptr..client.tcp_write_ptr + 2]
                        .copy_from_slice(&addr.port().to_be_bytes());
                    client.tcp_write_ptr += 2;
                }

                client.tcp_write_buf[client.tcp_write_ptr..client.tcp_write_ptr + data.len()]
                    .copy_from_slice(data);
                client.tcp_write_ptr += data.len();

                if let Err(e) = client.tcp.writable().await {
                    eprintln!("Error waiting for TCP stream to be writable: {}", e);
                }
            }

            println!(
                "<< UDP {}:{} -> {:#016x}",
                addr.ip(),
                addr.port(),
                client as *const _ as usize
            );
        }
    }

    async fn handle_tcp(&self, mut stream: TcpStream, addr: SocketAddr) {
        let udp = match self.get_unused_udp().await {
            Some(udp) => udp,
            None => {
                println!("** TCP rejected, no more UDP ports to assign");
                return;
            }
        };

        let mut client = Client {
            tcp_read_buf: vec![0; 131072],
            tcp_write_buf: vec![0; 131072],
            tcp_write_ptr: 0,
            tcp_read_ptr: 0,
            tcp: stream.clone(),
            udp: udp.clone(),
            last_activity: Instant::now(),
            new_version: false,
        };

        let mut clients = self.clients.lock().await;
        clients.insert(addr, client);
        drop(clients);

        println!(
            "<< TCP from {} -> {:#016x}",
            addr, &client as *const _ as usize
        );

        let mut buf = [0; 4096];
        loop {
            match stream.read(&mut buf).await {
                Ok(0) => break,
                Ok(n) => {
                    let mut clients = self.clients.lock().await;
                    if let Some(client) = clients.get_mut(&addr) {
                        client.last_activity = Instant::now();

                        client.tcp_read_buf[client.tcp_read_ptr..client.tcp_read_ptr + n]
                            .copy_from_slice(&buf[..n]);
                        client.tcp_read_ptr += n;

                        while client.tcp_read_ptr >= 5 {
                            let mlen = (client.tcp_read_buf[3] as usize) << 8
                                | client.tcp_read_buf[4] as usize;
                            if client.tcp_read_ptr >= mlen + 5 {
                                if mlen == 4 {
                                    client.new_version = true;
                                    println!("<< TCP {:#016x} HELLO", client as *const _ as usize);
                                } else if mlen >= 7 {
                                    let payload = &client.tcp_read_buf[5..mlen + 5];
                                    let mut payload_len = mlen;

                                    let mut dest =
                                        SocketAddr::new(IpAddr::V4(Ipv4Addr::new(0, 0, 0, 0)), 0);
                                    if client.new_version {
                                        if payload[0] == 4 {
                                            let ip = Ipv4Addr::new(
                                                payload[1], payload[2], payload[3], payload[4],
                                            );
                                            let port = u16::from_be_bytes([payload[5], payload[6]]);
                                            dest = SocketAddr::new(IpAddr::V4(ip), port);
                                            payload_len -= 7;
                                        }
                                    } else {
                                        dest = SocketAddr::new(
                                            IpAddr::V4(Ipv4Addr::new(127, 0, 0, 1)),
                                            9993,
                                        );
                                    }

                                    if dest.port() > 1024 && payload_len >= 16 {
                                        if let Err(e) =
                                            udp.send_to(&payload[7..payload_len + 7], dest).await
                                        {
                                            eprintln!("Error sending UDP packet: {}", e);
                                        }
                                        println!(
                                            ">> TCP {:#016x} to {}:{}",
                                            client as *const _ as usize,
                                            dest.ip(),
                                            dest.port()
                                        );
                                    }
                                }

                                client.tcp_read_ptr -= mlen + 5;
                                client.tcp_read_buf.copy_within(mlen + 5.., 0);
                            } else {
                                break;
                            }
                        }
                    }
                }
                Err(e) => {
                    eprintln!("Error reading from TCP stream: {}", e);
                    break;
                }
            }
        }

        let mut clients = self.clients.lock().await;
        clients.remove(&addr);
        println!("** TCP {:#016x} closed", &client as *const _ as usize);
    }

    async fn housekeeping(&self) {
        let now = Instant::now();
        let mut clients = self.clients.lock().await;
        let mut to_close = Vec::new();

        for (addr, client) in clients.iter() {
            if now.duration_since(client.last_activity)
                >= Duration::from_secs(ZT_TCP_PROXY_CONNECTION_TIMEOUT_SECONDS)
            {
                to_close.push(*addr);
            }
        }

        for addr in to_close {
            clients.remove(&addr);
        }
    }

    async fn run(&self) {
        let listener = TcpListener::bind(SocketAddr::new(
            IpAddr::V4(Ipv4Addr::new(0, 0, 0, 0)),
            ZT_TCP_PROXY_TCP_PORT,
        ))
        .await
        .expect("Failed to bind TCP listener");

        println!(
            "TCP proxy server listening on {}",
            listener.local_addr().unwrap()
        );

        let udp = UdpSocket::bind("0.0.0.0:0")
            .await
            .expect("Failed to bind UDP socket");

        let mut buf = [0u8; 2048];
        loop {
            tokio::select! {
                Ok((stream, addr)) = listener.accept() => {
                    tokio::spawn(self.handle_tcp(stream, addr));
                }
                Ok((len, addr)) = udp.recv_from(&mut buf) => {
                    self.handle_udp(addr, &buf[..len]).await;
                }
                _ = tokio::time::sleep(Duration::from_secs(120)) => {
                    self.housekeeping().await;
                }
            }
        }
    }
}

#[tokio::main]
async fn main() {
    let service = TcpProxyService {
        udp_port_counter: Arc::new(Mutex::new(1023)),
        clients: Arc::new(Mutex::new(HashMap::new())),
    };

    service.run().await;
}
*/
