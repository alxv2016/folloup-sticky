import iconButtonStyles from "./IconButton.css?raw";

const ICON_BUTTON_TAG = "ui-icon-button";
const ICON_BUTTON_TEMPLATE = document.createElement("template");

ICON_BUTTON_TEMPLATE.innerHTML = `
  <style>${iconButtonStyles}</style>
  <button type="button" part="button">
    <slot></slot>
  </button>
`;

type ButtonType = "button" | "submit" | "reset";

function normalizeButtonType(type: string | null): ButtonType {
  if (type === "submit" || type === "reset") {
    return type;
  }

  return "button";
}

export class IconButton extends HTMLElement {
  static get observedAttributes() {
    return ["disabled", "aria-label", "type"];
  }

  private readonly button: HTMLButtonElement;

  constructor() {
    super();
    const shadow = this.attachShadow({ mode: "open", delegatesFocus: true });
    shadow.append(ICON_BUTTON_TEMPLATE.content.cloneNode(true));
    this.button = shadow.querySelector("button")!;
  }

  connectedCallback() {
    this.dataset.component = ICON_BUTTON_TAG;
    this.syncAttributes();
  }

  attributeChangedCallback() {
    this.syncAttributes();
  }

  get disabled() {
    return this.hasAttribute("disabled");
  }

  set disabled(disabled: boolean) {
    this.toggleAttribute("disabled", disabled);
  }

  get type(): ButtonType {
    return normalizeButtonType(this.getAttribute("type"));
  }

  set type(type: string) {
    this.setAttribute("type", normalizeButtonType(type));
  }

  focus(options?: FocusOptions) {
    this.button.focus(options);
  }

  click() {
    this.button.click();
  }

  private syncAttributes() {
    this.button.disabled = this.disabled;
    this.button.type = this.type;
    this.tabIndex = this.disabled ? -1 : 0;

    const ariaLabel = this.getAttribute("aria-label");
    if (ariaLabel) {
      this.button.setAttribute("aria-label", ariaLabel);
    } else {
      this.button.removeAttribute("aria-label");
    }
  }
}

export function defineIconButton() {
  if (customElements.get(ICON_BUTTON_TAG)) {
    return;
  }

  customElements.define(ICON_BUTTON_TAG, IconButton);
}
