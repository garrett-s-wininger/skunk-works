const std = @import("std");

pub fn build(b: *std.Build) void {
    const worker = b.addExecutable(.{
        .name = "rbs-worker",
        .root_source_file = b.path("worker.zig"),
        .target = b.graph.host,
    });

    b.installArtifact(worker);
}
