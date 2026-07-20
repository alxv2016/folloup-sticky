import durationInputStyles from "./DurationInput.css?raw";

const DURATION_INPUT_TAG = "ui-duration-input";
const DURATION_INPUT_TEMPLATE = document.createElement("template");

DURATION_INPUT_TEMPLATE.innerHTML = `
  <style>${durationInputStyles}</style>
  <div class="form-control">
    <label></label>
    <div class="form-control__input-wrapper duration-fields">
      <ui-input variant="number" inputmode="numeric"></ui-input>
      <ui-input variant="number" inputmode="numeric"></ui-input>
    </div>
  </div>
`;

type PrimitiveInputField = HTMLElement & {
  disabled: boolean;
  focus: (options?: FocusOptions) => void;
  value: string;
};

function attributeOrDefault(element: Element, name: string, fallback: string) {
  return element.getAttribute(name) || fallback;
}

export class DurationInput extends HTMLElement {
  static get observedAttributes() {
    return [
      "disabled",
      "hours-id",
      "hours-max",
      "hours-min",
      "hours-step",
      "hours-suffix",
      "label",
      "minutes-id",
      "minutes-max",
      "minutes-min",
      "minutes-step",
      "minutes-suffix",
    ];
  }

  private readonly hoursInput: PrimitiveInputField;
  private readonly labelElement: HTMLLabelElement;
  private readonly minutesInput: PrimitiveInputField;

  constructor() {
    super();
    const shadow = this.attachShadow({ mode: "open", delegatesFocus: true });
    shadow.append(DURATION_INPUT_TEMPLATE.content.cloneNode(true));
    this.labelElement = shadow.querySelector("label")!;
    this.labelElement.addEventListener("click", () => {
      this.focus({ preventScroll: true });
    });

    const inputs = shadow.querySelectorAll("ui-input");
    this.hoursInput = inputs[0] as PrimitiveInputField;
    this.minutesInput = inputs[1] as PrimitiveInputField;

    this.forwardFieldEvents(this.hoursInput);
    this.forwardFieldEvents(this.minutesInput);
  }

  connectedCallback() {
    this.dataset.component = DURATION_INPUT_TAG;
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

  get hours() {
    return this.hoursInput;
  }

  get minutes() {
    return this.minutesInput;
  }

  focus(options?: FocusOptions) {
    this.hoursInput.focus(options);
  }

  private forwardFieldEvents(field: PrimitiveInputField) {
    field.addEventListener("input", event => {
      event.stopPropagation();
      this.dispatchEvent(new Event("input", { bubbles: true, composed: true }));
    });
    field.addEventListener("change", event => {
      event.stopPropagation();
      this.dispatchEvent(new Event("change", { bubbles: true, composed: true }));
    });
  }

  private syncAttributes() {
    const label = this.getAttribute("label") || "";
    const hoursId = attributeOrDefault(this, "hours-id", `${this.id || "duration"}HoursInput`);
    const minutesId = attributeOrDefault(this, "minutes-id", `${this.id || "duration"}MinutesInput`);

    this.tabIndex = this.disabled ? -1 : 0;
    this.labelElement.textContent = label;
    this.labelElement.hidden = label.length === 0;
    this.labelElement.htmlFor = hoursId;

    this.hoursInput.id = hoursId;
    this.minutesInput.id = minutesId;
    this.hoursInput.disabled = this.disabled;
    this.minutesInput.disabled = this.disabled;

    this.syncField(this.hoursInput, {
      ariaLabel: label ? `${label} hours` : "Hours",
      max: attributeOrDefault(this, "hours-max", "23"),
      min: attributeOrDefault(this, "hours-min", "0"),
      step: attributeOrDefault(this, "hours-step", "1"),
      suffix: attributeOrDefault(this, "hours-suffix", "hr"),
    });
    this.syncField(this.minutesInput, {
      ariaLabel: label ? `${label} minutes` : "Minutes",
      max: attributeOrDefault(this, "minutes-max", "59"),
      min: attributeOrDefault(this, "minutes-min", "0"),
      step: attributeOrDefault(this, "minutes-step", "1"),
      suffix: attributeOrDefault(this, "minutes-suffix", "min"),
    });
  }

  private syncField(
    field: PrimitiveInputField,
    config: {
      ariaLabel: string;
      max: string;
      min: string;
      step: string;
      suffix: string;
    }
  ) {
    field.setAttribute("aria-label", config.ariaLabel);
    field.setAttribute("max", config.max);
    field.setAttribute("min", config.min);
    field.setAttribute("step", config.step);
    field.setAttribute("suffix", config.suffix);
  }
}

export function defineDurationInput() {
  if (customElements.get(DURATION_INPUT_TAG)) {
    return;
  }

  customElements.define(DURATION_INPUT_TAG, DurationInput);
}
