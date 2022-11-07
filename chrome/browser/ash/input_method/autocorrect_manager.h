// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_AUTOCORRECT_MANAGER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_AUTOCORRECT_MANAGER_H_

#include <string>

#include "base/time/time.h"
#include "chrome/browser/ash/input_method/assistive_window_controller.h"
#include "chrome/browser/ash/input_method/diacritics_insensitive_string_comparator.h"
#include "chrome/browser/ash/input_method/input_method_engine.h"
#include "chrome/browser/ash/input_method/suggestion_handler_interface.h"
#include "chrome/browser/ash/input_method/text_field_contextual_info_fetcher.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace input_method {

// Must match with IMEAutocorrectActions in enums.xml
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AutocorrectActions {
  kWindowShown = 0,
  kUnderlined = 1,
  kReverted = 2,
  kUserAcceptedAutocorrect = 3,
  kUserActionClearedUnderline = 4,
  kUserExitedTextFieldWithUnderline = 5,
  kMaxValue = kUserExitedTextFieldWithUnderline,
};

// Must match with IMEAutocorrectInternalStates in enums.xml
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AutocorrectInternalStates {
  // Autocorrect handles an empty range.
  kHandleEmptyRange = 0,
  // Autocorrect handles a new suggestion while the previous one is still
  // pending.
  kHandleUnclearedRange = 1,
  // Autocorrect handles a new suggestion while input context is not available.
  kHandleNoInputContext = 2,
  // Autocorrect is called with a range, text, and suggestion that do not
  // match.
  kHandleInvalidArgs = 3,
  // Autocorrect handler sets a range to TextInputClient.
  kHandleSetRange = 4,
  // Autocorrect suggestion is underlined.
  kUnderlineShown = 5,
  // Autocorrect suggestion is resolved by user interactions and not
  // error, exit field or undone.
  kSuggestionResolved = 6,
  // Autocorrect suggestion is accepted by user interaction.
  kSuggestionAccepted = 7,
  // Autocorrect is cleared because Input context is lost while having a
  // pending autocorrect.
  kNoInputContext = 8,
  // Autocorrect cannot set a range because TextInputClient does not support
  // setting a range.
  kErrorSetRange = 9,
  // Autocorrect fails to validate a suggestion because of potentially async
  // problems prevent it from finding the suggested text within the autocorrect
  // range in surrounding text.
  kErrorRangeNotValidated = 10,
  // Autocorrect got an error when trying to show undo window.
  kErrorShowUndoWindow = 11,
  // Autocorrect got an error when trying to hide undo window.
  kErrorHideUndoWindow = 12,
  // Autocorrect shows an undo window.
  kShowUndoWindow = 13,
  // Autocorrect hides an undo window.
  kHideUndoWindow = 14,
  // Autocorrect highlights undo button of undo window.
  kHighlightUndoWindow = 15,
  // OnFocus event was called.
  kOnFocusEvent = 16,
  // OnFocus event was called with pending suggestion.
  kOnFocusEventWithPendingSuggestion = 17,
  // OnBlur event was called.
  kOnBlurEvent = 18,
  // OnBlue event was called with pending suggestion.
  kOnBlurEventWithPendingSuggestion = 19,
  // User did some typing and had at least one suggestion.
  kTextFieldEditsWithAtLeastOneSuggestion = 20,
  // Autocorrect could be triggered if the last word typed had an error.
  kCouldTriggerAutocorrect = 21,
  // The focused text field is in a denylisted domain.
  kAppIsInDenylist = 22,
  // The focused text field is in a denylisted domain but autocorrect is still
  // executed.
  kHandleSuggestionInDenylistedApp = 23,
  kMaxValue = kHandleSuggestionInDenylistedApp,
};

// Implements functionality for chrome.input.ime.autocorrect() extension API.
// This function shows UI to indicate that autocorrect has happened and allows
// it to be undone easily.
class AutocorrectManager {
 public:
  // `suggestion_handler_` must be alive for the duration of the lifetime of
  // this instance.
  explicit AutocorrectManager(SuggestionHandlerInterface* suggestion_handler);
  ~AutocorrectManager();

  AutocorrectManager(const AutocorrectManager&) = delete;
  AutocorrectManager& operator=(const AutocorrectManager&) = delete;

  // Mark `autocorrect_range` with an underline. `autocorrect_range` is based on
  // the `current_text` contents.
  // NOTE: Technically redundant to require client to supply `current_text` as
  // AutocorrectManager can retrieve it from current text editing state known to
  // IMF. However, due to async situation between browser-process IMF and
  // render-process TextInputClient, it may just get a stale value that way.
  // TODO(crbug/1194424): Remove technically redundant `current_text` param
  // to avoid situation with multiple conflicting sources of truth.
  void HandleAutocorrect(gfx::Range autocorrect_range,
                         const std::u16string& original_text,
                         const std::u16string& current_text);

