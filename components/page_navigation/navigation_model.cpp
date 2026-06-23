#include "page_navigation/navigation_model.h"

namespace page_navigation {
namespace {

void AddItem(NavigationModel& model,
             NavigationItemSection section,
             NavigationItemRole role,
             int item_index)
{
    model.items.push_back({
        .section = section,
        .role = role,
        .item_index = item_index,
    });
    model.item_count = static_cast<int>(model.items.size());
}

}  // namespace

const NavigationItemDescriptor* NavigationModel::ItemAt(int index) const
{
    if (index < 0 || index >= item_count) {
        return nullptr;
    }
    return &items[static_cast<size_t>(index)];
}

int NavigationModel::IndexOfRole(NavigationItemRole role) const
{
    for (int index = 0; index < item_count; ++index) {
        if (items[static_cast<size_t>(index)].role == role) {
            return index;
        }
    }
    return -1;
}

bool NavigationModel::IsRoleSelected(int selected_index, NavigationItemRole role) const
{
    const NavigationItemDescriptor* item = ItemAt(selected_index);
    return item != nullptr && item->role == role;
}

NavigationModel BuildSettingsPageNavigationModel()
{
    NavigationModel model = {};
    model.scope = NavigationScope::kSettings;

    AddItem(model,
            NavigationItemSection::kSettingsPageMenu,
            NavigationItemRole::kSettingsWifiToggle,
            0);
    AddItem(model,
            NavigationItemSection::kSettingsPageMenu,
            NavigationItemRole::kSettingsEnableApToggle,
            1);
    AddItem(model,
            NavigationItemSection::kSettingsPageMenu,
            NavigationItemRole::kSettingsFormatSdButton,
            2);
    AddItem(model, NavigationItemSection::kFooter, NavigationItemRole::kFooterSettings, 1);
    AddItem(model, NavigationItemSection::kFooter, NavigationItemRole::kFooterWifi, 2);
    AddItem(model, NavigationItemSection::kFooter, NavigationItemRole::kFooterHome, 0);
    return model;
}

NavigationModel BuildWifiPageNavigationModel()
{
    NavigationModel model = {};
    model.scope = NavigationScope::kWifi;

    AddItem(model,
            NavigationItemSection::kWifiPageControls,
            NavigationItemRole::kWifiPageNetworkList,
            0);
    AddItem(model,
            NavigationItemSection::kWifiPageControls,
            NavigationItemRole::kWifiPagePasswordInput,
            1);
    AddItem(model,
            NavigationItemSection::kWifiPageControls,
            NavigationItemRole::kWifiPagePasswordVisibilityButton,
            2);
    AddItem(model,
            NavigationItemSection::kWifiPageControls,
            NavigationItemRole::kWifiPageScanButton,
            3);
    AddItem(model,
            NavigationItemSection::kWifiPageControls,
            NavigationItemRole::kWifiPageConnectButton,
            4);
    AddItem(model, NavigationItemSection::kFooter, NavigationItemRole::kFooterSettings, 1);
    AddItem(model, NavigationItemSection::kFooter, NavigationItemRole::kFooterWifi, 2);
    AddItem(model, NavigationItemSection::kFooter, NavigationItemRole::kFooterHome, 0);
    return model;
}

}  // namespace page_navigation
