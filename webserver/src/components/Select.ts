import expandIcon from "../assets/expand.svg?raw";
import selectStyles from "./Select.css?raw";

const SELECT_TAG = "ui-select";
const SELECT_TEMPLATE = document.createElement("template");

SELECT_TEMPLATE.innerHTML = `
  <style>${selectStyles}</style>
  <div class="form-control">
    <label></label>
    <div class="form-control__input-wrapper">
      <select></select>
      <span class="select-icon"></span>
    </div>
    <p class="form-helper-text"></p>
  </div>
`;

export class Select extends HTMLElement {
  static get observedAttributes() {
    return [
      "aria-invalid",
      "aria-label",
      "disabled",
      "helper-text",
      "invalid",
      "label",
      "name",
      "required",
      "value",
    ];
  }

  private customValidationMessage = "";
  private readonly helperText: HTMLParagraphElement;
  private readonly labelElement: HTMLLabelElement;
  private readonly select: HTMLSelectElement;

  constructor() {
    super();
    const shadow = this.attachShadow({ mode: "open", delegatesFocus: true });
    shadow.append(SELECT_TEMPLATE.content.cloneNode(true));
    this.helperText = shadow.querySelector(".form-helper-text")!;
    this.labelElement = shadow.querySelector("label")!;
    this.select = shadow.querySelector("select")!;
    const icon = shadow.querySelector(".select-icon") as HTMLSpanElement;
    icon.innerHTML = svgWithClass(expandIcon, "select-icon");
    this.select.addEventListener("change", event => {
      this.syncValidationState();
      event.stopPropagation();
      this.dispatchEvent(new Event("change", { bubbles: true, composed: true }));
    });
    this.select.addEventListener("input", event => {
      this.syncValidationState();
      event.stopPropagation();
      this.dispatchEvent(new Event("input", { bubbles: true, composed: true }));
    });
    this.select.addEventListener("invalid", () => {
      this.syncValidationState();
    });

  }

  connectedCallback() {
    this.moveLightDomOptions();
    this.dataset.component = SELECT_TAG;
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

  get innerHTML() {
    return this.select.innerHTML;
  }

  set innerHTML(value: string) {
    this.select.innerHTML = value;
    this.syncValidationState();
  }

  get value() {
    return this.select.value;
  }

  set value(value: string) {
    this.select.value = value;
    this.syncValidationState();
  }

  appendChild<T extends Node>(node: T): T {
    if (node instanceof HTMLOptionElement || node instanceof HTMLOptGroupElement) {
      this.select.appendChild(node);
      return node;
    }

    return super.appendChild(node);
  }

  focus(options?: FocusOptions) {
    this.select.focus(options);
  }

  get validationMessage() {
    return this.select.validationMessage;
  }

  get validity() {
    return this.select.validity;
  }

  get willValidate() {
    return this.select.willValidate;
  }

  checkValidity() {
    const isValid = this.select.checkValidity();
    this.syncValidationState();
    return isValid;
  }

  reportValidity() {
    const isValid = this.select.reportValidity();
    this.syncValidationState();
    return isValid;
  }

  setCustomValidity(message: string) {
    this.customValidationMessage = message;
    this.select.setCustomValidity(message);
    this.syncValidationState();
  }

  private moveLightDomOptions() {
    Array.from(this.children).forEach(child => {
      if (child instanceof HTMLOptionElement || child instanceof HTMLOptGroupElement) {
        this.select.appendChild(child);
      }
    });
  }

  private syncAttributes() {
    const helperText = this.getAttribute("helper-text") || "";
    const selectId = this.id ? `${this.id}Select` : "select";
    const label = this.getAttribute("label") || "";

    this.select.id = selectId;
    this.select.disabled = this.disabled;
    this.tabIndex = this.disabled ? -1 : 0;
    this.select.required = this.hasAttribute("required");
    this.syncStringAttribute("aria-label");
    this.syncStringAttribute("name");

    if (this.hasAttribute("value")) {
      this.select.value = this.getAttribute("value") || "";
    }

    this.select.setCustomValidity(this.customValidationMessage);

    this.labelElement.htmlFor = selectId;
    this.labelElement.textContent = label;
    this.labelElement.hidden = label.length === 0;
    this.helperText.textContent = helperText;
    this.helperText.hidden = helperText.length === 0;

    if (helperText.length > 0) {
      const helperId = `${selectId}HelperText`;
      this.helperText.id = helperId;
      this.select.setAttribute("aria-describedby", helperId);
    } else {
      this.helperText.removeAttribute("id");
      this.select.removeAttribute("aria-describedby");
    }

    this.syncValidationState();
  }

  private syncStringAttribute(name: string) {
    const value = this.getAttribute(name);
    if (value === null) {
      this.select.removeAttribute(name);
      return;
    }

    this.select.setAttribute(name, value);
  }

  private syncValidationState() {
    const invalid =
      this.hasAttribute("invalid") ||
      this.getAttribute("aria-invalid") === "true" ||
      !this.select.validity.valid;

    this.classList.toggle("error", invalid);
    this.select.classList.toggle("error", invalid);
    if (this.hasAttribute("invalid") !== invalid) {
      this.toggleAttribute("invalid", invalid);
    }
    if (this.getAttribute("aria-invalid") !== String(invalid)) {
      this.setAttribute("aria-invalid", String(invalid));
    }
    if (this.select.getAttribute("aria-invalid") !== String(invalid)) {
      this.select.setAttribute("aria-invalid", String(invalid));
    }
  }
}

export function defineSelect() {
  if (customElements.get(SELECT_TAG)) {
    return;
  }

  customElements.define(SELECT_TAG, Select);
}

function svgWithClass(svg: string, className: string): string {
  if (svg.includes("class=")) {
    return svg.replace("<svg", `<svg class="${className}"`);
  }

  return svg.replace("<svg", `<svg class="${className}"`);
}
