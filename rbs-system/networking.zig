/// Provides networking constants and funcionality to other modules

// NOTE(garrett): IPv6 has a mandated MTU of 1280 bytes (RFC 8200 Section 5).
// 40 of these are required for the IPv6 header (RFC 8200 Section 3) while
// another 8 (RFC 768 Introduction) are mandated for UDP traffic. This leaves
// us at a minimum datagram size of 1280 - 40 - 8 (1232) bytes within an IPv6
// packet. IPv4 packets support a much lower size (576 bytes, RFC 791) but
// modern networks almost always support a MTU of 1500 bytes so this number
// prevents us from dead space in V6 as well as almost any chance of
// fragmentation.
pub const DATAGRAM_SIZE: usize = 1232;
