///! Shared location for access to native-C functionality for OS-specific tasks.
/// C header imports
pub const c = @cImport({
    @cInclude("signal.h");
    @cInclude("stdio.h");
    @cInclude("sys/socket.h");
    @cInclude("sys/un.h");
    @cInclude("unistd.h");
});
