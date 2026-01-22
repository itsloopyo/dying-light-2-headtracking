#include "pch.h"
#include "notification.h"
#include "core/logger.h"

namespace DL2HT {

void ShowNotification(const char* message) {
    Logger::Instance().Info("Notification: %s", message);
}

} // namespace DL2HT
