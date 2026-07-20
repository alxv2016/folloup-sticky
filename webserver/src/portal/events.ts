import type { PortalDom } from './dom';
import type { StatusType, ValidatableField } from './types';

interface PortalEventControllers {
  geminiController: {
    clearGeminiKey: () => Promise<void>;
    saveGeminiKey: () => Promise<void>;
  };
  timeController: {
    clearTimezoneLocation: () => Promise<void>;
    saveTimezoneLocation: () => Promise<void>;
  };
  wifiController: {
    disconnect: () => Promise<void>;
    handleConnectAction: () => void;
    handleListboxKeyDown: (event: KeyboardEvent) => void;
    handlePasswordInput: () => void;
    isCurrentlyConnected: () => boolean;
    scanNetworks: () => Promise<void>;
  };
}

interface PortalEventHelpers {
  clearFieldError: (field: ValidatableField) => void;
  setFieldError: (field: ValidatableField, message: string) => void;
  setTimezoneLocationNotification: (message: string, type?: StatusType) => void;
  updateUi: () => void;
}

interface BindPortalEventsDeps {
  controllers: PortalEventControllers;
  dom: PortalDom;
  helpers: PortalEventHelpers;
}

function runWithButtonFocus(button: HTMLButtonElement, action: () => Promise<void>): void {
  action().finally(() => {
    if (!button.disabled && button.offsetParent !== null) {
      setTimeout(() => {
        if (!button.disabled && button.offsetParent !== null) {
          button.focus();
        }
      }, 0);
    }
  });
}

export function bindPortalEvents(deps: BindPortalEventsDeps) {
  const { controllers, dom, helpers } = deps;

  function openWifiSettingsSheet() {
    dom.wifiSettingsSheet.openBottomSheet();
    void controllers.wifiController.scanNetworks();
  }

  // --- WiFi ---
  dom.scanBtn.addEventListener('click', () => {
    void controllers.wifiController.scanNetworks();
  });
  dom.connectBtn.addEventListener('click', () => {
    controllers.wifiController.handleConnectAction();
  });
  dom.wifiStatusCard.addEventListener('action', () => {
    if (controllers.wifiController.isCurrentlyConnected()) {
      void controllers.wifiController.disconnect();
      return;
    }
    openWifiSettingsSheet();
  });
  dom.wifiStatusCard.addEventListener('settings', () => {
    openWifiSettingsSheet();
  });
  dom.wifiSettingsCloseBtn.addEventListener('click', () => {
    dom.wifiSettingsSheet.closeBottomSheet();
  });
  dom.passwordInput.addEventListener('input', () => {
    controllers.wifiController.handlePasswordInput();
  });
  dom.networkList.addEventListener('keydown', event => {
    controllers.wifiController.handleListboxKeyDown(event);
  });

  // --- Gemini API key ---
  dom.geminiSaveBtn.addEventListener('click', () => {
    runWithButtonFocus(dom.geminiSaveBtn, () => controllers.geminiController.saveGeminiKey());
  });
  dom.geminiClearBtn.addEventListener('click', () => {
    runWithButtonFocus(dom.geminiClearBtn, () => controllers.geminiController.clearGeminiKey());
  });
  dom.geminiApiKeyInput.addEventListener('input', helpers.updateUi);

  // --- Time / timezone ---
  dom.timezoneLocationSaveBtn.addEventListener('click', () => {
    if (!dom.timezoneSelect.value.trim()) {
      helpers.setFieldError(dom.timezoneSelect, 'Select a timezone.');
      helpers.setTimezoneLocationNotification('Select a timezone.', 'error');
      dom.timezoneSelect.focus({ preventScroll: true });
      return;
    }

    helpers.clearFieldError(dom.timezoneSelect);
    runWithButtonFocus(dom.timezoneLocationSaveBtn, () =>
      controllers.timeController.saveTimezoneLocation()
    );
  });
  dom.timezoneLocationClearBtn.addEventListener('click', () => {
    runWithButtonFocus(dom.timezoneLocationClearBtn, () =>
      controllers.timeController.clearTimezoneLocation()
    );
  });
  dom.timezoneSelect.addEventListener('change', () => {
    if (dom.timezoneSelect.value?.trim().length) {
      helpers.clearFieldError(dom.timezoneSelect);
    }
    helpers.updateUi();
  });
  dom.manualDateInput.addEventListener('input', helpers.updateUi);
  dom.manualTimeInput.addEventListener('input', helpers.updateUi);
}
