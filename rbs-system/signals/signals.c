#include <signal.h>

/**
 * Applies the SIG_IGN disposition to the provided signal.
 */
void ignore_signal(int signal_number) {
    signal(signal_number, SIG_IGN);
}
