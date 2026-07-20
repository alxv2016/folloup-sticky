import cardStyles from "./Card.css?raw";

const CARD_TAG = "ui-card";
type CardStatus = "info" | "success" | "warning" | "error" | "danger";
const CARD_TEMPLATE = document.createElement("template");

CARD_TEMPLATE.innerHTML = `
  <style>${cardStyles}</style>
  <div class="card-component">
    <div class="card-component__container">
      <div class="card-component__header">
        <span class="card-icon">
          <span class="card-component__icon"></span>
        </span>
        <h2 class="card-component__title"></h2>
      </div>
      <div
        class="connection-notification"
        role="status"
        aria-live="polite"
        aria-atomic="true"
        hidden
      ></div>
      <div class="card-component__content">
        <slot></slot>
      </div>
      <div class="card-actions wifi-connection-form__actions card-component__footer">
        <slot name="footer"></slot>
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

export class Card extends HTMLElement {
  static get observedAttributes() {
    return ["title"];
  }

  private readonly footerContainer: HTMLDivElement;
  private readonly footerSlot: HTMLSlotElement;
  private readonly iconElement: HTMLSpanElement;
  private readonly notificationElement: HTMLDivElement;
  private readonly titleElement: HTMLHeadingElement;

  constructor() {
    super();
    const shadow = this.attachShadow({ mode: "open" });
    shadow.append(CARD_TEMPLATE.content.cloneNode(true));

    this.footerContainer = shadow.querySelector(".card-component__footer")!;
    this.footerSlot = shadow.querySelector('slot[name="footer"]')!;
    this.footerSlot.addEventListener("slotchange", () => {
      this.syncFooterVisibility();
    });
    this.iconElement = shadow.querySelector(".card-component__icon")!;
    this.notificationElement = shadow.querySelector(".connection-notification")!;
    this.titleElement = shadow.querySelector(".card-component__title")!;
  }

  connectedCallback() {
    this.dataset.component = CARD_TAG;
    this.syncAttributes();
    this.syncFooterVisibility();
  }

  attributeChangedCallback() {
    this.syncAttributes();
  }

  get iconSvg() {
    return this.iconElement.innerHTML;
  }

  set iconSvg(svg: string) {
    this.iconElement.innerHTML = svg ? svgWithClass(svg, "card-component__icon") : "";
  }

  setNotification(message: string, type: CardStatus = "info") {
    if (!message) {
      this.notificationElement.hidden = true;
      this.notificationElement.textContent = "";
      this.notificationElement.removeAttribute("data-status");
      this.notificationElement.setAttribute("role", "status");
      this.notificationElement.setAttribute("aria-live", "polite");
      return;
    }

    this.notificationElement.hidden = false;
    this.notificationElement.textContent = message;
    this.notificationElement.setAttribute("data-status", type);

    if (type === "error" || type === "danger") {
      this.notificationElement.setAttribute("role", "alert");
      this.notificationElement.setAttribute("aria-live", "assertive");
    } else if (type === "warning") {
      this.notificationElement.setAttribute("role", "alert");
      this.notificationElement.setAttribute("aria-live", "polite");
    } else {
      this.notificationElement.setAttribute("role", "status");
      this.notificationElement.setAttribute("aria-live", "polite");
    }
  }

  private syncAttributes() {
    this.titleElement.textContent = this.getAttribute("title") || "";
  }

  private syncFooterVisibility() {
    const hasFooterContent = this.footerSlot.assignedElements({ flatten: true }).length > 0;
    this.footerContainer.hidden = !hasFooterContent;
  }
}

export function defineCard() {
  if (customElements.get(CARD_TAG)) {
    return;
  }

  customElements.define(CARD_TAG, Card);
}
