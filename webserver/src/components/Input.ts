import hideIcon from "../assets/hide.svg?raw";
import showIcon from "../assets/show.svg?raw";
import inputStyles from "./Input.css?raw";

const INPUT_TAG = "ui-input";
const INPUT_TEMPLATE = document.createElement("template");

INPUT_TEMPLATE.innerHTML = `
  <style>${inputStyles}</style>
  <div class="form-control">
    <label></label>
    <div class="form-control__input-wrapper">
      <input />
      <span class="duration-fields__unit"></span>
    </div>
    <p class="form-helper-text"></p>
  </div>
`;

type InputVariant = "text" | "number" | "password" | "time" | "date";

function normalizeVariant(variant: string | null): InputVariant {
  if (
    variant === "number" ||
    variant === "password" ||
    variant === "time" ||
    variant === "date"
  ) {
    return variant;
  }

  return "text";
}

function defaultInputType(variant: InputVariant) {
  if (variant === "password" || variant === "number" || variant === "time" || variant === "date") {
    return variant;
  }

  return "text";
}

export class Input extends HTMLElement {
  static get observedAttributes() {
    return [
      "aria-label",
      "aria-invalid",
      "autocomplete",
      "disabled",
      "helper-text",
      "invalid",
      "inputmode",
      "label",
      "max",
      "min",
      "name",
      "placeholder",
      "readonly",
      "required",
      "step",
      "suffix",
      "type",
      "value",
      "variant",
    ];
  }

  private isPasswordVisible = false;
  private customValidationMessage = "";
  private readonly input: HTMLInputElement;
  private readonly helperText: HTMLParagraphElement;
  private readonly label: HTMLLabelElement;
  private readonly toggleButton: HTMLButtonElement;
  private readonly suffix: HTMLSpanElement;
  private readonly wrapper: HTMLDivElement;

  constructor() {
    super();
    const shadow = this.attachShadow({ mode: "open", delegatesFocus: true });
    shadow.append(INPUT_TEMPLATE.content.cloneNode(true));
    this.input = shadow.querySelector("input")!;
    this.helperText = shadow.querySelector(".form-helper-text")!;
    this.label = shadow.querySelector("label")!;
    this.suffix = shadow.querySelector(".duration-fields__unit")!;
    this.toggleButton = document.createElement("button");
    this.wrapper = shadow.querySelector(".form-control__input-wrapper")!;
    this.toggleButton.className = "password-toggle-btn";
    this.toggleButton.type = "button";

    this.input.addEventListener("input", event => {
      this.syncValidationState();
      event.stopPropagation();
      this.dispatchEvent(new Event("input", { bubbles: true, composed: true }));
    });
    this.input.addEventListener("change", event => {
      this.syncValidationState();
      event.stopPropagation();
      this.dispatchEvent(new Event("change", { bubbles: true, composed: true }));
    });
    this.input.addEventListener("invalid", () => {
      this.syncValidationState();
    });
    this.toggleButton.addEventListener("click", () => {
      this.togglePasswordVisibility();
    });

  }

