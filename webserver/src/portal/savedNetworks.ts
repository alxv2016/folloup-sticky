import type { PortalResponse, StatusType, ValidatableField } from './types';

const SSID_MAX_LENGTH = 32;
const WIFI_CREDENTIAL_MAX_LENGTH = 64;
const WPA_PASSWORD_MIN_LENGTH = 8;

interface SavedNetworksControllerDeps {
  clearFieldError: (field: ValidatableField) => void;
  fetchPortalJson: (path: string, init?: RequestInit) => Promise<PortalResponse>;
  list: HTMLUListElement;
  notify: (message: string, type?: StatusType) => void;
  passwordInput: ValidatableField;
  setFieldError: (field: ValidatableField, message: string) => void;
  ssidInput: ValidatableField;
}

// Known networks the device rotates through when one is out of range. The list itself
// comes back on every /api/status response; this controller renders it and owns the
// add/remove calls.
export function createSavedNetworksController(deps: SavedNetworksControllerDeps) {
  let savedNetworks: string[] = [];
  let maxSavedNetworks = 5;
  let isBusy = false;

  function render() {
    deps.list.replaceChildren();
    if (savedNetworks.length === 0) {
      const empty = document.createElement('li');
      empty.className = 'saved-networks__empty';
      empty.textContent = 'No networks saved yet.';
      deps.list.appendChild(empty);
      return;
    }

    savedNetworks.forEach((ssid, index) => {
      const item = document.createElement('li');
      item.className = 'saved-networks__item';

      const label = document.createElement('span');
      const priority = document.createElement('span');
      priority.className = 'saved-networks__priority';
      priority.textContent = `${index + 1}.`;
      const name = document.createElement('span');
      name.className = 'saved-networks__ssid';
      name.textContent = ssid;
      label.append(priority, name);

      const removeButton = document.createElement('ui-button');
      removeButton.setAttribute('variant', 'secondary');
      removeButton.textContent = 'Remove';
      removeButton.addEventListener('click', () => {
        void remove(ssid);
      });

      item.append(label, removeButton);
      deps.list.appendChild(item);
    });
  }

  function apply(data: PortalResponse) {
    if (!Array.isArray(data.saved_networks)) {
      return;
    }
    savedNetworks = data.saved_networks;
    if (typeof data.max_saved_networks === 'number') {
      maxSavedNetworks = data.max_saved_networks;
    }
    render();
  }

  async function add() {
    if (isBusy) {
      return;
    }

    const ssid = deps.ssidInput.value.trim();
    const password = deps.passwordInput.value;
    if (ssid.length === 0 || ssid.length > SSID_MAX_LENGTH) {
      deps.setFieldError(deps.ssidInput, `Enter a network name (up to ${SSID_MAX_LENGTH} characters).`);
      deps.ssidInput.focus({ preventScroll: true });
      return;
    }
    deps.clearFieldError(deps.ssidInput);
    if (
      password.length > WIFI_CREDENTIAL_MAX_LENGTH ||
      (password.length > 0 && password.length < WPA_PASSWORD_MIN_LENGTH)
    ) {
      deps.setFieldError(
        deps.passwordInput,
        `Password must be ${WPA_PASSWORD_MIN_LENGTH}-${WIFI_CREDENTIAL_MAX_LENGTH} characters, or empty for an open network.`
      );
      deps.passwordInput.focus({ preventScroll: true });
      return;
    }
    deps.clearFieldError(deps.passwordInput);

    const replacesLast = !savedNetworks.includes(ssid) && savedNetworks.length >= maxSavedNetworks;
    isBusy = true;
    try {
      const data = await deps.fetchPortalJson('/api/networks', {
        method: 'POST',
        body: JSON.stringify({ ssid, password }),
      });
      apply(data);
      deps.ssidInput.value = '';
      deps.passwordInput.value = '';
      deps.notify(
        replacesLast
          ? `${data.message || `Saved ${ssid}`} (replaced the last network; the list holds ${maxSavedNetworks}).`
          : data.message || `Saved ${ssid}.`,
        'success'
      );
    } catch (error) {
      deps.notify(error instanceof Error ? error.message : 'Saving the network failed.', 'error');
    } finally {
      isBusy = false;
    }
  }

  async function remove(ssid: string) {
    if (isBusy) {
      return;
    }
    isBusy = true;
    try {
      const data = await deps.fetchPortalJson('/api/networks/remove', {
        method: 'POST',
        body: JSON.stringify({ ssid }),
      });
      apply(data);
      deps.notify(data.message || `Removed ${ssid}.`, 'success');
    } catch (error) {
      deps.notify(error instanceof Error ? error.message : 'Removing the network failed.', 'error');
    } finally {
      isBusy = false;
    }
  }

  return { add, apply, remove, render };
}
