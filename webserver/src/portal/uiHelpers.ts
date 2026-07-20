import type {
  CardField,
  ModuleId,
  StatusType,
  TimezoneListResponse,
  ValidatableField,
} from './types';

export function createCardNotifier(card: CardField) {
  return (message: string, type: StatusType = 'info') => {
    card.setNotification(message, type);
  };
}

export function createLiveRegionNotifier(element: HTMLElement) {
  return (message: string, type: StatusType = 'info') => {
    if (!message) {
      element.hidden = true;
      element.textContent = '';
      element.removeAttribute('data-status');
      element.setAttribute('role', 'status');
      element.setAttribute('aria-live', 'polite');
      return;
    }

    element.hidden = false;
    element.textContent = message;
    element.setAttribute('data-status', type);

    if (type === 'error' || type === 'danger') {
      element.setAttribute('role', 'alert');
      element.setAttribute('aria-live', 'assertive');
    } else if (type === 'warning') {
      element.setAttribute('role', 'alert');
      element.setAttribute('aria-live', 'polite');
    } else {
      element.setAttribute('role', 'status');
      element.setAttribute('aria-live', 'polite');
    }
  };
}

export function clearFieldError(field: ValidatableField) {
  field.classList.remove('error');
  field.removeAttribute('invalid');
  field.setAttribute('aria-invalid', 'false');
  field.setCustomValidity?.('');
}

export function setFieldError(field: ValidatableField, message: string) {
  field.classList.add('error');
  field.setAttribute('invalid', '');
  field.setAttribute('aria-invalid', 'true');
  field.setCustomValidity?.(message);
}

export function initSwitch(toggle: HTMLButtonElement) {
  if (!toggle) {
    return;
  }
  if (!toggle.hasAttribute('role')) {
    toggle.setAttribute('role', 'switch');
  }
  if (!toggle.hasAttribute('aria-checked')) {
    toggle.setAttribute('aria-checked', 'false');
  }
}

export function setSwitchChecked(toggle: HTMLButtonElement, checked: boolean) {
  toggle.setAttribute('aria-checked', checked ? 'true' : 'false');
}

export function isSwitchChecked(toggle: HTMLButtonElement): boolean {
  return toggle.getAttribute('aria-checked') === 'true';
}

export function svgWithClass(svg: string, className: string): string {
  if (svg.includes('class=')) {
    return svg.replace('<svg', `<svg class="${className}"`);
  }

  return svg.replace('<svg', `<svg class="${className}"`);
}

export function setIcon(target: HTMLElement, svg: string, className: string) {
  target.innerHTML = svgWithClass(svg, className);
}

export function normalizeModuleId(id?: string): ModuleId | '' {
  switch (id) {
    case 'core':
    case 'classic':
    case 'xiaozhi':
    case 'gemini':
    case 'openai':
      return id;
    default:
      return '';
  }
}

export function delayMs(durationMs: number): Promise<void> {
  return new Promise(resolve => {
    setTimeout(resolve, durationMs);
  });
}

export function formatBytes(bytes?: number): string {
  if (typeof bytes !== 'number' || !Number.isFinite(bytes) || bytes <= 0) {
    return '0 KB';
  }

  if (bytes >= 1024 * 1024) {
    return `${(bytes / (1024 * 1024)).toFixed(2)} MB`;
  }

  return `${(bytes / 1024).toFixed(1)} KB`;
}

function getTimezoneFormatterLocale(): string {
  const documentLanguage = document.documentElement.lang?.trim();
  if (documentLanguage) {
    return documentLanguage;
  }

  return 'en-US';
}

function getTimezoneGenericLabel(name: string): string {
  try {
    const formatter = new Intl.DateTimeFormat(getTimezoneFormatterLocale(), {
      hour: 'numeric',
      timeZone: name,
      timeZoneName: 'longGeneric',
    });
    const part = formatter
      .formatToParts(new Date())
      .find(item => item.type === 'timeZoneName');

    return part?.value?.trim() || '';
  } catch (error) {
    console.warn(`Failed to format timezone label for "${name}".`, error);
    return '';
  }
}

function isOffsetStyleTimezoneLabel(label: string): boolean {
  return /^(GMT|UTC)(?:[+-]\d{1,2}(?::\d{2})?)?$/.test(label);
}

function getTimezoneCityLabel(name: string): string {
  const lastSegment = name.split('/').pop() || name;
  return lastSegment.replace(/_/g, ' ');
}

function getTimezoneFallbackLabel(name: string): string {
  return name
    .split('/')
    .map(segment => segment.replace(/_/g, ' '))
    .join(' / ');
}

export function buildTimezoneLabelMap(
  timezones: Array<{ name: string; description?: string }>
) {
  const genericLabels = timezones.map(item => {
    const genericLabel = getTimezoneGenericLabel(item.name);
    return {
      cityLabel: getTimezoneCityLabel(item.name),
      genericLabel,
      name: item.name,
    };
  });
  const genericLabelByName = new Map(
    genericLabels.map(item => [item.name, item] as const)
  );
  const genericLabelCounts = new Map<string, number>();

  genericLabels.forEach(item => {
    if (!item.genericLabel || isOffsetStyleTimezoneLabel(item.genericLabel)) {
      return;
    }

    genericLabelCounts.set(
      item.genericLabel,
      (genericLabelCounts.get(item.genericLabel) || 0) + 1
    );
  });

  return new Map(
    timezones.map(item => {
      const generic = genericLabelByName.get(item.name);
      const description = item.description?.trim() || '';

      if (generic?.genericLabel && !isOffsetStyleTimezoneLabel(generic.genericLabel)) {
        const shouldDisambiguate = (genericLabelCounts.get(generic.genericLabel) || 0) > 1;
        const label = shouldDisambiguate
          ? `${generic.genericLabel} (${generic.cityLabel})`
          : generic.genericLabel;
        return [item.name, label];
      }

      if (description) {
        return [item.name, description];
      }

      return [item.name, getTimezoneFallbackLabel(item.name)];
    })
  );
}

export function formatTimezoneLabel(
  item: TimezoneListResponse['timezones'] extends Array<infer TimezoneItem>
    ? TimezoneItem
    : { name: string; description?: string },
  labelMap: Map<string, string>
) {
  return labelMap.get(item.name) || item.description?.trim() || getTimezoneFallbackLabel(item.name);
}