  // Handles interactions with Undo UI.
  bool OnKeyEvent(const ui::KeyEvent& event);

  // Indicates a new text field is focused, used to save context ID.
  void OnFocus(int context_id);

  // Handles OnBlur event and processes any pending autocorrect range.
  void OnBlur();

  // Processes the state where a user leaves or focuses a text field. At this
  // stage any pending autocorrect range is cleared and relevant metrics are
  // recorded.
  void ProcessTextFieldChange();

  // To show the undo window when cursor is in an autocorrected word, this class
  // is notified of surrounding text changes.
  void OnSurroundingTextChanged(const std::u16string& text,
                                int cursor_pos,
                                int anchor_pos);

  void UndoAutocorrect();

  // Whether auto correction is disabled by some rule.
  bool DisabledByRule();

 private:
  void LogAssistiveAutocorrectAction(AutocorrectActions action);

  void OnTextFieldContextualInfoChanged(const TextFieldContextualInfo& info);

  // Forces to accept or clear a pending autocorrect suggestion if any. If the
  // autocorrect range is empty, it means the user interacted with the
  // pending autocorrect suggestion and made it invalid, so it considers
  // the autocorrect suggestion as "cleared". Otherwise, it considers the
  // autocorrect suggestion as "accepted". For the both cases, relevant
  // metrics are recorded, state variables are reset and autocorrect range is
  // set to empty.
  void AcceptOrClearPendingAutocorrect();

  // Hides undo window if there is any visible.
  void HideUndoWindow();

  // Shows undo window and record the relevant metric if undo window is
  // not already visible.
  void ShowUndoWindow(gfx::Range range, const std::u16string& text);

  // Highlights undo button of undo window if it is visible.
  void HighlightUndoButton();

  // Processes the result of a set autocorrect range call. An unsuccessful
  // result could mean that autocorrect was not supported by the text input
  // client, so the autocorrect suggestion can be ignored. Otherwise, the
  // autocorrect suggestion will be set as pending and its relevant
  // interactions and metrics will be managed here.
  void ProcessSetAutocorrectRangeDone(const gfx::Range& autocorrect_range,
                                      const std::u16string& original_text,
                                      const std::u16string& current_text,
                                      bool set_range_success);

  struct PendingAutocorrectState {
    explicit PendingAutocorrectState(const std::u16string& original_text,
                                     const std::u16string& suggested_text,
                                     const base::TimeTicks& start_time,
                                     bool virtual_keyboard_visible = false);
    PendingAutocorrectState(const PendingAutocorrectState& other);
    ~PendingAutocorrectState();

    // Original text that is now corrected by autocorrect.
    std::u16string original_text;

    // Autocorrect suggestion that replaced original text.
    std::u16string suggested_text;

    // Specifies if the suggestion is validated in the surrounding text.
    bool is_validated = false;

    // Number of times that validation of autocorrect suggestion in the
    // surrounding text failed.
    int validation_tries = 0;

    // Number of characters inserted anytime after setting the pending
    // autocorrect range. Negative means no autocorrect range is pending or a
    // range has just been set to pending with no OnSurroundingTextChanged
    // called yet.
    int num_inserted_chars = -1;

    // Last known text length from OnSurroundingTextChanged after setting
    // the pending autocorrect range. Negative means no autocorrect range is
    // pending or a range has just been set to pending with no
    // OnSurroundingTextChanged called yet.
    int text_length = -1;

    // Specifies if undo window is visible or not.
    bool undo_window_visible = false;

    // Specifies if undo button is highlighted or not.
    bool undo_button_highlighted = false;

    // Specifies if window_shown metric is already incremented for the pending
    // autocorrect or not.
    bool window_shown_logged = false;

    // The time of setting the pending range.
    base::TimeTicks start_time;

    // Specifies if virtual keyboard was visible when suggesting the pending
    // autocorrect or not.
    bool virtual_keyboard_visible = false;
  };

  // State variable for pending autocorrect, nullopt means no autocorrect
  // suggestion is pending. The state is kept to avoid issue where
  // InputContext returns stale autocorrect range.
  absl::optional<PendingAutocorrectState> pending_autocorrect_;

  // Specifies if the last try for hiding undo window failed. This means
  // undo window is possibly visible while it must not be.
  bool error_on_hiding_undo_window_ = false;

  // The number of autocorrect suggestions that have been handled since
  // focusing on the text field.
  int num_handled_autocorrect_in_text_field_ = 0;

  SuggestionHandlerInterface* suggestion_handler_;
  int context_id_ = 0;

  DiacriticsInsensitiveStringComparator
      diacritics_insensitive_string_comparator_;
  bool in_diacritical_autocorrect_session_ = false;

  bool disabled_by_rule_ = false;

  base::WeakPtrFactory<AutocorrectManager> weak_ptr_factory_{this};
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_AUTOCORRECT_MANAGER_H_:w
