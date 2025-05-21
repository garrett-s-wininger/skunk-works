const std = @import("std");

pub fn build(b: *std.Build) void {
    const worker = b.addExecutable(.{
        .name = "rbs-worker",
        .root_source_file = b.path("worker.zig"),
        .target = b.graph.host,
    });

    worker.linkLibC();
    b.installArtifact(worker);

    const queue = b.addExecutable(.{
        .name = "rbs-queue",
        .root_source_file = b.path("queue.zig"),
        .target = b.graph.host,
    });

    queue.linkLibC();
    b.installArtifact(queue);
}
