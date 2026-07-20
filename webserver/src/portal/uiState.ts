import type { PortalDom } from './dom';

interface PortalUiStateControllers {
  geminiController: {
    getGeminiHasKey: () => boolean;
    isGeminiBusy: () => boolean;
  };
  timeController: {
    isClockBusy: () => boolean;
    isLocationBusy: () => boolean;
    isTimezoneBusy: () => boolean;
  };
  wifiController: {
    getSelectedNetwork: () => string;
    isCheckingStatus: () => boolean;
    isConnecting: () => boolean;
    isCurrentlyConnected: () => boolean;
    isScanning: () => boolean;
    isSelectedConnected: () => boolean;
    requiresPassword: () => boolean;
  };
}

interface UpdatePortalUiStateDeps {
  controllers: PortalUiStateControllers;
  dom: PortalDom;
}

export function updatePortalUiState(deps: UpdatePortalUiStateDeps) {
  const { controllers, dom } = deps;

  // --- WiFi ---
  const wifiBusy =
    controllers.wifiController.isScanning() ||
    controllers.wifiController.isConnecting() ||
    controllers.wifiController.isCheckingStatus();
  const isSelectedConnected = controllers.wifiController.isSelectedConnected();
  const selectedNetwork = controllers.wifiController.getSelectedNetwork().trim();
  const passwordRequired =
    selectedNetwork.length > 0 &&
    !isSelectedConnected &&
    controllers.wifiController.requiresPassword();
  const hasWifiActionTarget =
    isSelectedConnected ||
    (selectedNetwork.length > 0 &&
      (!passwordRequired || dom.passwordInput.value.trim().length > 0));

  dom.scanBtn.disabled = wifiBusy;
  dom.connectBtn.disabled = wifiBusy || !hasWifiActionTarget;
  dom.wifiStatusCard.actionDisabled = wifiBusy;
  dom.passwordInput.disabled = controllers.wifiController.isConnecting();
  if (controllers.wifiController.isConnecting()) {
    dom.connectBtn.textContent = 'Connecting...';
    dom.wifiStatusCard.actionLabel = 'Connecting...';
  } else if (isSelectedConnected) {
    dom.connectBtn.textContent = 'Disconnect';
    dom.wifiStatusCard.actionLabel = 'Disconnect';
  } else {
    dom.connectBtn.textContent = 'Connect';
    dom.wifiStatusCard.actionLabel = controllers.wifiController.isCurrentlyConnected()
      ? 'Disconnect'
      : 'Connect';
  }

  // --- Time / timezone ---
  const timeConfigBusy =
    controllers.timeController.isTimezoneBusy() ||
    controllers.timeController.isLocationBusy() ||
    controllers.timeController.isClockBusy();
  dom.timezoneSelect.disabled = timeConfigBusy;
  dom.manualDateInput.disabled = timeConfigBusy;
  dom.manualTimeInput.disabled = timeConfigBusy;
  dom.timezoneLocationSaveBtn.disabled =
    timeConfigBusy || dom.timezoneSelect.value.trim().length === 0;
  dom.timezoneLocationClearBtn.disabled =
    timeConfigBusy || dom.timezoneSelect.value.trim().length === 0;

  // --- Gemini API key (always available on Followup) ---
  const geminiBusy = controllers.geminiController.isGeminiBusy();
  const geminiHasKey = controllers.geminiController.getGeminiHasKey();
  dom.geminiApiKeyInput.readOnly = geminiHasKey || geminiBusy;
  dom.geminiApiKeyInput.disabled = geminiBusy;
  dom.geminiSaveBtn.disabled =
    geminiBusy || geminiHasKey || dom.geminiApiKeyInput.value.trim().length === 0;
  dom.geminiSaveBtn.hidden = geminiHasKey;
  dom.geminiClearBtn.disabled = geminiBusy || !geminiHasKey;
  dom.geminiClearBtn.hidden = !geminiHasKey;
}
