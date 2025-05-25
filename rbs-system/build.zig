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

    const supervisor = b.addExecutable(.{
        .name = "rbs-supervisor",
        .root_source_file = b.path("supervisor.zig"),
        .target = b.graph.host,
    });

    supervisor.linkLibC();
    b.installArtifact(supervisor);
}
