import type {
  StatusType,
  TalkingClockModuleResponse,
  TalkingClockModuleSettings,
  TimeConfigFormValues,
  TimeRuntimeStatus,
  TimeSettingsResponse,
  TimezoneListResponse,
  ValidatableField,
} from './types';

interface TimeControllerDeps {
  applyTalkingClockModuleSettings: (
    settings?: TalkingClockModuleSettings
  ) => void;
  bedtimeTimeInput: ValidatableField;
  clearFieldError: (field: ValidatableField) => void;
  clockModeToggle: HTMLButtonElement;
  clockSyncPollAttempts: number;
  clockSyncPollIntervalMs: number;
  fetchTimeRuntimeJson: (
    path: string,
    init?: RequestInit
  ) => Promise<{ success: boolean; message?: string; runtime?: TimeRuntimeStatus }>;
  fetchTimeSettingsJson: (
    path: string,
    init?: RequestInit
  ) => Promise<TimeSettingsResponse>;
  fetchTimezoneListJson: (
    path: string,
    init?: RequestInit
  ) => Promise<TimezoneListResponse>;
  focusTalkingClockTimeInput: () => void;
  buildTimezoneLabelMap: (
    timezones: Array<{ name: string; description?: string }>
  ) => Map<string, string>;
  formatTimezoneLabel: (
    item: { name: string; description?: string },
    labelMap: Map<string, string>
  ) => string;
  isSwitchChecked: (toggle: HTMLButtonElement) => boolean;
  isTalkingClockModuleActive: () => boolean;
  isTalkingClockModuleBusy: () => boolean;
  manualDateInput: ValidatableField;
  manualTimeInput: ValidatableField;
  notifyClockMode: (message: string, type?: StatusType) => void;
  notify: (message: string, type?: StatusType) => void;
  onStateChange: () => void;
  onTalkingClockBusyChange: (busy: boolean) => void;
  parseClockTimeInputValue: (input: { value: string }) => number | null;
  patchTalkingClockSettings: (
    patch: Partial<TalkingClockModuleSettings>
  ) => Promise<TalkingClockModuleResponse>;
  setFieldError: (field: ValidatableField, message: string) => void;
  setSwitchChecked: (toggle: HTMLButtonElement, checked: boolean) => void;
  timeRuntimeApi: string;
  timeSettingsApi: string;
  timezoneSelect: ValidatableField;
  wakeupTimeInput: ValidatableField;
}

