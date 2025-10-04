const std = @import("std");

pub fn build(b: *std.Build) void {
    const signalLib = b.addLibrary(.{ .name = "signals", .linkage = .static, .root_module = b.createModule(.{
        .root_source_file = b.path("signals/signals.zig"),
        .target = b.graph.host,
    }) });

    signalLib.addCSourceFile(.{
        .file = .{ .cwd_relative = "signals/signals.c" },
        .flags = &.{ "-Wall", "-Werror" }
    });

    const worker = b.addExecutable(.{ .name = "rbs-worker", .root_module = b.createModule(.{
        .root_source_file = b.path("worker.zig"),
        .target = b.graph.host,
    }) });

    b.installArtifact(worker);

    const queue = b.addExecutable(.{ .name = "rbs-queue", .root_module = b.createModule(.{
        .root_source_file = b.path("queue.zig"),
        .target = b.graph.host,
    }) });

    queue.linkLibrary(signalLib);
    b.installArtifact(queue);

    const supervisor = b.addExecutable(.{ .name = "rbs-supervisor", .root_module = b.createModule(.{
        .root_source_file = b.path("supervisor.zig"),
        .target = b.graph.host,
    }) });

    b.installArtifact(supervisor);
}
