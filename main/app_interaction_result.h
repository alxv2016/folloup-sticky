#ifndef APP_INTERACTION_RESULT_H_
#define APP_INTERACTION_RESULT_H_

namespace app_interaction {

struct InputResult {
    bool consumed = false;
    bool request_shutdown = false;
    bool select_modal_submitted = false;
    int select_modal_selected_index = -1;
};

}  // namespace app_interaction

#endif  // APP_INTERACTION_RESULT_H_
