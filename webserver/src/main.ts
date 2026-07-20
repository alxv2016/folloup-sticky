import './portal.css';
import { createGradualBlur } from './gradualBlur';
import {
  defineBottomSheet,
  defineButton,
  defineCard,
  defineIconButton,
  defineInput,
  defineNetworkList,
  defineNetworkStatus,
  defineSelect,
  defineToast,
} from './components';

import followupLogo from './assets/followup_logo.svg?raw';
import wifiIcon from './assets/wifi.svg?raw';
import wifiSecureIcon from './assets/wifi_secure.svg?raw';
import wifi1BarIcon from './assets/wifi_1bar.svg?raw';
import wifi2BarIcon from './assets/wifi_2bar.svg?raw';
import wifi3BarIcon from './assets/wifi_3bar.svg?raw';
import wifi4BarIcon from './assets/wifi_4bar.svg?raw';
import wifiFindIcon from './assets/wifi_find.svg?raw';
import apiKeyIcon from './assets/api_key.svg?raw';
import checkIcon from './assets/check.svg?raw';
import clockIcon from './assets/clock.svg?raw';
import {
  fetchGeminiModuleJson,
  fetchPortalJson,
  fetchTimeRuntimeJson,
  fetchTimeSettingsJson,
  fetchTimezoneListJson,
} from './portal/api';
import {
  CLOCK_SYNC_POLL_ATTEMPTS,
  CLOCK_SYNC_POLL_INTERVAL_MS,
  NETWORK_SCAN_POLL_ATTEMPTS,
  NETWORK_SCAN_POLL_INTERVAL_MS,
  STATUS_POLL_ATTEMPTS,
  STATUS_POLL_INTERVAL_MS,
  TIME_RUNTIME_POLL_INTERVAL_MS,
  TIME_RUNTIME_API,
  TIME_SETTINGS_API,
} from './portal/constants';
import { createPortalDom } from './portal/dom';
import {
  buildTimezoneLabelMap,
  createCardNotifier,
  createLiveRegionNotifier,
  clearFieldError,
  delayMs,
  formatTimezoneLabel,
  isSwitchChecked,
  setFieldError,
  setIcon,
  setSwitchChecked,
  svgWithClass,
} from './portal/uiHelpers';
import { parseClockTimeInputValue } from './portal/duration';
import { createProviderKeysController } from './portal/providerKeys';
import { createTimeController } from './portal/time';
import { createWiFiController } from './portal/wifi';
import { bindPortalEvents } from './portal/events';
import { updatePortalUiState } from './portal/uiState';
import type {
  OpenAiModuleResponse,
  TalkingClockModuleResponse,
  ValidatableField,
} from './portal/types';

defineBottomSheet();
defineButton();
defineCard();
defineIconButton();
defineInput();
defineNetworkList();
defineNetworkStatus();
defineSelect();
defineToast();

let pageFade: ReturnType<typeof createGradualBlur> | null = null;

const dom = createPortalDom();

const setNotification = createLiveRegionNotifier(dom.wifiSettingsNotification);
const setGeminiNotification = createCardNotifier(dom.geminiCard);
const setTimezoneLocationNotification = createCardNotifier(dom.timezoneLocationCard);

// Detached stand-ins for the byte90-only "talking clock" time controls, which Followup does not
// use. They let the shared time controller keep its (now inert) clock-mode/wakeup/bedtime paths
// without those elements existing in the DOM.
const noopTimeInput = () => document.createElement('input') as unknown as ValidatableField;
const stubClockModeToggle = document.createElement('button');
const stubWakeupTimeInput = noopTimeInput();
const stubBedtimeTimeInput = noopTimeInput();
const stubOpenAiApiKeyInput = document.createElement('input') as unknown as ValidatableField & {
  readOnly: boolean;
};

function updateUi() {
  updatePortalUiState({
    controllers: {
      geminiController,
      timeController,
      wifiController,
    },
    dom,
  });
}

const geminiController = createProviderKeysController({
  fetchGeminiModuleJson,
  fetchOpenAiModuleJson: () => Promise.resolve({} as OpenAiModuleResponse),
  geminiApiKeyInput: dom.geminiApiKeyInput,
  isGeminiModuleActive: () => true,
  isOpenAiModuleActive: () => false,
  notifyGemini: setGeminiNotification,
  notifyOpenAi: () => {},
  openAiApiKeyInput: stubOpenAiApiKeyInput,
  updateButtons: updateUi,
});

