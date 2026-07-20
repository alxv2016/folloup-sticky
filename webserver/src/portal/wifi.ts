import type {
  Network,
  NetworkListField,
  NetworkStatusField,
  PortalResponse,
  StatusType,
  ValidatableField,
} from './types';

interface ConnectionStateChange {
  connectedNetwork: string;
  isCurrentlyConnected: boolean;
  previousNetwork: string;
  wasConnected: boolean;
}

interface WiFiControllerDeps {
  checkIcon: string;
  clearFieldError: (field: ValidatableField) => void;
  delayMs: (durationMs: number) => Promise<void>;
  fetchPortalJson: (path: string, init?: RequestInit) => Promise<PortalResponse>;
  networkList: NetworkListField;
  notify: (message: string, type?: StatusType) => void;
  onConnectionStateChange?: (state: ConnectionStateChange) => void;
  onStateChange: () => void;
  passwordInput: ValidatableField;
  scanPollAttempts: number;
  scanPollIntervalMs: number;
  securityIcon: string;
  setFieldError: (field: ValidatableField, message: string) => void;
  signalIcons: {
    oneBar: string;
    twoBar: string;
    threeBar: string;
    fourBar: string;
  };
  statusPollAttempts: number;
  statusPollIntervalMs: number;
  svgWithClass: (svg: string, className: string) => string;
  wifiFindIcon: string;
  wifiStatusCard: NetworkStatusField;
}

function getSignalIcon(
  rssi: number,
  signalIcons: WiFiControllerDeps['signalIcons']
): string {
  if (rssi >= -50) return signalIcons.fourBar;
  if (rssi >= -60) return signalIcons.threeBar;
  if (rssi >= -70) return signalIcons.twoBar;
  return signalIcons.oneBar;
}

