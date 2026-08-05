#include "app/ui_system_status_mailbox.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAILED] " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

} // namespace

int main() {
    smart_attendance::app::UiSystemStatusMailbox mailbox;
    smart_attendance::app::UiSystemStatusSnapshot snapshot;

    require(!mailbox.tryConsume(snapshot),
            "a new mailbox must not expose an uninitialized snapshot");

    mailbox.publishTime("08:00:00", "Mon");
    mailbox.publishTime("08:00:01", "Mon");
    require(mailbox.tryConsume(snapshot),
            "the latest time snapshot must be consumable");
    require(snapshot.timeText == "08:00:01" &&
                snapshot.weekdayText == "Mon" &&
                !snapshot.diskStatusKnown,
            "pending time updates must coalesce to the latest value");
    require(!mailbox.tryConsume(snapshot),
            "a snapshot must only be consumed once per version");

    mailbox.publishDiskStatus(true);
    mailbox.publishTime("08:00:02", "Mon");
    require(mailbox.tryConsume(snapshot),
            "time and disk updates must merge into one snapshot");
    require(snapshot.timeText == "08:00:02" && snapshot.diskStatusKnown &&
                snapshot.diskFull,
            "the merged snapshot must preserve the current disk warning");

    mailbox.publishDiskStatus(false);
    require(mailbox.tryConsume(snapshot) && !snapshot.diskFull,
            "a recovered disk state must replace the previous warning");

    std::cout << "[PASSED] UI system status mailbox coalescing\n";
    return EXIT_SUCCESS;
}
