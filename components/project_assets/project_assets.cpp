#include "project_assets.h"

#include "generated_epaper_logos.h"
#include "generated_epaper_icons.h"
#include "generated_epaper_footer_icons.h"
#include "generated_epaper_images.h"

namespace project_assets {

const EmbeddedImageAsset* GetLogo(EmbeddedLogoId id)
{
    switch (id) {
        case EmbeddedLogoId::kAlxvLabsLogo:
            return &epaper_logos::kAlxvLabsLogo;
        case EmbeddedLogoId::kFollowupLogo:
            return &epaper_logos::kFollowupLogo;
        default:
            return nullptr;
    }
}

const EmbeddedImageAsset* GetIcon(EmbeddedIconId id)
{
    switch (id) {
        case EmbeddedIconId::kAdd:
            return &epaper_icons::kAdd;
        case EmbeddedIconId::kAiConnect:
            return &epaper_icons::kAiConnect;
        case EmbeddedIconId::kApiKey:
            return &epaper_icons::kApiKey;
        case EmbeddedIconId::kAudio:
            return &epaper_icons::kAudio;
        case EmbeddedIconId::kBattery1:
            return &epaper_icons::kBattery1;
        case EmbeddedIconId::kBattery2:
            return &epaper_icons::kBattery2;
        case EmbeddedIconId::kBattery3:
            return &epaper_icons::kBattery3;
        case EmbeddedIconId::kBattery4:
            return &epaper_icons::kBattery4;
        case EmbeddedIconId::kBattery5:
            return &epaper_icons::kBattery5;
        case EmbeddedIconId::kBattery6:
            return &epaper_icons::kBattery6;
        case EmbeddedIconId::kBle1:
            return &epaper_icons::kBle1;
        case EmbeddedIconId::kBle2:
            return &epaper_icons::kBle2;
        case EmbeddedIconId::kBle3:
            return &epaper_icons::kBle3;
        case EmbeddedIconId::kCheck:
            return &epaper_icons::kCheck;
        case EmbeddedIconId::kCheckboxChecked:
            return &epaper_icons::kCheckboxChecked;
        case EmbeddedIconId::kCheckboxUnchecked:
            return &epaper_icons::kCheckboxUnchecked;
        case EmbeddedIconId::kChevronLeft:
            return &epaper_icons::kChevronLeft;
        case EmbeddedIconId::kChevronRight:
            return &epaper_icons::kChevronRight;
        case EmbeddedIconId::kSticky:
            return &epaper_icons::kSticky;
        case EmbeddedIconId::kClose:
            return &epaper_icons::kClose;
        case EmbeddedIconId::kDelete:
            return &epaper_icons::kDelete;
        case EmbeddedIconId::kFileAudio:
            return &epaper_icons::kFileAudio;
        case EmbeddedIconId::kFileJson:
            return &epaper_icons::kFileJson;
        case EmbeddedIconId::kFileMd:
            return &epaper_icons::kFileMd;
        case EmbeddedIconId::kFileTranscript:
            return &epaper_icons::kFileTranscript;
        case EmbeddedIconId::kFile:
            return &epaper_icons::kFile;
        case EmbeddedIconId::kFolderOpen:
            return &epaper_icons::kFolderOpen;
        case EmbeddedIconId::kFolder:
            return &epaper_icons::kFolder;
        case EmbeddedIconId::kGeminiApi:
            return &epaper_icons::kGeminiApi;
        case EmbeddedIconId::kGoal:
            return &epaper_icons::kGoal;
        case EmbeddedIconId::kHome:
            return &epaper_icons::kHome;
        case EmbeddedIconId::kIdea:
            return &epaper_icons::kIdea;
        case EmbeddedIconId::kInvisible:
            return &epaper_icons::kInvisible;
        case EmbeddedIconId::kKeyboard:
            return &epaper_icons::kKeyboard;
        case EmbeddedIconId::kLock:
            return &epaper_icons::kLock;
        case EmbeddedIconId::kMicOff:
            return &epaper_icons::kMicOff;
        case EmbeddedIconId::kMicOn:
            return &epaper_icons::kMicOn;
        case EmbeddedIconId::kPin:
            return &epaper_icons::kPin;
        case EmbeddedIconId::kPower:
            return &epaper_icons::kPower;
        case EmbeddedIconId::kRefresh:
            return &epaper_icons::kRefresh;
        case EmbeddedIconId::kRemove:
            return &epaper_icons::kRemove;
        case EmbeddedIconId::kSdCard:
            return &epaper_icons::kSdCard;
        case EmbeddedIconId::kSelect:
            return &epaper_icons::kSelect;
        case EmbeddedIconId::kSettings:
            return &epaper_icons::kSettings;
        case EmbeddedIconId::kSleep:
            return &epaper_icons::kSleep;
        case EmbeddedIconId::kStar:
            return &epaper_icons::kStar;
        case EmbeddedIconId::kTaskStart:
            return &epaper_icons::kTaskStart;
        case EmbeddedIconId::kTime:
            return &epaper_icons::kTime;
        case EmbeddedIconId::kTranscribe:
            return &epaper_icons::kTranscribe;
        case EmbeddedIconId::kUsbStorage:
            return &epaper_icons::kUsbStorage;
        case EmbeddedIconId::kVisible:
            return &epaper_icons::kVisible;
        case EmbeddedIconId::kWifi1:
            return &epaper_icons::kWifi1;
        case EmbeddedIconId::kWifi2:
            return &epaper_icons::kWifi2;
        case EmbeddedIconId::kWbar2:
            return &epaper_icons::kWbar2;
        case EmbeddedIconId::kWifi3:
            return &epaper_icons::kWifi3;
        case EmbeddedIconId::kWbar3:
            return &epaper_icons::kWbar3;
        case EmbeddedIconId::kWifi4:
            return &epaper_icons::kWifi4;
        case EmbeddedIconId::kWifiConfig:
            return &epaper_icons::kWifiConfig;
        default:
            return nullptr;
    }
}

const EmbeddedImageAsset* GetFooterIcon(EmbeddedFooterIconId id)
{
    switch (id) {
        default:
            return nullptr;
    }
}

const EmbeddedImageAsset* GetImage(EmbeddedImageId id)
{
    switch (id) {
        case EmbeddedImageId::kSlide1:
            return &epaper_images::kSlide1;
        case EmbeddedImageId::kSlide2:
            return &epaper_images::kSlide2;
        case EmbeddedImageId::kSlide3:
            return &epaper_images::kSlide3;
        case EmbeddedImageId::kSlide4:
            return &epaper_images::kSlide4;
        case EmbeddedImageId::kSlide5:
            return &epaper_images::kSlide5;
        case EmbeddedImageId::kSlide6:
            return &epaper_images::kSlide6;
        default:
            return nullptr;
    }
}

}  // namespace project_assets
