import toggleStyles from "./Toggle.css?raw";

const TOGGLE_TAG = "ui-toggle";
const TOGGLE_TEMPLATE = document.createElement("template");

TOGGLE_TEMPLATE.innerHTML = `
  <style>${toggleStyles}</style>
  <div class="form-control">
    <div class="switch-container">
      <button type="button" part="button">
        <span class="knob"></span>
      </button>
      <span class="switch-label"></span>
    </div>
  </div>
`;

type ButtonType = "button" | "submit" | "reset";

function normalizeButtonType(type: string | null): ButtonType {
  if (type === "submit" || type === "reset") {
    return type;
  }

  return "button";
}

export class Toggle extends HTMLElement {
  static get observedAttributes() {
    return ["aria-checked", "aria-label", "disabled", "label", "type"];
  }

  private readonly button: HTMLButtonElement;
  private readonly knob: HTMLSpanElement;
  private readonly labelElement: HTMLSpanElement;

  constructor() {
    super();
    const shadow = this.attachShadow({ mode: "open", delegatesFocus: true });
    shadow.append(TOGGLE_TEMPLATE.content.cloneNode(true));
    this.button = shadow.querySelector("button")!;
    this.knob = shadow.querySelector(".knob")!;
    this.labelElement = shadow.querySelector(".switch-label")!;
    this.labelElement.addEventListener("click", () => {
      if (!this.disabled) {
        this.button.click();
      }
    });
  }

  connectedCallback() {
    if (!this.hasAttribute("role")) {
      this.setAttribute("role", "switch");
    }
    if (!this.hasAttribute("aria-checked")) {
      this.setAttribute("aria-checked", "false");
    }

    this.dataset.component = TOGGLE_TAG;
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
    const checked = this.getAttribute("aria-checked") === "true";
    const ariaLabel = this.getAttribute("aria-label");
    const label = this.getAttribute("label") || "";

    this.button.disabled = this.disabled;
    this.button.type = this.type;
    this.tabIndex = this.disabled ? -1 : 0;
    this.button.setAttribute("role", "switch");
    this.button.setAttribute("aria-checked", String(checked));
    if (ariaLabel) {
      this.button.setAttribute("aria-label", ariaLabel);
    } else {
      this.button.removeAttribute("aria-label");
    }

    this.button.toggleAttribute("data-checked", checked);
    this.knob.style.transform = checked ? "translateX(28px)" : "translateX(0)";
    this.labelElement.textContent = label;
    this.labelElement.hidden = label.length === 0;
  }
}

export function defineToggle() {
  if (customElements.get(TOGGLE_TAG)) {
    return;
  }

  customElements.define(TOGGLE_TAG, Toggle);
}
