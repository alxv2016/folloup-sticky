#ifndef PROJECT_ASSET_MANIFEST_H_
#define PROJECT_ASSET_MANIFEST_H_

#include <cstdint>

enum class EmbeddedLogoId : uint8_t {
    kAlxvLabsLogo,
    kFollowupLogo,
};

enum class EmbeddedIconId : uint8_t {
    kAdd,
    kAiConnect,
    kApiKey,
    kAudio,
    kBattery1,
    kBattery2,
    kBattery3,
    kBattery4,
    kBattery5,
    kBattery6,
    kBle1,
    kBle2,
    kBle3,
    kCheck,
    kCheckboxChecked,
    kCheckboxUnchecked,
    kChevronLeft,
    kChevronRight,
    kSticky,
    kClose,
    kDelete,
    kFileAudio,
    kFileJson,
    kFileMd,
    kFileTranscript,
    kFile,
    kFolderOpen,
    kFolder,
    kGeminiApi,
    kGoal,
    kHome,
    kIdea,
    kInvisible,
    kKeyboard,
    kLock,
    kMicOff,
    kMicOn,
    kPin,
    kPower,
    kRefresh,
    kRemove,
    kSdCard,
    kSelect,
    kSettings,
    kSleep,
    kStar,
    kTaskStart,
    kTime,
    kTranscribe,
    kUsbStorage,
    kVisible,
    kWifi1,
    kWifi2,
    kWbar2,
    kWifi3,
    kWbar3,
    kWifi4,
    kWifiConfig,
};

enum class EmbeddedFooterIconId : uint8_t {
};

#endif  // PROJECT_ASSET_MANIFEST_H_
