#ifndef APP_INTERACTION_RESULT_H_
#define APP_INTERACTION_RESULT_H_

#include <cstdint>

namespace app_interaction {

enum class FeedbackCue : uint8_t {
    kNone = 0,
    kClick,
    kTouchContact,
    kModalOpen,
    kError,
    kRecordingStart,
};

struct InputResult {
    bool consumed = false;
    bool play_feedback = false;
    FeedbackCue feedback_cue = FeedbackCue::kNone;
    bool request_shutdown = false;
    bool request_format_sd_card = false;
    bool select_modal_submitted = false;
    int select_modal_selected_index = -1;
};

}  // namespace app_interaction

#endif  // APP_INTERACTION_RESULT_H_
