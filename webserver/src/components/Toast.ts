import toastStyles from "./Toast.css?raw";

const TOAST_TAG = "ui-toast";
const TOAST_TEMPLATE = document.createElement("template");

type ToastStatus = "success" | "info" | "warning" | "error" | "danger";

TOAST_TEMPLATE.innerHTML = `
  <style>${toastStyles}</style>
  <dialog class="toast-dialog" aria-live="polite">
    <p class="toast-dialog__message"></p>
  </dialog>
`;

function normalizeStatus(status: string | null): ToastStatus | null {
  if (
    status === "success" ||
    status === "info" ||
    status === "warning" ||
    status === "error" ||
    status === "danger"
  ) {
    return status;
  }

  return null;
}

export class Toast extends HTMLElement {
  static get observedAttributes() {
    return ["aria-label", "message", "status"];
  }

  private readonly dialog: HTMLDialogElement;
  private readonly messageElement: HTMLParagraphElement;

  constructor() {
    super();
    const shadow = this.attachShadow({ mode: "open" });
    shadow.append(TOAST_TEMPLATE.content.cloneNode(true));
    this.dialog = shadow.querySelector("dialog")!;
    this.messageElement = shadow.querySelector(".toast-dialog__message")!;
  }

  connectedCallback() {
    this.dataset.component = TOAST_TAG;
    this.syncAttributes();
  }

  attributeChangedCallback() {
    this.syncAttributes();
  }

  get message() {
    return this.messageElement.textContent || "";
  }

  set message(message: string) {
    this.setAttribute("message", message);
  }

  get status(): ToastStatus | null {
    return normalizeStatus(this.getAttribute("status"));
  }

  set status(status: ToastStatus | null) {
    if (status) {
      this.setAttribute("status", status);
      return;
    }

    this.removeAttribute("status");
  }

  show(message?: string, status?: ToastStatus) {
    if (typeof message === "string") {
      this.message = message;
    }
    if (status) {
      this.status = status;
    }

    if (!this.dialog.open) {
      this.dialog.show();
    }
  }

  close(returnValue?: string) {
    this.dialog.close(returnValue);
  }

  private syncAttributes() {
    const ariaLabel = this.getAttribute("aria-label");
    const message = this.getAttribute("message") || "";
    const status = normalizeStatus(this.getAttribute("status"));

    if (ariaLabel) {
      this.dialog.setAttribute("aria-label", ariaLabel);
    } else {
      this.dialog.removeAttribute("aria-label");
    }

    this.messageElement.textContent = message;
    this.messageElement.id = this.id ? `${this.id}Message` : "statusToastMessage";
    this.dialog.id = this.id || "statusToast";

    if (status) {
      this.dialog.setAttribute("data-status", status);
    } else {
      this.dialog.removeAttribute("data-status");
    }
  }
}

export function defineToast() {
  if (customElements.get(TOAST_TAG)) {
    return;
  }

  customElements.define(TOAST_TAG, Toast);
}
