import { API_HEADERS } from './constants';
import type {
  AudioStatusResponse,
  BootstrapResponse,
  DisplayStatusResponse,
  EffectsStatusResponse,
  GeminiModuleResponse,
  OpenAiModuleResponse,
  PortalResponse,
  PowerRuntimeStatus,
  SleepStatusResponse,
  TalkingClockModuleResponse,
  TimeRuntimeStatus,
  TimeSettingsResponse,
  TimezoneListResponse,
  UpdateRuntimeResponse,
  XiaozhiModuleResponse,
} from './types';

export async function fetchApiJson<T>(path: string, init?: RequestInit): Promise<T> {
  const response = await fetch(path, {
    cache: 'no-store',
    ...init,
    headers: {
      ...API_HEADERS,
      ...(init?.headers || {}),
    },
  });

  const contentType = response.headers.get('content-type') || '';
  const bodyText = await response.text();

  if (!contentType.includes('application/json')) {
    throw new Error(
      `Unexpected response for ${path}. ` +
        'Make sure you are connected to the device AP and loading the portal from the ESP32.'
    );
  }

  const data = JSON.parse(bodyText) as { success?: boolean; message?: string };
  if (!response.ok || data.success !== true) {
    throw new Error(data.message || 'Request failed.');
  }

  return data as T;
}

export const fetchPortalJson = (path: string, init?: RequestInit) =>
  fetchApiJson<PortalResponse>(path, init);
export const fetchTimeSettingsJson = (path: string, init?: RequestInit) =>
  fetchApiJson<TimeSettingsResponse>(path, init);
export const fetchTimeRuntimeJson = (path: string, init?: RequestInit) =>
  fetchApiJson<{ success: boolean; message?: string; runtime?: TimeRuntimeStatus }>(path, init);
export const fetchPowerRuntimeJson = (path: string, init?: RequestInit) =>
  fetchApiJson<{ success: boolean; message?: string; runtime?: PowerRuntimeStatus }>(path, init);
export const fetchUpdateRuntimeJson = (path: string, init?: RequestInit) =>
  fetchApiJson<UpdateRuntimeResponse>(path, init);
export const fetchAudioStatusJson = (path: string, init?: RequestInit) =>
  fetchApiJson<AudioStatusResponse>(path, init);
export const fetchDisplayStatusJson = (path: string, init?: RequestInit) =>
  fetchApiJson<DisplayStatusResponse>(path, init);
export const fetchSleepStatusJson = (path: string, init?: RequestInit) =>
  fetchApiJson<SleepStatusResponse>(path, init);
export const fetchBootstrapJson = (path: string, init?: RequestInit) =>
  fetchApiJson<BootstrapResponse>(path, init);
export const fetchEffectsJson = (path: string, init?: RequestInit) =>
  fetchApiJson<EffectsStatusResponse>(path, init);
export const fetchTalkingClockModuleJson = (path: string, init?: RequestInit) =>
  fetchApiJson<TalkingClockModuleResponse>(path, init);
export const fetchGeminiModuleJson = (path: string, init?: RequestInit) =>
  fetchApiJson<GeminiModuleResponse>(path, init);
export const fetchOpenAiModuleJson = (path: string, init?: RequestInit) =>
  fetchApiJson<OpenAiModuleResponse>(path, init);
export const fetchXiaozhiModuleJson = (path: string, init?: RequestInit) =>
  fetchApiJson<XiaozhiModuleResponse>(path, init);
export const fetchTimezoneListJson = (path: string, init?: RequestInit) =>
  fetchApiJson<TimezoneListResponse>(path, init);
