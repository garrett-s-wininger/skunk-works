const std = @import("std");

const c = @cImport({
    @cInclude("stdio.h");
    @cInclude("sys/socket.h");
    @cInclude("sys/un.h");
    @cInclude("unistd.h");
});

pub fn main() !void {
    const connection_socket = c.socket(c.AF_UNIX, c.SOCK_DGRAM, 0);

    // NOTE(garrett): Initial socket config
    // TODO(garrett): More Zig-like interface should be possible for interfacing w/ POSIX
    if (connection_socket == -1) {
        c.perror("socket");
        return error.SocketCreationFailure;
    }

    defer _ = c.close(connection_socket);

    var unix_address: c.sockaddr_un = .{};
    unix_address.sun_family = c.AF_UNIX;

    const socket_fs_path = "/tmp/rbs.sock";
    @memcpy(unix_address.sun_path[0..socket_fs_path.len], socket_fs_path);
    unix_address.sun_path[socket_fs_path.len] = 0;

    // NOTE(garrett): Official binding
    const bind_result = c.bind(connection_socket, @ptrCast(&unix_address), @sizeOf(c.sockaddr_un));

    if (bind_result == -1) {
        c.perror("bind");
        return error.SocketBindFailure;
    }

    // FIXME(garrett): Under SIGINT (and TERM), this does not get cleaned up, we'll want to add into handler
    defer _ = c.unlink(socket_fs_path);
    var buff: [1024]u8 = undefined;

    // NOTE(garrett): Receiving
    while (true) {
        // TODO(garrett): Likely want to do recvmsg for more advanced handling
        const message_size = c.recv(connection_socket, @ptrCast(buff[0..]), buff.len, 0);
        std.debug.print("Received message of {d} bytes.\n", .{message_size});
    }

    std.debug.print("Reached successfully.\n", .{});
}
