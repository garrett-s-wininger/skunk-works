//! Supervisory process for local RBS execution to allow a proper startup/shutdown sequence
//! for the various child processes involved.

const c = @import("ffi.zig").c;
const std = @import("std");

const SupervisedProcess = enum { queue, worker };

const process_type_names = [_][]const u8{ "Queue", "Worker" };

const SubprocessContext = struct { path: []const u8, identifier: SupervisedProcess };

const subprocesses = [_]SubprocessContext{ .{ .path = "zig-out/bin/rbs-queue", .identifier = .queue }, .{ .path = "zig-out/bin/rbs-worker", .identifier = .worker } };

/// Launch a child process that will be supervised, ensuring the application does not
/// terminate until each one has exited.
fn launchSubprocess(working_directory: []const u8, process: SubprocessContext, allocator: std.mem.Allocator) !i32 {
    // NOTE(garrett): Combine our working directory and the relative paths for our
    // child processes so that we can properly start them via `exec` (on POSIX systems)
    const binary_path = try allocator.alloc(u8, working_directory.len + process.path.len + 2);
    defer allocator.free(binary_path);

    @memcpy(binary_path[0..working_directory.len], working_directory);
    binary_path[working_directory.len] = '/';
    @memcpy(binary_path[working_directory.len + 1 .. binary_path.len - 1], process.path);
    binary_path[binary_path.len - 1] = 0;

    const child_pid = c.fork();

    // TODO(garrett): Handle -1 ret val indicating a failure to fork
    if (child_pid == 0) {
        const exec_result = c.execl(@ptrCast(std.mem.sliceTo(binary_path, 0)), null);

        if (exec_result == -1) {
            std.process.exit(1);
        }
    }

    return child_pid;
}

pub fn main() !void {
    // NOTE(garrett): Validate at compile-time that we haven't set up our type name matching
    // correctly
    if (comptime process_type_names.len != @typeInfo(SupervisedProcess).@"enum".fields.len) {
        @compileError("Mismatch detected between supervised process type and name counts.");
    }

    const current_pid = c.getpid();
    std.log.info("Supervisor launched as PID {d}.", .{current_pid});

    // NOTE(garrett): Begin with configuring our fixed buffer for allocations
    // within the program
    var buffer: [4096]u8 = undefined;
    var fixed_allocator = std.heap.FixedBufferAllocator.init(&buffer);
    const allocator = fixed_allocator.allocator();

    // NOTE(garrett): Gather our current working directory for combining with other
    // string slices for child binary location(s)
    const working_directory = try std.process.getCwdAlloc(allocator);
    defer allocator.free(working_directory);

    for (subprocesses) |process| {
        std.log.info("Launching {s} process...", .{process_type_names[@intFromEnum(process.identifier)]});
        const child_pid = try launchSubprocess(working_directory, process, allocator);
        std.log.info("{s} process forked as PID {d}.", .{ process_type_names[@intFromEnum(process.identifier)], child_pid });

        // FIXME(garrett): Ensure resources actually created before moving onto the next
    }

    // FIXME(garrett): Institute signal handler to forward signals to child processes

    while (c.wait(null) > 0) {}
    std.log.info("All child processes exited.", .{});
}
