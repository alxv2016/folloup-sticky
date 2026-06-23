#ifndef UI_REFRESH_DISPATCHER_H_
#define UI_REFRESH_DISPATCHER_H_

#include <cstdint>
#include <functional>

namespace ui_refresh_dispatcher {

void Init();
void Dispatch(std::function<void()> callback);
void DispatchLatest(uint32_t key, std::function<void()> callback);

}  // namespace ui_refresh_dispatcher

#endif  // UI_REFRESH_DISPATCHER_H_
