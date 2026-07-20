export interface Network {
  ssid: string;
  rssi: number;
  signal_strength: string;
  encryption_type: number;
  is_open: boolean;
  security: string;
}

export interface PortalResponse {
  success: boolean;
  message?: string;
  ssid?: string;
  rssi?: number;
  connected?: boolean;
  scan_in_progress?: boolean;
  networks?: Network[];
}

export interface TimezoneListResponse {
  success: boolean;
  message?: string;
  timezones?: Array<{ name: string; description?: string }>;
}

export interface EffectsStatusResponse {
  success: boolean;
  message?: string;
  effect?: string;
  tint?: string;
}

export interface TimeSettingsValues {
  enabled?: boolean;
  timezone_name?: string;
  location?: string;
}

export interface TimeRuntimeStatus {
  clock_enabled?: boolean;
  time_valid?: boolean;
  time_source?: string;
  has_network_sync?: boolean;
  last_network_sync_epoch?: number;
  current_date?: string;
  current_time?: string;
}

export interface TimeSettingsResponse {
  success: boolean;
  message?: string;
  settings?: TimeSettingsValues;
  runtime?: TimeRuntimeStatus;
}

export interface AudioStatusResponse {
  success: boolean;
  message?: string;
  enabled?: boolean;
  volume?: number;
}

export interface DisplayStatusResponse {
  success: boolean;
  message?: string;
  brightness?: number;
}

export interface PowerRuntimeStatus {
  battery_connected?: boolean;
  battery_percentage?: number;
  usb_power_present?: boolean;
  charging?: boolean;
}

export type UpdateTransport = 'portal' | 'serial';
export type UpdateState =
  | 'IDLE'
  | 'UPLOADING'
  | 'PROCESSING'
  | 'SUCCESS'
  | 'ERROR'
  | 'ABORTED';
export type OtaUpdateType = 'firmware' | 'filesystem';

export interface UpdateRuntimeStatus {
  active?: boolean;
  transport?: UpdateTransport | null;
  type?: OtaUpdateType | null;
  state?: UpdateState;
  progress?: number;
  received?: number;
  total?: number;
  version?: string;
  running_partition?: string;
  completed?: boolean;
  can_abort?: boolean;
  restart_required?: boolean;
  error_message?: string;
}

export interface UpdateRuntimeResponse {
  success: boolean;
  message?: string;
  runtime?: UpdateRuntimeStatus;
}

export interface SleepStatusResponse {
  success: boolean;
  message?: string;
  light_sleep_trigger_minutes?: number;
}

export type ModuleId = 'core' | 'classic' | 'xiaozhi' | 'gemini' | 'openai';

export interface BootstrapResponse {
  success: boolean;
  message?: string;
  schema_version?: number;
  wifi?: PortalResponse;
  time?: TimeSettingsResponse;
  audio?: AudioStatusResponse;
  display?: DisplayStatusResponse;
  sleep?: SleepStatusResponse;
  effects?: EffectsStatusResponse;
  module?: ModuleStatusResponse;
}

export interface ModuleRoutes {
  settings?: string;
  reset?: string;
}

export interface GeminiModuleSettings {
  has_key?: boolean;
  last4?: string;
}

export interface OpenAiModuleSettings {
  has_key?: boolean;
  last4?: string;
  resumption_available?: boolean;
}

export interface XiaozhiModuleSettings {
  api_url?: string;
  using_default_api_url?: boolean;
  activation_completed?: boolean;
  has_activation_display?: boolean;
  activation_message?: string;
  activation_code?: string;
  has_cached_protocol_config?: boolean;
}

export interface TalkingClockModuleSettings {
  wakeup_minutes?: number;
  bedtime_minutes?: number;
  phrase_interval_minutes?: number;
  hourly_announcements_enabled?: boolean;
  random_phrases_enabled?: boolean;
  ble_enabled?: boolean;
  drink_water_reminder_enabled?: boolean;
  drink_water_interval_minutes?: number;
  stretch_reminder_enabled?: boolean;
  stretch_interval_minutes?: number;
  break_reminder_enabled?: boolean;
  break_interval_minutes?: number;
  timed_mute_active?: boolean;
  timed_mute_duration_minutes?: number;
  timed_mute_until_epoch?: number;
}

export interface ModuleStatusResponse {
  active?: boolean;
  id?: string;
  name?: string;
  routes?: ModuleRoutes;
  settings?: unknown;
}

export interface TalkingClockModuleResponse {
  success: boolean;
  message?: string;
  settings?: TalkingClockModuleSettings;
}

export interface GeminiModuleResponse {
  success: boolean;
  message?: string;
  settings?: GeminiModuleSettings;
}

export interface OpenAiModuleResponse {
  success: boolean;
  message?: string;
  settings?: OpenAiModuleSettings;
}

export interface XiaozhiModuleResponse {
  success: boolean;
  message?: string;
  settings?: XiaozhiModuleSettings;
}

export interface TimeConfigFormValues {
  timezoneName: string;
  manualDate: string;
  manualTime: string;
  wakeupMinutes?: number;
  bedtimeMinutes?: number;
}

export type StatusType = 'info' | 'success' | 'warning' | 'error' | 'danger';
export type ValidatableField = HTMLElement & {
  checkValidity?: () => boolean;
  disabled: boolean;
  focus: (options?: FocusOptions) => void;
  reportValidity?: () => boolean;
  setCustomValidity?: (message: string) => void;
  value: string;
};

export type RangeSliderField = HTMLElement & {
  disabled: boolean;
  focus: (options?: FocusOptions) => void;
  value: string;
};

export type DurationField = HTMLElement & {
  disabled: boolean;
  focus: (options?: FocusOptions) => void;
  hours: ValidatableField;
  minutes: ValidatableField;
};

export type DurationInputs = {
  hours: ValidatableField;
  minutes: ValidatableField;
};

export type FileUploadField = HTMLElement & {
  accept: string;
  disabled: boolean;
  files: FileList | null;
  focus: (options?: FocusOptions) => void;
  progressHidden: boolean;
  progressMax: number;
  progressMessageText: string;
  progressValue: number;
  warningText: string;
};

export type BottomSheetField = HTMLElement & {
  closeBottomSheet: () => void;
  openBottomSheet: () => void;
};

export type NetworkStatusField = HTMLElement & {
  actionDisabled: boolean;
  actionLabel: string;
  iconSvg: string;
  network: string;
  statusLabel: string;
};

export type NetworkListField = HTMLElement & {
  appendItem: (item: HTMLElement) => void;
  clearItems: () => void;
  getOptions: () => HTMLElement[];
  scrollOptionIntoView: (index: number) => void;
  setActiveDescendant: (value: string | null) => void;
  setEmptyState: (item: HTMLElement) => void;
  focus: (options?: FocusOptions) => void;
};

export type CardField = HTMLElement & {
  iconSvg: string;
  setNotification: (message: string, type?: StatusType) => void;
};
