//! DAG node implementations that form the core of RBS' execution model.

const std = @import("std");

/// Execution results can currently be success or failure which we use to gate logic
/// in the execution pipeline or provide notice back to the user.
const Result = enum { success, failure };

/// Nodes must provide a given VTable so they can be executed through the same API
/// in order to simplify handling across mixed types.
const VTable = struct { execute: *const fn (*anyopaque) Result };

/// The basic execution node, contains everything we expect each implementation to
/// provide.
pub const Node = struct {
    ptr: *anyopaque,
    vtable: *const VTable,

    pub fn execute(self: Node) Result {
        return self.vtable.execute(self.ptr);
    }
};

/// A node responsible for simply echoing its text arguments back to the user. The
/// result of this is always success.
pub const Echo = struct {
    const Self = @This();
    output: []const u8,

    pub fn executionNode(self: *Self) Node {
        return .{
            .ptr = self,
            .vtable = &.{
                .execute = execute,
            },
        };
    }

    fn execute(ctx: *anyopaque) Result {
        const self: *Self = @ptrCast(@alignCast(ctx));
        std.debug.print("{s}\n", .{self.output});

        return .success;
    }
};

/// A node responsible for executing a given command on the local system. Result
/// is depending on the exit status of the underlying command's execution.
pub const Command = struct {
    const Self = @This();
    allocator: std.mem.Allocator,
    argv: []const []const u8,

    pub fn executionNode(self: *Self) Node {
        return .{ .ptr = self, .vtable = &.{
            .execute = execute,
        } };
    }

    pub fn init(allocator: std.mem.Allocator, argv: []const []const u8) Self {
        return .{
            .allocator = allocator,
            .argv = argv,
        };
    }

    fn execute(ctx: *anyopaque) Result {
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
};

/// A node who's job it is to run all child items in sequence. When one fails, further
/// items will not be processed and the failure status will be returned immediately.
/// Otherwise, all itmes will be processed, returning success upon completion.
pub fn SequentialContainer(comptime Size: usize) type {
    return struct {
        const Self = @This();
        children: [Size]*const Node,

        pub fn init() Self {
            return .{ .children = undefined };
        }

        pub fn executionNode(self: *Self) Node {
            return .{ .ptr = self, .vtable = &.{
                .execute = execute,
            } };
        }

        fn execute(ctx: *anyopaque) Result {
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