export function createTimeController(deps: TimeControllerDeps) {
  let isTimezoneBusy = false;
  let isLocationBusy = false;
  let isClockBusy = false;
  let isClockSyncRefreshPending = false;

  async function fetchTimezoneList(): Promise<TimezoneListResponse> {
    return deps.fetchTimezoneListJson('/api/timezone/list');
  }

  function applyTimeSettingsStatus(data: TimeSettingsResponse) {
    const settings = data.settings;
    const runtime = data.runtime;

    deps.setSwitchChecked(deps.clockModeToggle, Boolean(settings?.enabled));
    deps.timezoneSelect.value = settings?.timezone_name || '';
    deps.clearFieldError(deps.timezoneSelect);
    deps.manualDateInput.value =
      typeof runtime?.current_date === 'string' ? runtime.current_date : '';
    deps.manualTimeInput.value =
      typeof runtime?.current_time === 'string' ? runtime.current_time : '';
  }

  function applyTimeRuntimeStatus(runtime?: TimeRuntimeStatus) {
    if (!runtime) {
      return;
    }

    if (typeof runtime.clock_enabled === 'boolean') {
      deps.setSwitchChecked(deps.clockModeToggle, runtime.clock_enabled);
    }

    deps.manualDateInput.value =
      typeof runtime.current_date === 'string' ? runtime.current_date : '';
    deps.manualTimeInput.value =
      typeof runtime.current_time === 'string' ? runtime.current_time : '';
  }

  async function populateTimezoneOptions() {
    deps.timezoneSelect.innerHTML = '';
    const placeholder = document.createElement('option');
    placeholder.value = '';
    placeholder.textContent = 'Select timezone';
    deps.timezoneSelect.appendChild(placeholder);

    try {
      const data = await fetchTimezoneList();
      const timezones = Array.isArray(data.timezones) ? data.timezones : [];
      const labelMap = deps.buildTimezoneLabelMap(timezones);
      timezones
        .map(item => ({
          value: item.name,
          label: deps.formatTimezoneLabel(item, labelMap),
        }))
        .sort((a, b) => a.label.localeCompare(b.label))
        .forEach(option => {
          const element = document.createElement('option');
          element.value = option.value;
          element.textContent = option.label;
          deps.timezoneSelect.appendChild(element);
        });
    } catch (error) {
      console.error('Timezone list fetch failed:', error);
      deps.notify(
        error instanceof Error ? error.message : 'Failed to load timezones.',
        'error'
      );
    }
  }

  async function fetchTimeSettingsStatus() {
    if (isTimezoneBusy || isLocationBusy || isClockBusy) {
      return;
    }

    isTimezoneBusy = true;
    isLocationBusy = true;
    isClockBusy = true;
    deps.onStateChange();

    try {
      const data = await deps.fetchTimeSettingsJson(deps.timeSettingsApi);
      applyTimeSettingsStatus(data);
    } catch (error) {
      console.error('Time settings status failed:', error);
      deps.notify(
        error instanceof Error ? error.message : 'Time settings status failed.',
        'error'
      );
    } finally {
      isTimezoneBusy = false;
      isLocationBusy = false;
      isClockBusy = false;
      deps.onStateChange();
    }
  }

  async function refreshClockStatusAfterWifiConnect() {
    if (isClockBusy || isClockSyncRefreshPending) {
      return;
    }

    isClockSyncRefreshPending = true;

    try {
      for (let attempt = 0; attempt < deps.clockSyncPollAttempts; attempt++) {
        const data = await deps.fetchTimeRuntimeJson(deps.timeRuntimeApi);
        applyTimeRuntimeStatus(data.runtime);

        if (
          data.runtime?.time_valid === true &&
          typeof data.runtime.current_time === 'string' &&
          data.runtime.current_time.length > 0
        ) {
          return;
        }

        if (attempt < deps.clockSyncPollAttempts - 1) {
          await new Promise<void>(resolve => {
            setTimeout(resolve, deps.clockSyncPollIntervalMs);
          });
        }
      }
    } catch (error) {
      console.error('Clock refresh after WiFi connect failed:', error);
    } finally {
      isClockSyncRefreshPending = false;
    }
  }

  async function fetchTimeRuntimeStatus() {
    if (document.hidden || isTimezoneBusy || isLocationBusy || isClockBusy) {
      return;
    }

    if (!deps.isSwitchChecked(deps.clockModeToggle)) {
      return;
    }

    try {
      const data = await deps.fetchTimeRuntimeJson(deps.timeRuntimeApi);
      applyTimeRuntimeStatus(data.runtime);
    } catch (error) {
      console.error('Time runtime status failed:', error);
    }
  }

  function getTimeConfigFormValues(): TimeConfigFormValues | null {
    const timezoneName = deps.timezoneSelect.value.trim();
    const manualDate = deps.manualDateInput.value.trim();
    const manualTime = deps.manualTimeInput.value.trim();
    const wakeupMinutes = deps.parseClockTimeInputValue(deps.wakeupTimeInput);
    const bedtimeMinutes = deps.parseClockTimeInputValue(deps.bedtimeTimeInput);

    if (Boolean(manualDate) !== Boolean(manualTime)) {
      deps.notify(
        'Provide both current date and time for a manual clock set.',
        'error'
      );
      return null;
    }

    return {
      timezoneName,
      manualDate,
      manualTime,
      wakeupMinutes: wakeupMinutes ?? undefined,
      bedtimeMinutes: bedtimeMinutes ?? undefined,
    };
  }

  async function updateTimeSettings(
    formValues: TimeConfigFormValues,
    enabled: boolean
  ): Promise<TimeSettingsResponse> {
    return deps.fetchTimeSettingsJson(deps.timeSettingsApi, {
      method: 'PATCH',
      body: JSON.stringify({
        timezone_name: formValues.timezoneName,
        enabled,
        manual_date: formValues.manualDate,
        manual_time: formValues.manualTime,
      }),
    });
  }

  async function toggleClockModeSetting() {
    if (isClockBusy || isTimezoneBusy || isLocationBusy) {
      deps.notifyClockMode('Clock mode update already in progress.', 'warning');
      return;
    }

    const previousEnabled = deps.isSwitchChecked(deps.clockModeToggle);
    const enabled = !previousEnabled;
    const timezoneName = deps.timezoneSelect.value.trim();
    deps.setSwitchChecked(deps.clockModeToggle, enabled);

    isClockBusy = true;
    deps.notifyClockMode(
      enabled ? 'Enabling clock mode...' : 'Disabling clock mode...',
      'info'
    );
    deps.onStateChange();

    try {
      const data = await deps.fetchTimeSettingsJson(deps.timeSettingsApi, {
        method: 'PATCH',
        body: JSON.stringify({
          enabled,
          timezone_name: enabled && timezoneName ? timezoneName : undefined,
        }),
      });
      applyTimeSettingsStatus(data);
      deps.notifyClockMode(
        data.message || (enabled ? 'Clock mode enabled.' : 'Clock mode disabled.'),
        'success'
      );
    } catch (error) {
      deps.setSwitchChecked(deps.clockModeToggle, previousEnabled);
      console.error('Clock mode toggle failed:', error);
      const errorMessage =
        error instanceof Error ? error.message : 'Failed to update clock mode.';
      const finalErrorMessage =
        errorMessage === 'timezone_name required to enable clock'
          ? 'Please set timezone first.'
          : errorMessage;
      if (errorMessage === 'timezone_name required to enable clock') {
        deps.setFieldError(deps.timezoneSelect, 'Select a timezone.');
        deps.timezoneSelect.focus({ preventScroll: true });
      }
      deps.notifyClockMode(finalErrorMessage, 'error');
    } finally {
      isClockBusy = false;
      deps.onStateChange();
    }
  }

  async function saveTimezoneLocation() {
    if (isTimezoneBusy || isLocationBusy || isClockBusy || deps.isTalkingClockModuleBusy()) {
      deps.notify('Time configuration update already in progress.', 'warning');
      return;
    }

    const formValues = getTimeConfigFormValues();
    if (!formValues) {
      return;
    }
    if (!formValues.timezoneName) {
      deps.setFieldError(deps.timezoneSelect, 'Select a timezone.');
      deps.notify('Select a timezone.', 'error');
      deps.timezoneSelect.focus({ preventScroll: true });
      return;
    }
    deps.clearFieldError(deps.timezoneSelect);
    if (
      deps.isTalkingClockModuleActive() &&
      (formValues.wakeupMinutes === undefined || formValues.bedtimeMinutes === undefined)
    ) {
      deps.notify(
        'Wakeup and bedtime times are required for the talking clock module.',
        'error'
      );
      deps.focusTalkingClockTimeInput();
      return;
    }

    isTimezoneBusy = true;
    isLocationBusy = true;
    isClockBusy = true;
    deps.onTalkingClockBusyChange(deps.isTalkingClockModuleActive());
    deps.notify('Saving time configuration...', 'info');
    deps.onStateChange();

    try {
      const data = await updateTimeSettings(
        formValues,
        deps.isSwitchChecked(deps.clockModeToggle)
      );
      applyTimeSettingsStatus(data);

      if (deps.isTalkingClockModuleActive()) {
        const moduleData = await deps.patchTalkingClockSettings({
          wakeup_minutes: formValues.wakeupMinutes,
          bedtime_minutes: formValues.bedtimeMinutes,
        });
        deps.applyTalkingClockModuleSettings(moduleData.settings);
      }
      deps.notify('Time configuration saved successfully.', 'success');
    } catch (error) {
      console.error('Timezone/location save failed:', error);
      deps.notify(
        error instanceof Error ? error.message : 'Failed to save time configuration.',
        'error'
      );
    } finally {
      isTimezoneBusy = false;
      isLocationBusy = false;
      isClockBusy = false;
      deps.onTalkingClockBusyChange(false);
      deps.onStateChange();
    }
  }

  async function clearTimezoneLocation() {
    if (isTimezoneBusy || isLocationBusy || isClockBusy) {
      deps.notify('Time configuration update already in progress.', 'warning');
      return;
    }

    isTimezoneBusy = true;
    isLocationBusy = true;
    isClockBusy = true;
    deps.notify('Clearing timezone/location...', 'info');
    deps.onStateChange();

    try {
      const data = await deps.fetchTimeSettingsJson(deps.timeSettingsApi, {
        method: 'PATCH',
        body: JSON.stringify({
          enabled: false,
          timezone_name: '',
        }),
      });
      applyTimeSettingsStatus(data);
      deps.notify('Timezone/location cleared successfully.', 'success');
    } catch (error) {
      console.error('Timezone/location clear failed:', error);
      deps.notify(
        error instanceof Error ? error.message : 'Failed to clear timezone/location.',
        'error'
      );
    } finally {
      isTimezoneBusy = false;
      isLocationBusy = false;
      isClockBusy = false;
      deps.onStateChange();
    }
  }

  return {
    applyTimeRuntimeStatus,
    applyTimeSettingsStatus,
    clearTimezoneLocation,
    fetchTimeRuntimeStatus,
    fetchTimeSettingsStatus,
    isClockBusy: () => isClockBusy,
    isLocationBusy: () => isLocationBusy,
    isTimezoneBusy: () => isTimezoneBusy,
    populateTimezoneOptions,
    refreshClockStatusAfterWifiConnect,
    saveTimezoneLocation,
    toggleClockModeSetting,
  };
}
