const std = @import("std");
const exec = @import("exec.zig");

pub fn main() !void {
    var dag1: exec.Echo = .{
        .output = "Starting pipeline...",
    };

    const argv: [2][]const u8 = .{ "echo", "hello world" };
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();

    var dag2 = exec.Command.init(gpa.allocator(), &argv);

    var dag3: exec.Echo = .{
        .output = "End of pipeline.",
    };

    const children = [_]*const exec.Node{
        &dag1.executionNode(),
        &dag2.executionNode(),
        &dag3.executionNode(),
    };

    var dag = exec.SequentialContainer(3).init();

    dag.children = children;
    _ = dag.executionNode().execute();
}
