import type { DurationInputs } from './types';

export function clampDurationMinutes(minutes: number): number {
  return Math.max(1, Math.min(minutes, (23 * 60) + 59));
}

export function setDurationInputsValue(inputs: DurationInputs, minutes: number) {
  const clampedMinutes = clampDurationMinutes(minutes);
  inputs.hours.value = String(Math.floor(clampedMinutes / 60));
  inputs.minutes.value = String(clampedMinutes % 60);
}

export function parseDurationInputsValue(inputs: DurationInputs): number | null {
  const hoursValue = inputs.hours.value.trim();
  const minutesValue = inputs.minutes.value.trim();
  const hours = Number.parseInt(hoursValue || '0', 10);
  const minutes = Number.parseInt(minutesValue || '0', 10);

  if (!Number.isFinite(hours) || !Number.isFinite(minutes)) {
    return null;
  }
  if (hours < 0 || hours > 23 || minutes < 0 || minutes > 59) {
    return null;
  }

  const totalMinutes = (hours * 60) + minutes;
  return totalMinutes > 0 ? totalMinutes : null;
}

export function focusDurationInputs(inputs: DurationInputs) {
  inputs.hours.focus({ preventScroll: true });
}

export function formatClockMinutes(totalMinutes: number): string {
  const normalizedMinutes = ((totalMinutes % (24 * 60)) + (24 * 60)) % (24 * 60);
  const hours = Math.floor(normalizedMinutes / 60);
  const minutes = normalizedMinutes % 60;
  return `${String(hours).padStart(2, '0')}:${String(minutes).padStart(2, '0')}`;
}

export function formatDurationMinutes(totalMinutes: number): string {
  const normalizedMinutes = clampDurationMinutes(totalMinutes);
  const hours = Math.floor(normalizedMinutes / 60);
  const minutes = normalizedMinutes % 60;

  if (hours === 0) {
    return `${minutes}m`;
  }

  return `${hours}:${String(minutes).padStart(2, '0')}h`;
}

export function parseClockTimeInputValue(input: { value: string }): number | null {
  const value = input.value.trim();
  if (!value) {
    return null;
  }

  const match = /^(\d{2}):(\d{2})$/.exec(value);
  if (!match) {
    return null;
  }

  const hours = Number.parseInt(match[1], 10);
  const minutes = Number.parseInt(match[2], 10);
  if (!Number.isFinite(hours) || !Number.isFinite(minutes)) {
    return null;
  }
  if (hours < 0 || hours > 23 || minutes < 0 || minutes > 59) {
    return null;
  }

  return (hours * 60) + minutes;
}
