export const API_HEADERS = {
  'Content-Type': 'application/json',
  Accept: 'application/json',
};

export const STATUS_POLL_INTERVAL_MS = 2000;
export const STATUS_POLL_ATTEMPTS = 8;
export const CLOCK_SYNC_POLL_INTERVAL_MS = 2000;
export const CLOCK_SYNC_POLL_ATTEMPTS = 8;
export const TIME_RUNTIME_POLL_INTERVAL_MS = 3000;
export const POWER_RUNTIME_POLL_INTERVAL_MS = 60000;
export const UPDATE_RUNTIME_POLL_INTERVAL_MS = 1000;
export const NETWORK_SCAN_POLL_INTERVAL_MS = 750;
export const NETWORK_SCAN_POLL_ATTEMPTS = 20;
export const XIAOZHI_DEFAULT_STATUS = 'Connect to WiFi to get Activation Code';

export const TALKING_CLOCK_DEFAULTS = {
  phraseIntervalMinutes: 30,
  timedMuteDurationMinutes: 45,
  drinkWaterIntervalMinutes: 45,
  stretchIntervalMinutes: 60,
  breakIntervalMinutes: 30,
};

export const SETTINGS_API_ROOT = '/api/settings';
export const TIME_SETTINGS_API = `${SETTINGS_API_ROOT}/time`;
export const TIME_RUNTIME_API = '/api/runtime/time';
export const POWER_RUNTIME_API = '/api/runtime/power';
export const UPDATE_RUNTIME_API = '/api/runtime/update';
export const UPDATE_UPLOAD_API = '/api/update';
export const UPDATE_ABORT_API = '/api/update/abort';
export const AUDIO_SETTINGS_API = `${SETTINGS_API_ROOT}/audio`;
export const DISPLAY_SETTINGS_API = `${SETTINGS_API_ROOT}/display`;
export const DEVICE_SLEEP_SETTINGS_API = `${SETTINGS_API_ROOT}/device-sleep`;
export const EFFECTS_SETTINGS_API = `${SETTINGS_API_ROOT}/effects`;
