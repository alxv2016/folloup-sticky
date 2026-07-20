import badgeStyles from "./Badge.css?raw";

const BADGE_TAG = "ui-badge";
const BADGE_TEMPLATE = document.createElement("template");

BADGE_TEMPLATE.innerHTML = `
  <style>${badgeStyles}</style>
  <span class="badge">
    <slot></slot>
  </span>
`;

export class Badge extends HTMLElement {
  constructor() {
    super();
    const shadow = this.attachShadow({ mode: "open" });
    shadow.append(BADGE_TEMPLATE.content.cloneNode(true));
  }

  connectedCallback() {
    this.dataset.component = BADGE_TAG;
  }
}

export function defineBadge() {
  if (customElements.get(BADGE_TAG)) {
    return;
  }

  customElements.define(BADGE_TAG, Badge);
}
