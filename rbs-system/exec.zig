const std = @import("std");

const Result = enum { success, failure };
const VTable = struct { execute: *const fn (*anyopaque) Result };

pub const Node = struct {
    ptr: *anyopaque,
    vtable: *const VTable,

    pub fn execute(self: Node) Result {
        return self.vtable.execute(self.ptr);
    }
};

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
