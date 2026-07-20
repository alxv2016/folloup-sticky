import rangeSliderStyles from "./RangeSlider.css?raw";

const RANGE_SLIDER_TAG = "ui-range-slider";
const RANGE_SLIDER_TEMPLATE = document.createElement("template");

RANGE_SLIDER_TEMPLATE.innerHTML = `
  <style>${rangeSliderStyles}</style>
  <div class="form-control">
    <label></label>
    <div class="range-slider">
      <input class="range-slider__range" type="range" />
      <span class="range-slider__value"></span>
    </div>
  </div>
`;

export class RangeSlider extends HTMLElement {
  static get observedAttributes() {
    return ["aria-label", "disabled", "label", "max", "min", "name", "step", "value"];
  }

  private readonly input: HTMLInputElement;
  private readonly labelElement: HTMLLabelElement;
  private readonly valueElement: HTMLSpanElement;

  constructor() {
    super();
    const shadow = this.attachShadow({ mode: "open", delegatesFocus: true });
    shadow.append(RANGE_SLIDER_TEMPLATE.content.cloneNode(true));
    this.labelElement = shadow.querySelector("label")!;
    this.input = shadow.querySelector("input")!;
    this.valueElement = shadow.querySelector(".range-slider__value")!;

    this.input.addEventListener("input", event => {
      this.updateValueDisplay();
      event.stopPropagation();
      this.dispatchEvent(new Event("input", { bubbles: true, composed: true }));
    });
    this.input.addEventListener("change", event => {
      this.updateValueDisplay();
      event.stopPropagation();
      this.dispatchEvent(new Event("change", { bubbles: true, composed: true }));
    });

  }

  connectedCallback() {
    this.dataset.component = RANGE_SLIDER_TAG;
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

  get value() {
    return this.input.value;
  }

  set value(value: string) {
    this.input.value = value;
    this.updateValueDisplay();
  }

  focus(options?: FocusOptions) {
    this.input.focus(options);
  }

  private syncAttributes() {
    const inputId = this.id || "range-slider";
    const label = this.getAttribute("label") || "";

    this.input.id = inputId;
    this.input.disabled = this.disabled;
    this.tabIndex = this.disabled ? -1 : 0;
    this.syncStringAttribute("aria-label");
    this.syncStringAttribute("max");
    this.syncStringAttribute("min");
    this.syncStringAttribute("name");
    this.syncStringAttribute("step");

    if (this.hasAttribute("value")) {
      this.input.value = this.getAttribute("value") || "";
    }

    this.labelElement.htmlFor = inputId;
    this.labelElement.textContent = label;
    this.labelElement.hidden = label.length === 0;
    this.updateValueDisplay();
  }

  private syncStringAttribute(name: string) {
    const value = this.getAttribute(name);
    if (value === null) {
      this.input.removeAttribute(name);
      return;
    }

    this.input.setAttribute(name, value);
  }

  private updateValueDisplay() {
    this.valueElement.textContent = this.input.value;
  }
}

export function defineRangeSlider() {
  if (customElements.get(RANGE_SLIDER_TAG)) {
    return;
  }

  customElements.define(RANGE_SLIDER_TAG, RangeSlider);
}