export function createWiFiController(deps: WiFiControllerDeps) {
  let networks: Network[] = [];
  let selectedNetwork = '';
  let isCurrentlyConnected = false;
  let connectedNetwork = '';
  let isScanning = false;
  let isCheckingStatus = false;
  let isConnecting = false;
  let focusedIndex = -1;

  function updateWifiStatusCard() {
    deps.wifiStatusCard.hidden = false;
    deps.wifiStatusCard.statusLabel = isCurrentlyConnected ? 'Connected' : 'Disconnected';
    deps.wifiStatusCard.network = isCurrentlyConnected ? connectedNetwork : '';
  }

  function renderEmptyNetworkState() {
    const emptyItem = document.createElement('li');
    emptyItem.className = 'networks__list-empty-state';
    emptyItem.innerHTML =
      `${deps.svgWithClass(deps.wifiFindIcon, 'wifi-find-icon')}<p>Scan for available networks</p>`;
    deps.networkList.setEmptyState(emptyItem);
  }

  function normalizeNetworkSelection() {
    if (networks.length === 0) {
      selectedNetwork = '';
      focusedIndex = -1;
      return;
    }

    const selectedIndex = networks.findIndex(item => item.ssid === selectedNetwork);
    if (selectedIndex >= 0) {
      focusedIndex = selectedIndex;
      return;
    }

    selectedNetwork = '';
    focusedIndex = -1;
  }

  function updateNetworkListSelectionState() {
    const options = deps.networkList.getOptions();
    options.forEach((option, index) => {
      option.setAttribute('aria-selected', String(selectedNetwork === networks[index]?.ssid));
    });

    if (focusedIndex >= 0 && focusedIndex < options.length) {
      deps.networkList.setActiveDescendant(options[focusedIndex].id);
    } else {
      deps.networkList.setActiveDescendant(null);
    }
  }

  function requiresPassword(): boolean {
    const network = networks.find(item => item.ssid === selectedNetwork);
    if (!network) {
      return true;
    }
    return !network.is_open;
  }

  function selectNetwork(ssid: string) {
    selectedNetwork = ssid;
    focusedIndex = networks.findIndex(item => item.ssid === ssid);
    if (!requiresPassword()) {
      deps.clearFieldError(deps.passwordInput);
    }
    updateNetworkListSelectionState();
    deps.onStateChange();
  }

  function renderNetworkList() {
    normalizeNetworkSelection();
    deps.networkList.clearItems();

    if (networks.length === 0) {
      renderEmptyNetworkState();
      return;
    }

    networks.forEach((network, index) => {
      const item = document.createElement('li');
      item.className = 'networks__item';
      item.id = `network-option-${index}`;
      item.setAttribute('role', 'option');
      item.setAttribute('tabindex', '-1');
      item.setAttribute('aria-selected', String(selectedNetwork === network.ssid));

      const ssidSpan = document.createElement('span');
      ssidSpan.className = 'networks__item-ssid';
      ssidSpan.textContent = network.ssid;

      const details = document.createElement('div');
      details.className = 'networks__item-details';

      if (connectedNetwork === network.ssid && isCurrentlyConnected) {
        const connectedSpan = document.createElement('span');
        connectedSpan.title = 'Currently connected';
        connectedSpan.innerHTML = deps.svgWithClass(deps.checkIcon, 'wifi-connected-icon');
        details.appendChild(connectedSpan);
      }

      const securitySpan = document.createElement('span');
      securitySpan.title = network.is_open ? 'Open' : 'Secured';
      if (!network.is_open) {
        securitySpan.innerHTML = deps.svgWithClass(deps.securityIcon, 'wifi-security-icon');
      }
      details.appendChild(securitySpan);

      const signalSpan = document.createElement('span');
      signalSpan.title = `Signal strength: ${network.signal_strength} (${network.rssi} dBm)`;
      signalSpan.innerHTML = deps.svgWithClass(
        getSignalIcon(network.rssi, deps.signalIcons),
        'wifi-signal-icon'
      );
      details.appendChild(signalSpan);

      item.append(ssidSpan, details);
      item.addEventListener('click', () => {
        focusedIndex = index;
        selectNetwork(network.ssid);
        deps.networkList.focus({ preventScroll: true });
      });

      deps.networkList.appendItem(item);
    });

    updateNetworkListSelectionState();
  }

  function applyPortalStatus(data: PortalResponse) {
    const wasConnected = isCurrentlyConnected;
    const previousNetwork = connectedNetwork;
    isCurrentlyConnected = data.connected === true;
    connectedNetwork = typeof data.ssid === 'string' ? data.ssid : '';
    updateWifiStatusCard();

    if (wasConnected !== isCurrentlyConnected || previousNetwork !== connectedNetwork) {
      renderNetworkList();
    }

    deps.onConnectionStateChange?.({
      wasConnected,
      previousNetwork,
      isCurrentlyConnected,
      connectedNetwork,
    });
  }

  async function scanNetworks() {
    if (isScanning) {
      return;
    }

    isScanning = true;
    deps.notify('Scanning for networks...', 'info');
    deps.onStateChange();

    try {
      let lastMessage = 'Scanning for networks...';

      for (let attempt = 0; attempt < deps.scanPollAttempts; attempt++) {
        const data = await deps.fetchPortalJson('/api/scan');
        lastMessage = data.message || lastMessage;

        if (data.scan_in_progress === true) {
          deps.notify(lastMessage, 'info');
          if (attempt < deps.scanPollAttempts - 1) {
            await deps.delayMs(deps.scanPollIntervalMs);
            continue;
          }
        } else {
          networks = Array.isArray(data.networks) ? data.networks : [];
          deps.notify(lastMessage || 'Scan complete.', 'success');
          renderNetworkList();
          return;
        }
      }

      throw new Error('Network scan timed out. Please try again.');
    } catch (error) {
      console.error('Scan failed:', error);
      deps.notify(
        error instanceof Error ? error.message : 'Scan failed. Please try again.',
        'error'
      );
      networks = [];
      renderNetworkList();
    } finally {
      isScanning = false;
      deps.onStateChange();
    }
  }

  async function checkStatus() {
    if (isCheckingStatus) {
      return;
    }

    isCheckingStatus = true;
    deps.onStateChange();

    try {
      const data = await deps.fetchPortalJson('/api/status');
      applyPortalStatus(data);
      if (!isScanning) {
        deps.notify(data.message || 'Status updated.', data.connected ? 'success' : 'info');
      }
    } catch (error) {
      console.error('Status check failed:', error);
      deps.notify(
        error instanceof Error ? error.message : 'Status check failed.',
        'error'
      );
    } finally {
      isCheckingStatus = false;
      deps.onStateChange();
    }
  }

  async function connect() {
    if (isConnecting || !selectedNetwork) {
      return;
    }

    if (requiresPassword() && deps.passwordInput.value.trim().length === 0) {
      deps.setFieldError(deps.passwordInput, 'Please enter a WiFi password.');
      deps.passwordInput.focus({ preventScroll: true });
      return;
    }
    deps.clearFieldError(deps.passwordInput);

    isConnecting = true;
    deps.notify(`Connecting to ${selectedNetwork}...`, 'info');
    deps.onStateChange();

    try {
      const data = await deps.fetchPortalJson('/api/configure', {
        method: 'POST',
        body: JSON.stringify({
          ssid: selectedNetwork,
          password: deps.passwordInput.value,
        }),
      });

      deps.notify(data.message || 'Connection request sent.', 'info');

      for (let attempt = 0; attempt < deps.statusPollAttempts; attempt++) {
        await deps.delayMs(deps.statusPollIntervalMs);

        const statusData = await deps.fetchPortalJson('/api/status');
        applyPortalStatus(statusData);

        if (statusData.connected) {
          deps.notify(statusData.message || 'Connected.', 'success');
          break;
        }
      }

      if (!isCurrentlyConnected) {
        deps.notify('Connection in progress...', 'info');
      }
    } catch (error) {
      console.error('Connection failed:', error);
      deps.notify(
        error instanceof Error ? error.message : 'Connection failed.',
        'error'
      );
    } finally {
      isConnecting = false;
      deps.onStateChange();
    }
  }

  async function disconnect() {
    if (isConnecting || isCheckingStatus || !isCurrentlyConnected) {
      return;
    }

    isCheckingStatus = true;
    deps.notify('Disconnecting...', 'info');
    deps.onStateChange();

    try {
      const data = await deps.fetchPortalJson('/api/disconnect', { method: 'POST' });
      const wasConnected = isCurrentlyConnected;
      const previousNetwork = connectedNetwork;

      isCurrentlyConnected = false;
      connectedNetwork = '';
      updateWifiStatusCard();
      renderNetworkList();
      deps.notify(data.message || 'Disconnected.', 'success');
      deps.onConnectionStateChange?.({
        wasConnected,
        previousNetwork,
        isCurrentlyConnected,
        connectedNetwork,
      });
    } catch (error) {
      console.error('Disconnect failed:', error);
      deps.notify(
        error instanceof Error ? error.message : 'Disconnect failed.',
        'error'
      );
    } finally {
      isCheckingStatus = false;
      deps.onStateChange();
    }
  }

  function handleConnectAction() {
    if (isSelectedConnected()) {
      void disconnect();
      return;
    }

    if (isConnecting || isCheckingStatus) {
      deps.notify('WiFi status is busy. Please wait.', 'warning');
      return;
    }

    if (!selectedNetwork) {
      deps.notify('Select a WiFi network first.', 'error');
      deps.networkList.focus({ preventScroll: true });
      return;
    }

    if (requiresPassword() && deps.passwordInput.value.trim().length === 0) {
      deps.setFieldError(deps.passwordInput, 'Please enter a WiFi password.');
      deps.notify('Please check if your password is correct.', 'error');
      deps.passwordInput.focus({ preventScroll: true });
      return;
    }

    deps.clearFieldError(deps.passwordInput);
    void connect();
  }

  function handlePasswordInput() {
    if (deps.passwordInput.value?.trim().length) {
      deps.clearFieldError(deps.passwordInput);
    }
    deps.onStateChange();
  }

  function handleListboxKeyDown(event: KeyboardEvent) {
    if (networks.length === 0) {
      return;
    }

    normalizeNetworkSelection();

    switch (event.key) {
      case 'Tab':
        break;
      case 'ArrowDown': {
        event.preventDefault();
        const nextIndex = focusedIndex < networks.length - 1 ? focusedIndex + 1 : 0;
        focusedIndex = nextIndex;
        selectNetwork(networks[nextIndex].ssid);
        deps.networkList.scrollOptionIntoView(nextIndex);
        break;
      }
      case 'ArrowUp': {
        event.preventDefault();
        const prevIndex = focusedIndex > 0 ? focusedIndex - 1 : networks.length - 1;
        focusedIndex = prevIndex;
        selectNetwork(networks[prevIndex].ssid);
        deps.networkList.scrollOptionIntoView(prevIndex);
        break;
      }
      case 'Home': {
        event.preventDefault();
        focusedIndex = 0;
        selectNetwork(networks[0].ssid);
        deps.networkList.scrollOptionIntoView(0);
        break;
      }
      case 'End': {
        event.preventDefault();
        const lastIndex = networks.length - 1;
        focusedIndex = lastIndex;
        selectNetwork(networks[lastIndex].ssid);
        deps.networkList.scrollOptionIntoView(lastIndex);
        break;
      }
      case 'Enter':
      case ' ': {
        event.preventDefault();
        if (focusedIndex >= 0 && focusedIndex < networks.length) {
          selectNetwork(networks[focusedIndex].ssid);
        }
        break;
      }
    }
  }

  function isSelectedConnected() {
    return (
      isCurrentlyConnected &&
      connectedNetwork.length > 0 &&
      selectedNetwork === connectedNetwork
    );
  }

  return {
    applyPortalStatus,
    checkStatus,
    connect,
    disconnect,
    getConnectedNetwork: () => connectedNetwork,
    getSelectedNetwork: () => selectedNetwork,
    handleConnectAction,
    handleListboxKeyDown,
    handlePasswordInput,
    isCheckingStatus: () => isCheckingStatus,
    isConnecting: () => isConnecting,
    isCurrentlyConnected: () => isCurrentlyConnected,
    isScanning: () => isScanning,
    isSelectedConnected,
    renderNetworkList,
    requiresPassword,
    scanNetworks,
    updateWifiStatusCard,
  };
}
