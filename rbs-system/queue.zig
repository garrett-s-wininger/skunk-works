//! A queuing implementation for RBS, responsible for taking outstanding work items
//! and providing them to worker processes as their requests come in.

const builtin = @import("builtin");
const c = @import("ffi.zig").c;
const networking = @import("networking.zig");
const signals = @import("signals/signals.zig");
const std = @import("std");

const socket_fs_path = "/tmp/rbs.sock";

/// Performs cleanup required during a graceful application shutdown.
fn cleanup(_: c_int) callconv(.c) void {
    _ = c.unlink(socket_fs_path);
    std.process.exit(0);
}

pub fn main() !void {
    // NOTE(garrett): Prepare signal handling on POSIX-based systems
    const ignore_signals = [_]c_int{ c.SIGHUP, c.SIGUSR1, c.SIGUSR2 };

    for (ignore_signals) |signal| {
        signals.ignore(signal);
    }

    var cleanup_action: c.struct_sigaction = .{};

    if (builtin.os.tag == .macos) {
        cleanup_action.__sigaction_u.__sa_handler = &cleanup;
    } else if (builtin.os.tag == .linux) {
        cleanup_action.__sigaction_handler.sa_handler = &cleanup;
    }

    const cleanup_signals = [_]c_int{ c.SIGINT, c.SIGTERM };

    for (cleanup_signals) |signal| {
        const cleanup_config_result = c.sigaction(signal, &cleanup_action, null);

        if (cleanup_config_result == -1) {
            c.perror("cleanup_sigaction");
            return error.SignalHandlerConfigurationFailure;
        }
    }

    // NOTE(garrett): Initial socket config
    // TODO(garrett): More Zig-like interface should be possible for interfacing w/ POSIX
    const connection_socket = c.socket(c.AF_UNIX, c.SOCK_DGRAM, 0);

    if (connection_socket == -1) {
        c.perror("socket");
        return error.SocketCreationFailure;
    }

    defer _ = c.close(connection_socket);

    var unix_address: c.sockaddr_un = .{};
    unix_address.sun_family = c.AF_UNIX;

    @memcpy(unix_address.sun_path[0..socket_fs_path.len], socket_fs_path);
    unix_address.sun_path[socket_fs_path.len] = 0;

    // NOTE(garrett): Official binding
    const bind_result = c.bind(connection_socket, @ptrCast(&unix_address), @sizeOf(c.sockaddr_un));

    if (bind_result == -1) {
        c.perror("bind");
        return error.SocketBindFailure;
    }

    defer _ = c.unlink(socket_fs_path);
    var buff: [networking.DATAGRAM_SIZE]u8 = undefined;

    // NOTE(garrett): Receiving
    while (true) {
        // TODO(garrett): Likely want to do recvmsg for more advanced handling
        const message_size = c.recv(connection_socket, @ptrCast(buff[0..]), buff.len, 0);
        std.debug.print("Received message of {d} bytes.\n", .{message_size});
    }

    std.debug.print("Reached successfully.\n", .{});
}
