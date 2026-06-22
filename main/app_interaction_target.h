#ifndef APP_INTERACTION_TARGET_H_
#define APP_INTERACTION_TARGET_H_

#include <cstdint>

namespace app_interaction {

enum class Owner : uint8_t {
    kNone = 0,
    kOverlay,
    kFooter,
    kPage,
};

enum class Kind : uint8_t {
    kNone = 0,
    kOverlayStorageModalAction,
    kOverlaySelectModalItem,
    kOverlayShutdownAction,
    kOverlayToastCloseAction,
    kFooterItem,
    kPageAction,
    kPageComposite,
    kPageListRow,
};

struct InteractiveTarget {
    Owner owner = Owner::kNone;
    Kind kind = Kind::kNone;
    int32_t primary_index = -1;
    int32_t secondary_index = -1;

    constexpr bool IsValid() const
    {
        return owner != Owner::kNone && kind != Kind::kNone;
    }
};

constexpr bool operator==(const InteractiveTarget& lhs, const InteractiveTarget& rhs)
{
    return lhs.owner == rhs.owner && lhs.kind == rhs.kind &&
           lhs.primary_index == rhs.primary_index &&
           lhs.secondary_index == rhs.secondary_index;
}

constexpr bool operator!=(const InteractiveTarget& lhs, const InteractiveTarget& rhs)
{
    return !(lhs == rhs);
}

}  // namespace app_interaction

#endif  // APP_INTERACTION_TARGET_H_
