import fileUploadStyles from "./FileUpload.css?raw";

const FILE_UPLOAD_TAG = "ui-file-upload";
const FILE_UPLOAD_TEMPLATE = document.createElement("template");

FILE_UPLOAD_TEMPLATE.innerHTML = `
  <style>${fileUploadStyles}</style>
  <div class="form-control">
    <label></label>
    <div class="form-control__input-wrapper">
      <input type="file" />
    </div>
    <div class="progress-bar">
      <progress value="0" max="100"></progress>
      <div class="progress-bar__message"></div>
    </div>
    <p class="form-helper-text"></p>
  </div>
`;

export class FileUpload extends HTMLElement {
  static get observedAttributes() {
    return [
      "accept",
      "aria-label",
      "disabled",
      "label",
      "multiple",
      "name",
      "progress-max",
      "progress-message",
      "progress-value",
      "warning-text",
    ];
  }

  private readonly helperText: HTMLParagraphElement;
  private readonly input: HTMLInputElement;
  private readonly labelElement: HTMLLabelElement;
  private readonly progressBar: HTMLProgressElement;
  private readonly progressContainer: HTMLDivElement;
  private readonly progressMessage: HTMLDivElement;

  constructor() {
    super();
    const shadow = this.attachShadow({ mode: "open", delegatesFocus: true });
    shadow.append(FILE_UPLOAD_TEMPLATE.content.cloneNode(true));
    this.helperText = shadow.querySelector(".form-helper-text")!;
    this.input = shadow.querySelector("input")!;
    this.labelElement = shadow.querySelector("label")!;
    this.progressBar = shadow.querySelector("progress")!;
    this.progressContainer = shadow.querySelector(".progress-bar")!;
    this.progressMessage = shadow.querySelector(".progress-bar__message")!;

    this.input.addEventListener("change", event => {
      event.stopPropagation();
      this.dispatchEvent(new Event("change", { bubbles: true, composed: true }));
    });
    this.input.addEventListener("input", event => {
      event.stopPropagation();
      this.dispatchEvent(new Event("input", { bubbles: true, composed: true }));
    });

  }

  connectedCallback() {
    this.dataset.component = FILE_UPLOAD_TAG;
    this.syncAttributes();
  }

  attributeChangedCallback() {
    this.syncAttributes();
  }

  get accept() {
    return this.input.accept;
  }

  set accept(value: string) {
    this.setAttribute("accept", value);
  }

  get disabled() {
    return this.hasAttribute("disabled");
  }

  set disabled(disabled: boolean) {
    this.toggleAttribute("disabled", disabled);
  }

  get files() {
    return this.input.files;
  }

  get progressHidden() {
    return this.progressContainer.hidden;
  }

  set progressHidden(hidden: boolean) {
    this.progressContainer.hidden = hidden;
  }

  get progressMax() {
    return this.progressBar.max;
  }

  set progressMax(value: number) {
    this.progressBar.max = value;
  }

  get progressMessageText() {
    return this.progressMessage.textContent || "";
  }

  set progressMessageText(value: string) {
    this.progressMessage.textContent = value;
  }

  get progressValue() {
    return this.progressBar.value;
  }

  set progressValue(value: number) {
    this.progressBar.value = value;
  }

  get value() {
    return this.input.value;
  }

  get warningText() {
    return this.helperText.textContent || "";
  }

  set warningText(value: string) {
    this.helperText.textContent = value;
  }

  focus(options?: FocusOptions) {
    this.input.focus(options);
  }

  private syncAttributes() {
    const inputId = this.id || "file-upload";
    const label = this.getAttribute("label") || "";
    const warningText = this.getAttribute("warning-text") || "";

    this.input.id = inputId;
    this.input.accept = this.getAttribute("accept") || "";
    this.input.disabled = this.disabled;
    this.tabIndex = this.disabled ? -1 : 0;
    this.input.multiple = this.hasAttribute("multiple");
    this.syncStringAttribute("aria-label");
    this.syncStringAttribute("name");

    if (this.hasAttribute("progress-max")) {
      this.progressBar.max = Number(this.getAttribute("progress-max")) || 100;
    }
    if (this.hasAttribute("progress-value")) {
      this.progressBar.value = Number(this.getAttribute("progress-value")) || 0;
    }
    if (this.hasAttribute("progress-message")) {
      this.progressMessage.textContent = this.getAttribute("progress-message") || "";
    }

    this.labelElement.htmlFor = inputId;
    this.labelElement.textContent = label;
    this.labelElement.hidden = label.length === 0;
    this.helperText.textContent = warningText;
    this.helperText.hidden = warningText.length === 0;
  }

  private syncStringAttribute(name: string) {
    const value = this.getAttribute(name);
    if (value === null) {
      this.input.removeAttribute(name);
      return;
    }

    this.input.setAttribute(name, value);
  }
}

export function defineFileUpload() {
  if (customElements.get(FILE_UPLOAD_TAG)) {
    return;
  }

  customElements.define(FILE_UPLOAD_TAG, FileUpload);
}
