import type {
  BottomSheetField,
  CardField,
  NetworkListField,
  NetworkStatusField,
  ValidatableField,
} from './types';

function getRequiredElement<T>(id: string): T {
  const element = document.getElementById(id);
  if (!element) {
    throw new Error(`Required portal element "#${id}" was not found.`);
  }

  return element as T;
}

export interface PortalDom {
  followupLogoEl: HTMLDivElement;
  // WiFi
  wifiStatusCard: NetworkStatusField;
  wifiSettingsSheet: BottomSheetField;
  wifiSettingsCloseBtn: HTMLElement;
  wifiSettingsNotification: HTMLDivElement;
  networkList: NetworkListField;
  passwordInput: ValidatableField;
  scanBtn: HTMLButtonElement;
  connectBtn: HTMLButtonElement;
  // Gemini API key
  geminiCard: CardField;
  geminiApiKeyInput: ValidatableField & { readOnly: boolean };
  geminiClearBtn: HTMLButtonElement;
  geminiSaveBtn: HTMLButtonElement;
  // Time / timezone
  timezoneLocationCard: CardField;
  timezoneSelect: ValidatableField;
  manualTimeInput: ValidatableField;
  manualDateInput: ValidatableField;
  timezoneLocationClearBtn: HTMLButtonElement;
  timezoneLocationSaveBtn: HTMLButtonElement;
}

export function createPortalDom(): PortalDom {
  return {
    followupLogoEl: getRequiredElement<HTMLDivElement>('followupLogo'),

    wifiStatusCard: getRequiredElement<NetworkStatusField>('wifiStatusCard'),
    wifiSettingsSheet: getRequiredElement<BottomSheetField>('wifiSettingsSheet'),
    wifiSettingsCloseBtn: getRequiredElement<HTMLElement>('wifiSettingsCloseBtn'),
    wifiSettingsNotification: getRequiredElement<HTMLDivElement>('wifiSettingsNotification'),
    networkList: getRequiredElement<NetworkListField>('networkList'),
    passwordInput: getRequiredElement<ValidatableField>('password'),
    scanBtn: getRequiredElement<HTMLButtonElement>('scanBtn'),
    connectBtn: getRequiredElement<HTMLButtonElement>('connectBtn'),

    geminiCard: getRequiredElement<CardField>('geminiCard'),
    geminiApiKeyInput: getRequiredElement<ValidatableField & { readOnly: boolean }>(
      'geminiApiKeyInput'
    ),
    geminiClearBtn: getRequiredElement<HTMLButtonElement>('geminiClearBtn'),
    geminiSaveBtn: getRequiredElement<HTMLButtonElement>('geminiSaveBtn'),

    timezoneLocationCard: getRequiredElement<CardField>('timezoneLocationCard'),
    timezoneSelect: getRequiredElement<ValidatableField>('timezoneSelect'),
    manualTimeInput: getRequiredElement<ValidatableField>('manualTimeInput'),
    manualDateInput: getRequiredElement<ValidatableField>('manualDateInput'),
    timezoneLocationClearBtn: getRequiredElement<HTMLButtonElement>('timezoneLocationClearBtn'),
    timezoneLocationSaveBtn: getRequiredElement<HTMLButtonElement>('timezoneLocationSaveBtn'),
  };
}
