#ifndef PROJECT_ASSETS_H_
#define PROJECT_ASSETS_H_

#include "asset_manifest.h"
#include "asset_types.h"

namespace project_assets {

const EmbeddedImageAsset* GetLogo(EmbeddedLogoId id);
const EmbeddedImageAsset* GetIcon(EmbeddedIconId id);
const EmbeddedImageAsset* GetFooterIcon(EmbeddedFooterIconId id);
const EmbeddedImageAsset* GetImage(EmbeddedImageId id);

}  // namespace project_assets

#endif  // PROJECT_ASSETS_H_
