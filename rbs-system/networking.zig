//! Provides networking constants and funcionality to other modules

const builtin = @import("builtin");
const c = @import("ffi.zig").c;

// NOTE(garrett): IPv6 has a mandated MTU of 1280 bytes (RFC 8200 Section 5).
// 40 of these are required for the IPv6 header (RFC 8200 Section 3) while
// another 8 (RFC 768 Introduction) are mandated for UDP traffic. This leaves
// us at a minimum datagram size of 1280 - 40 - 8 (1232) bytes within an IPv6
// packet. IPv4 packets support a much lower size (576 bytes, RFC 791) but
// modern networks almost always support a MTU of 1500 bytes so this number
// prevents us from dead space in V6 as well as almost any chance of
// fragmentation.
pub const DATAGRAM_SIZE: usize = 1232;

pub const LOCAL_QUEUE_SOCKET = switch (builtin.os.tag) {
    .linux => "/run/rbs-queue.sock",
    .macos => "/tmp/rbs-queue.sock",
    else => @compileError("Built on unsupported operating system"),
};

pub const SocketError = error{ CreationFailure, BindFailure };

pub const UnixSocket = struct {
    path: [:0]const u8,
    socket_fd: c_int,

    pub fn receive(self: @This(), buffer: []u8) isize {
        // TODO(garrett): Likely want to do recvmsg for more advanced handling
        return c.recv(self.socket_fd, @ptrCast(buffer[0..]), buffer.len, 0);
    }

    pub fn close(self: @This()) void {
        _ = c.unlink(self.path);
    }
};

pub fn create_unix_socket(path: [:0]const u8) SocketError!UnixSocket {
    const connection_socket = c.socket(c.AF_UNIX, c.SOCK_DGRAM, 0);

    if (connection_socket == -1) {
        return SocketError.CreationFailure;
    }

    var unix_address: c.sockaddr_un = .{};
    unix_address.sun_family = c.AF_UNIX;

    @memcpy(unix_address.sun_path[0..path.len], path);
    unix_address.sun_path[path.len] = 0;

    // NOTE(garrett): Official binding
    const bind_result = c.bind(connection_socket, @ptrCast(&unix_address), @sizeOf(c.sockaddr_un));

    if (bind_result == -1) {
        return SocketError.BindFailure;
    }

    return UnixSocket{ .path = path, .socket_fd = connection_socket };
}
