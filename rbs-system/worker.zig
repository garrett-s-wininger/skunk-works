const std = @import("std");

const ExecutionResult = enum { success, failure };

const ExecutionNodeVTable = struct { execute: *const fn (*anyopaque) ExecutionResult };

const ExecutionNode = struct {
    ptr: *anyopaque,
    vtable: *const ExecutionNodeVTable,

    pub fn execute(self: ExecutionNode) ExecutionResult {
        return self.vtable.execute(self.ptr);
    }
};

const EchoNode = struct {
    const Self = @This();
    output: []const u8,

    fn executionNode(self: *Self) ExecutionNode {
        return .{
            .ptr = self,
            .vtable = &.{
                .execute = execute,
            },
        };
    }

    fn execute(ctx: *anyopaque) ExecutionResult {
        const self: *Self = @ptrCast(@alignCast(ctx));
        std.debug.print("{s}\n", .{self.output});

        return .success;
    }
};

const CommandNode = struct {
    const Self = @This();
    allocator: std.mem.Allocator,
    argv: []const []const u8,

    fn executionNode(self: *Self) ExecutionNode {
        return .{ .ptr = self, .vtable = &.{
            .execute = execute,
        } };
    }

    fn execute(ctx: *anyopaque) ExecutionResult {
        const self: *Self = @ptrCast(@alignCast(ctx));
        var process = std.process.Child.init(self.argv, self.allocator);

        process.stdout_behavior = .Inherit;
        process.stderr_behavior = .Inherit;

        process.spawn() catch {
            return .failure;
        };

        _ = process.wait() catch {
            return .failure;
        };

        return .success;
    }

    fn init(allocator: std.mem.Allocator, argv: []const []const u8) Self {
        return .{
            .allocator = allocator,
            .argv = argv,
        };
    }
};

fn SequentialNode(comptime Size: usize) type {
    return struct {
        const Self = @This();
        children: [Size]*const ExecutionNode,

        fn init() Self {
            return .{ .children = undefined };
        }

        fn executionNode(self: *Self) ExecutionNode {
            return .{ .ptr = self, .vtable = &.{
                .execute = execute,
            } };
        }

        fn execute(ctx: *anyopaque) ExecutionResult {
            const self: *Self = @ptrCast(@alignCast(ctx));

            for (self.children) |child| {
                switch (child.execute()) {
                    .success => continue,
                    .failure => return .failure,
                }
            }

            return .success;
        }
    };
}

pub fn main() !void {
    var dag1: EchoNode = .{
        .output = "Starting pipeline...",
    };

    const argv: [2][]const u8 = .{ "echo", "hello world" };
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();

    var dag2 = CommandNode.init(gpa.allocator(), &argv);

    var dag3: EchoNode = .{
        .output = "End of pipeline.",
    };

    const children = [_]*const ExecutionNode{
        &dag1.executionNode(),
        &dag2.executionNode(),
        &dag3.executionNode(),
    };

    var dag = SequentialNode(3).init();

    dag.children = children;
    _ = dag.executionNode().execute();
}
