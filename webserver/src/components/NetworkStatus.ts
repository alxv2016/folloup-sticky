import networkStatusStyles from "./NetworkStatus.css?raw";

const NETWORK_STATUS_TAG = "ui-network-status";
const ACTION_BUTTON_ID = "actionBtn";
const SETTINGS_BUTTON_ID = "settingsBtn";
const WIFI_STATUS_LABEL_ID = "wifiStatusLabel";
const WIFI_STATUS_ICON_ID = "wifiStatusIcon";
const WIFI_STATUS_NETWORK_ID = "wifiStatusNetwork";
const NETWORK_STATUS_TEMPLATE = document.createElement("template");

NETWORK_STATUS_TEMPLATE.innerHTML = `
  <style>${networkStatusStyles}</style>
  <div class="wifi-status-card">
    <div class="wifi-status-card__content">
      <div class="wifi-status-card__status">
        <span class="card-icon" id="${WIFI_STATUS_ICON_ID}"></span>
        <span id="${WIFI_STATUS_LABEL_ID}" class="wifi-status-card__label">
          Disconnected
        </span>
        <span
          id="${WIFI_STATUS_NETWORK_ID}"
          class="wifi-status-card__network"
        ></span>
      </div>
      <div class="wifi-status-card__actions">
        <ui-button
          id="${ACTION_BUTTON_ID}"
          variant="outline"
          inverse
          aria-label="Connect"
        >
          Connect
        </ui-button>
        <ui-button
          id="${SETTINGS_BUTTON_ID}"
          variant="outline"
          inverse
          aria-label="Settings"
        >
          Settings
        </ui-button>
      </div>
    </div>
  </div>
`;

function svgWithClass(svg: string, className: string): string {
  if (svg.includes("class=")) {
    return svg.replace("<svg", `<svg class="${className}"`);
  }

  return svg.replace("<svg", `<svg class="${className}"`);
}

export class NetworkStatus extends HTMLElement {
  static get observedAttributes() {
    return ["action-label", "network", "status-label"];
  }

  private readonly actionButton: HTMLElement & {
    disabled?: boolean;
  };
  private readonly iconElement: HTMLSpanElement;
  private readonly labelElement: HTMLSpanElement;
  private readonly networkElement: HTMLSpanElement;
  private readonly settingsButton: HTMLElement & {
    disabled?: boolean;
  };

  constructor() {
    super();
    const shadow = this.attachShadow({ mode: "open" });
    shadow.append(NETWORK_STATUS_TEMPLATE.content.cloneNode(true));

    this.actionButton = shadow.querySelector(`#${ACTION_BUTTON_ID}`)!;
    this.actionButton.addEventListener("click", event => {
      event.stopPropagation();
      this.dispatchEvent(new Event("action", { bubbles: true, composed: true }));
    });
    this.iconElement = shadow.querySelector(`#${WIFI_STATUS_ICON_ID}`)!;
    this.labelElement = shadow.querySelector(`#${WIFI_STATUS_LABEL_ID}`)!;
    this.networkElement = shadow.querySelector(`#${WIFI_STATUS_NETWORK_ID}`)!;
    this.settingsButton = shadow.querySelector(`#${SETTINGS_BUTTON_ID}`)!;
    this.settingsButton.addEventListener("click", event => {
      event.stopPropagation();
      this.dispatchEvent(new Event("settings", { bubbles: true, composed: true }));
    });
  }

  connectedCallback() {
    this.dataset.component = NETWORK_STATUS_TAG;
    this.syncAttributes();
  }

  attributeChangedCallback() {
    this.syncAttributes();
  }

  get actionDisabled() {
    return Boolean(this.actionButton.disabled);
  }

  set actionDisabled(disabled: boolean) {
    this.actionButton.disabled = disabled;
  }

  get iconSvg() {
    return this.iconElement.innerHTML;
  }

  set iconSvg(svg: string) {
    this.iconElement.innerHTML = svg ? svgWithClass(svg, "wifi-icon") : "";
  }

  get network() {
    return this.getAttribute("network") || "";
  }

  set network(value: string) {
    this.setAttribute("network", value);
  }

  get actionLabel() {
    return this.getAttribute("action-label") || "Connect";
  }

  set actionLabel(value: string) {
    this.setAttribute("action-label", value);
  }

  get statusLabel() {
    return this.getAttribute("status-label") || "Disconnected";
  }

  set statusLabel(value: string) {
    this.setAttribute("status-label", value);
  }

  private syncAttributes() {
    this.actionButton.textContent = this.actionLabel;
    this.actionButton.setAttribute("aria-label", this.actionLabel);
    this.labelElement.textContent = this.statusLabel;
    this.networkElement.textContent = this.network;
    this.networkElement.hidden = this.network.length === 0;
  }
}

export function defineNetworkStatus() {
  if (customElements.get(NETWORK_STATUS_TAG)) {
    return;
  }

  customElements.define(NETWORK_STATUS_TAG, NetworkStatus);
}
