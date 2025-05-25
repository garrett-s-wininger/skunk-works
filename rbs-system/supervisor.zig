//! Supervisory process for local RBS execution to allow a proper startup/shutdown sequence
//! for the various child processes involved.

const c = @import("ffi.zig").c;
const std = @import("std");

pub fn main() !void {
    // Begin with configuring our fixed buffer for allocations within the program
    var buffer: [1024]u8 = undefined;
    var fixed_allocator = std.heap.FixedBufferAllocator.init(&buffer);
    const allocator = fixed_allocator.allocator();

    // TODO(garrett): This method limits us to being in the root source directory for
    // running our application which is not desirable in a final product. We'll want
    // to update this so that we gather the full binary path and assume that our
    // companion programs are in the same directory so that we can run from any CWD
    // on the system.

    // Gather our current working directory for combining with other string slices for
    // child binary location(s)
    const working_directory = try std.process.getCwdAlloc(allocator);
    defer allocator.free(working_directory);

    // Combine our working directory and the relative paths for our child processes
    // so that we can properly start them via `exec` (on POSIX systems)
    const relative_queue_binary = "/zig-out/bin/rbs-queue";
    const queue_binary = try allocator.alloc(u8, working_directory.len + relative_queue_binary.len + 1);
    queue_binary[queue_binary.len - 1] = 0;
    defer allocator.free(queue_binary);

    @memcpy(queue_binary[0..working_directory.len], working_directory);
    @memcpy(queue_binary[working_directory.len .. queue_binary.len - 1], relative_queue_binary);

    const relative_worker_binary = "/zig-out/bin/rbs-worker";
    const worker_binary = try allocator.alloc(u8, working_directory.len + relative_worker_binary.len + 1);
    worker_binary[worker_binary.len - 1] = 0;
    defer allocator.free(worker_binary);

    @memcpy(worker_binary[0..working_directory.len], working_directory);
    @memcpy(worker_binary[working_directory.len .. worker_binary.len - 1], relative_worker_binary);

    const queue_pid = c.fork();

    // TODO(garrett): Handle -1 ret val indicating a failure to fork
    if (queue_pid == 0) {
        const exec_result = c.execl(@ptrCast(std.mem.sliceTo(queue_binary, 0)), null);

        if (exec_result == -1) {
            c.perror("queue initialization");
            std.process.exit(1);
        }
    }

    // TODO(garrett): Wait for a certain amount of time to ensure that our IPC mechanisms are fully
    // configured

    const worker_pid = c.fork();

    // TODO(garrett): Handle -1 ret val indicating a failure to fork
    if (worker_pid == 0) {
        const exec_result = c.execl(@ptrCast(std.mem.sliceTo(worker_binary, 0)), null);

        if (exec_result == -1) {
            c.perror("worker initialization");
            std.process.exit(1);
        }
    }

    // TODO(garrett): Institute signal handler to forward signals to child processes

    while (c.wait(null) > 0) {}
    std.debug.print("All child processes exited.\n", .{});
}