  connectedCallback() {
    if (!this.hasAttribute("variant")) {
      this.setAttribute("variant", "text");
    }

    this.dataset.component = INPUT_TAG;
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

  get type() {
    return this.input.type;
  }

  set type(type: string) {
    this.setAttribute("type", type);
  }

  get value() {
    return this.input.value;
  }

  set value(value: string) {
    this.input.value = value;
    this.syncValidationState();
  }

  focus(options?: FocusOptions) {
    this.input.focus(options);
  }

  select() {
    this.input.select();
  }

  get validationMessage() {
    return this.input.validationMessage;
  }

  get validity() {
    return this.input.validity;
  }

  get willValidate() {
    return this.input.willValidate;
  }

  checkValidity() {
    const isValid = this.input.checkValidity();
    this.syncValidationState();
    return isValid;
  }

  reportValidity() {
    const isValid = this.input.reportValidity();
    this.syncValidationState();
    return isValid;
  }

  setCustomValidity(message: string) {
    this.customValidationMessage = message;
    this.input.setCustomValidity(message);
    this.syncValidationState();
  }

  private syncAttributes() {
    const variant = normalizeVariant(this.getAttribute("variant"));
    const helperText = this.getAttribute("helper-text") || "";
    const inputId = this.id ? `${this.id}Input` : "input";
    const label = this.getAttribute("label") || "";
    const suffix = this.getAttribute("suffix") || "";
    const type =
      variant === "password"
        ? (this.isPasswordVisible ? "text" : "password")
        : this.getAttribute("type") || defaultInputType(variant);

    this.input.id = inputId;
    this.input.type = type;
    this.input.disabled = this.disabled;
    this.tabIndex = this.disabled ? -1 : 0;
    this.input.readOnly = this.hasAttribute("readonly");
    this.input.required = this.hasAttribute("required");
    this.syncStringAttribute("aria-label");
    this.syncStringAttribute("autocomplete");
    this.syncStringAttribute("inputmode");
    this.syncStringAttribute("max");
    this.syncStringAttribute("min");
    this.syncStringAttribute("name");
    this.syncStringAttribute("placeholder");
    this.syncStringAttribute("step");

    if (this.hasAttribute("value") && this.input.value !== this.getAttribute("value")) {
      this.input.value = this.getAttribute("value") || "";
    }

    this.input.setCustomValidity(this.customValidationMessage);

    this.label.htmlFor = inputId;
    this.label.textContent = label;
    this.label.hidden = label.length === 0;
    this.helperText.textContent = helperText;
    this.helperText.hidden = helperText.length === 0;
    this.wrapper.classList.toggle("duration-fields", variant === "number");
    this.suffix.textContent = suffix;
    this.suffix.hidden = variant !== "number" || suffix.length === 0;

    if (helperText.length > 0) {
      const helperId = `${inputId}HelperText`;
      this.helperText.id = helperId;
      this.input.setAttribute("aria-describedby", helperId);
    } else {
      this.helperText.removeAttribute("id");
      this.input.removeAttribute("aria-describedby");
    }

    if (variant === "password") {
      if (!this.toggleButton.isConnected) {
        this.wrapper.append(this.toggleButton);
      }
      this.toggleButton.disabled = this.disabled;
      this.toggleButton.setAttribute(
        "aria-label",
        this.isPasswordVisible ? "Hide password" : "Show password"
      );
      this.toggleButton.innerHTML = svgWithClass(
        this.isPasswordVisible ? hideIcon : showIcon,
        "password-icon"
      );
    } else {
      this.isPasswordVisible = false;
      this.toggleButton.remove();
    }

    this.syncValidationState();
  }

  private syncStringAttribute(name: string) {
    const value = this.getAttribute(name);
    if (value === null) {
      this.input.removeAttribute(name);
      return;
    }

    this.input.setAttribute(name, value);
  }

  private togglePasswordVisibility() {
    this.isPasswordVisible = !this.isPasswordVisible;
    this.syncAttributes();
  }

  private syncValidationState() {
    const invalid =
      this.hasAttribute("invalid") ||
      this.getAttribute("aria-invalid") === "true" ||
      !this.input.validity.valid;

    this.classList.toggle("error", invalid);
    this.input.classList.toggle("error", invalid);
    if (this.hasAttribute("invalid") !== invalid) {
      this.toggleAttribute("invalid", invalid);
    }
    if (this.getAttribute("aria-invalid") !== String(invalid)) {
      this.setAttribute("aria-invalid", String(invalid));
    }
    if (this.input.getAttribute("aria-invalid") !== String(invalid)) {
      this.input.setAttribute("aria-invalid", String(invalid));
    }
  }
}

function svgWithClass(svg: string, className: string): string {
  if (svg.includes("class=")) {
    return svg.replace("<svg", `<svg class="${className}"`);
  }

  return svg.replace("<svg", `<svg class="${className}"`);
}

export function defineInput() {
  if (customElements.get(INPUT_TAG)) {
    return;
  }

  customElements.define(INPUT_TAG, Input);
}
