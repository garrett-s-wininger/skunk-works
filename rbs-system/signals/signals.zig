const std = @import("std");

extern fn ignore_signal(c_int) void;

/// Applies the SIG_IGN disposition to the requested signal.
pub fn ignore(signal_number: i32) void {
    ignore_signal(signal_number);
}
