import networkListStyles from "./NetworkList.css?raw";

const NETWORK_LIST_TAG = "ui-network-list";
const NETWORK_LIST_ID = "networkList";
const NETWORK_LIST_LABEL_ID = "network-list-label";
const NETWORK_LIST_TEMPLATE = document.createElement("template");

NETWORK_LIST_TEMPLATE.innerHTML = `
  <style>${networkListStyles}</style>
  <div class="networks">
    <span id="${NETWORK_LIST_LABEL_ID}" hidden>Available networks</span>
    <ul
      id="${NETWORK_LIST_ID}"
      class="networks__list"
      role="listbox"
      aria-labelledby="${NETWORK_LIST_LABEL_ID}"
      tabindex="0"
    ></ul>
  </div>
`;

export class NetworkList extends HTMLElement {
  static get observedAttributes() {
    return ["aria-label", "label"];
  }

  private readonly listElement: HTMLUListElement;
  private readonly labelElement: HTMLSpanElement;

  constructor() {
    super();
    const shadow = this.attachShadow({ mode: "open", delegatesFocus: true });
    shadow.append(NETWORK_LIST_TEMPLATE.content.cloneNode(true));

    this.labelElement = shadow.querySelector(`#${NETWORK_LIST_LABEL_ID}`)!;
    this.listElement = shadow.querySelector(`#${NETWORK_LIST_ID}`)!;
    this.listElement.addEventListener("keydown", event => {
      if (
        event.key === "ArrowDown" ||
        event.key === "ArrowUp" ||
        event.key === "Home" ||
        event.key === "End" ||
        event.key === " "
      ) {
        event.preventDefault();
      }
    });
  }

  connectedCallback() {
    this.tabIndex = 0;
    this.dataset.component = NETWORK_LIST_TAG;
    this.syncAttributes();
  }

  attributeChangedCallback() {
    this.syncAttributes();
  }

  appendItem(item: HTMLElement) {
    this.listElement.appendChild(item);
  }

  clearItems() {
    this.listElement.innerHTML = "";
  }

  focus(options?: FocusOptions) {
    this.listElement.focus(options);
  }

  getOptions() {
    return Array.from(this.listElement.querySelectorAll<HTMLElement>('[role="option"]'));
  }

  scrollOptionIntoView(index: number) {
    const option = this.getOptions()[index];
    if (!option) {
      return;
    }

    const listTop = this.listElement.scrollTop;
    const listBottom = listTop + this.listElement.clientHeight;
    const optionTop = option.offsetTop;
    const optionBottom = optionTop + option.offsetHeight;

    if (optionTop < listTop) {
      this.listElement.scrollTop = optionTop;
      return;
    }

    if (optionBottom > listBottom) {
      this.listElement.scrollTop = optionBottom - this.listElement.clientHeight;
    }
  }

  setActiveDescendant(value: string | null) {
    if (value) {
      this.listElement.setAttribute("aria-activedescendant", value);
      return;
    }

    this.listElement.removeAttribute("aria-activedescendant");
  }

  setEmptyState(item: HTMLElement) {
    this.clearItems();
    this.appendItem(item);
  }

  private syncAttributes() {
    const label = this.getAttribute("label") || "Available networks";
    const ariaLabel = this.getAttribute("aria-label");

    this.labelElement.textContent = label;

    if (ariaLabel) {
      this.listElement.setAttribute("aria-label", ariaLabel);
    } else {
      this.listElement.removeAttribute("aria-label");
    }
  }
}

export function defineNetworkList() {
  if (customElements.get(NETWORK_LIST_TAG)) {
    return;
  }

  customElements.define(NETWORK_LIST_TAG, NetworkList);
}
