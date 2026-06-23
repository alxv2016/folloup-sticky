#ifndef INPUT_CALLBACK_DISPATCHER_H_
#define INPUT_CALLBACK_DISPATCHER_H_

#include <cstdint>
#include <functional>

class InputCallbackDispatcher {
public:
    static InputCallbackDispatcher& GetInstance();

    void Initialize();
    void Dispatch(std::function<void()> callback);
    void DispatchLatest(uint32_t key, std::function<void()> callback);

private:
    InputCallbackDispatcher() = default;
};

#endif  // INPUT_CALLBACK_DISPATCHER_H_
