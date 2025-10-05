//! A queuing implementation for RBS, responsible for taking outstanding work items
//! and providing them to worker processes as their requests come in.

const builtin = @import("builtin");
const c = @import("ffi.zig").c;
const networking = @import("networking.zig");
const signals = @import("signals/signals.zig");
const std = @import("std");

const socket_fs_path = networking.LOCAL_QUEUE_SOCKET;
var socket: networking.UnixSocket = undefined;

/// Performs cleanup required during a graceful application shutdown.
fn cleanup(_: c_int) callconv(.c) void {
    socket.close();
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
    socket = try networking.create_unix_socket(socket_fs_path);
    defer socket.close();

    var buff: [networking.DATAGRAM_SIZE]u8 = undefined;

    // NOTE(garrett): Main queue logic
    while (true) {
        const message_size = socket.receive(&buff);
        std.debug.print("Received message of {d} bytes.\n", .{message_size});
    }

    std.debug.print("Reached successfully.\n", .{});
}
