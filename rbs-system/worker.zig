const std = @import("std");

const ExecutionResult = enum { success, failure };

const ExecutionNode = struct {
    ptr: *anyopaque,
    executeFn: *const fn (ptr: *anyopaque) ExecutionResult,

    pub fn init(ptr: anytype) ExecutionNode {
        const T = @TypeOf(ptr);
        const ptr_info = @typeInfo(T);

        const gen = struct {
            pub fn execute(pointer: *anyopaque) ExecutionResult {
                const self: T = @ptrCast(@alignCast(pointer));
                return ptr_info.pointer.child.execute(self);
            }
        };

        return .{
            .ptr = ptr,
            .executeFn = gen.execute,
        };
    }

    pub fn execute(self: ExecutionNode) ExecutionResult {
        return self.executeFn(self.ptr);
    }
};

const EchoNode = struct {
    output: []const u8,

    fn execute(self: *EchoNode) ExecutionResult {
        std.debug.print("{s}\n", .{self.output});
        return ExecutionResult.success;
    }
};

pub fn main() !void {
    var echoNode: EchoNode = .{
        .output = "Hello world!",
    };

    var dag: ExecutionNode = ExecutionNode.init(&echoNode);
    _ = dag.execute();
}