const timeController = createTimeController({
  applyTalkingClockModuleSettings: () => {},
  bedtimeTimeInput: stubBedtimeTimeInput,
  clearFieldError,
  clockModeToggle: stubClockModeToggle,
  clockSyncPollAttempts: CLOCK_SYNC_POLL_ATTEMPTS,
  clockSyncPollIntervalMs: CLOCK_SYNC_POLL_INTERVAL_MS,
  fetchTimeRuntimeJson,
  fetchTimeSettingsJson,
  fetchTimezoneListJson,
  focusTalkingClockTimeInput: () => {},
  buildTimezoneLabelMap,
  formatTimezoneLabel,
  isSwitchChecked,
  isTalkingClockModuleActive: () => false,
  isTalkingClockModuleBusy: () => false,
  manualDateInput: dom.manualDateInput,
  manualTimeInput: dom.manualTimeInput,
  notifyClockMode: () => {},
  notify: setTimezoneLocationNotification,
  onStateChange: updateUi,
  onTalkingClockBusyChange: () => {},
  parseClockTimeInputValue,
  patchTalkingClockSettings: () =>
    Promise.resolve({ success: true } as TalkingClockModuleResponse),
  setFieldError,
  setSwitchChecked,
  timeRuntimeApi: TIME_RUNTIME_API,
  timeSettingsApi: TIME_SETTINGS_API,
  timezoneSelect: dom.timezoneSelect,
  wakeupTimeInput: stubWakeupTimeInput,
});

const wifiController = createWiFiController({
  checkIcon,
  clearFieldError,
  delayMs,
  fetchPortalJson,
  networkList: dom.networkList,
  notify: setNotification,
  onConnectionStateChange: ({ wasConnected, isCurrentlyConnected }) => {
    if (!wasConnected && isCurrentlyConnected) {
      void timeController.refreshClockStatusAfterWifiConnect();
    }
  },
  onStateChange: updateUi,
  passwordInput: dom.passwordInput,
  scanPollAttempts: NETWORK_SCAN_POLL_ATTEMPTS,
  scanPollIntervalMs: NETWORK_SCAN_POLL_INTERVAL_MS,
  securityIcon: wifiSecureIcon,
  setFieldError,
  signalIcons: {
    oneBar: wifi1BarIcon,
    twoBar: wifi2BarIcon,
    threeBar: wifi3BarIcon,
    fourBar: wifi4BarIcon,
  },
  statusPollAttempts: STATUS_POLL_ATTEMPTS,
  statusPollIntervalMs: STATUS_POLL_INTERVAL_MS,
  svgWithClass,
  wifiFindIcon,
  wifiStatusCard: dom.wifiStatusCard,
});

function updatePageFade() {
  if (!pageFade) {
    return;
  }

  const scrollRoot = document.documentElement;
  const canScroll = scrollRoot.scrollHeight > window.innerHeight + 1;
  const isAtBottom = window.scrollY + window.innerHeight >= scrollRoot.scrollHeight - 1;

  pageFade.setVisible(canScroll && !isAtBottom);
}

function initialize() {
  setIcon(dom.followupLogoEl, followupLogo, 'followup-logo');
  dom.wifiStatusCard.iconSvg = wifiIcon;
  dom.geminiCard.iconSvg = apiKeyIcon;
  dom.timezoneLocationCard.iconSvg = clockIcon;

  wifiController.renderNetworkList();

  if (document.body) {
    pageFade = createGradualBlur({
      mount: document.body,
      mode: 'fixed',
      position: 'bottom',
      height: '6rem',
      width: '100vw',
      strength: 1.8,
      divCount: 5,
      curve: 'bezier',
      exponential: true,
      opacity: 1,
      zIndex: 20,
      className: 'page-gradual-blur',
      fallbackColor: 'var(--color-bg-selected)',
    });
    updatePageFade();
    window.addEventListener('scroll', updatePageFade, { passive: true });
    window.addEventListener('resize', updatePageFade);
    if (typeof ResizeObserver !== 'undefined') {
      const resizeObserver = new ResizeObserver(() => {
        updatePageFade();
      });
      resizeObserver.observe(document.body);
    }
  }

  updateUi();
  wifiController.updateWifiStatusCard();
  setNotification('');

  bindPortalEvents({
    controllers: {
      geminiController,
      timeController,
      wifiController,
    },
    dom,
    helpers: {
      clearFieldError,
      setFieldError,
      setTimezoneLocationNotification,
      updateUi,
    },
  });

  void loadInitialStatus();

  window.setInterval(() => {
    void timeController.fetchTimeRuntimeStatus();
  }, TIME_RUNTIME_POLL_INTERVAL_MS);
}

async function loadInitialStatus() {
  await Promise.allSettled([
    timeController.populateTimezoneOptions(),
    timeController.fetchTimeSettingsStatus(),
    wifiController.checkStatus(),
    loadGeminiSettings(),
  ]);
}

async function loadGeminiSettings() {
  try {
    const data = await fetchGeminiModuleJson('/api/settings/gemini');
    geminiController.applyGeminiSettings(data.settings);
  } catch (error) {
    console.error('Gemini settings status failed:', error);
  } finally {
    updateUi();
  }
}

initialize();
