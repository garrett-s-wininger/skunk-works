//! A queuing implementation for RBS, responsible for taking outstanding work items
//! and providing them to worker processes as their requests come in.

const std = @import("std");

const c = @cImport({
    @cInclude("signal.h");
    @cInclude("stdio.h");
    @cInclude("sys/socket.h");
    @cInclude("sys/un.h");
    @cInclude("unistd.h");
});

const socket_fs_path = "/tmp/rbs.sock";

/// Performs cleanup required during a graceful application shutdown.
fn cleanup(_: c_int) callconv(.C) void {
    _ = c.unlink(socket_fs_path);
    std.process.exit(0);
}

// NOTE(garrett): While not a perfect stand-in for C's SIG_IGN, we can't properly get MacOS'
// SIG_IGN value of (*void)1 to a 4-byte aligned pointer to cooperate with Zig's alignment
// checking. The net result is that we'll call a no-op function instead of no call at all,
// but this should be minimal enough that it's not an issue. Famous last words.
/// Empty function for signals which should effectively be a no-op.
fn ignore_signal(_: c_int) callconv(.C) void {}

pub fn main() !void {
    // NOTE(garrett): Prepare signal handling on POSIX-based systems
    var ignore_action: c.struct_sigaction = .{};
    ignore_action.__sigaction_u.__sa_handler = &ignore_signal;

    const ignore_signals = [_]c_int{ c.SIGHUP, c.SIGUSR1, c.SIGUSR2 };

    for (ignore_signals) |signal| {
        const handler_config_result = c.sigaction(signal, &ignore_action, null);

        if (handler_config_result == -1) {
            c.perror("ignore_sigaction");
            return error.SignalHandlerConfigurationFailure;
        }
    }

    var cleanup_action: c.struct_sigaction = .{};
    cleanup_action.__sigaction_u.__sa_handler = &cleanup;

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
