//! A worker implementation for RBS that periodically reaches out to a queuing endpoint in
//! order to obtain any available work.

const c = @import("ffi.zig").c;
const std = @import("std");

pub fn main() !void {
    // NOTE(garrett): Basic socket creation
    const connection_socket = c.socket(c.AF_UNIX, c.SOCK_DGRAM, 0);

    if (connection_socket == -1) {
        c.perror("socket");
        return error.SocketCreationFailure;
    }

    defer _ = c.close(connection_socket);

    // NOTE(garrett): Address definition
    var socket_address: c.sockaddr_un = .{};

    socket_address.sun_family = c.AF_UNIX;
    const socket_fs_path = "/tmp/rbs.sock";
    @memcpy(socket_address.sun_path[0..socket_fs_path.len], socket_fs_path);

    // NOTE(garrett): Connection
    const connection_result = c.connect(connection_socket, @ptrCast(&socket_address), @sizeOf(c.sockaddr_un));

    if (connection_result == -1) {
        c.perror("connect");
        return error.ConnectionFailure;
    }

    // NOTE(garrett): Message pass
    const message: [1024]u8 = undefined;

    // TODO(garrett): Use sendmsg for more control
    const send_result = c.send(connection_socket, @ptrCast(message[0..]), message.len, 0);

    if (send_result == -1) {
        c.perror("send");
        return error.SendFailure;
    }
}
