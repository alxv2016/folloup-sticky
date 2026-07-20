import bottomSheetStyles from "./BottomSheet.css?raw";

const BOTTOM_SHEET_TAG = "ui-bottom-sheet";
const BOTTOM_SHEET_TEMPLATE = document.createElement("template");
const FOCUSABLE_SELECTOR = [
  "a[href]",
  "button:not([disabled])",
  "details summary",
  "input:not([disabled])",
  "select:not([disabled])",
  "textarea:not([disabled])",
  '[tabindex]:not([tabindex="-1"])',
].join(", ");

let bottomSheetInstanceCount = 0;

BOTTOM_SHEET_TEMPLATE.innerHTML = `
  <style>${bottomSheetStyles}</style>
  <dialog class="bottom-sheet-dialog" part="dialog">
    <div class="bottom-sheet-content" part="content">
      <div class="bottom-sheet-header" part="header">
        <div class="bottom-sheet-header-copy">
          <h2 class="bottom-sheet-title" part="title"></h2>
          <p class="bottom-sheet-description" part="description"></p>
        </div>
        <div class="bottom-sheet-header-actions" part="header-actions">
          <slot name="header-actions"></slot>
        </div>
      </div>
      <div class="bottom-sheet-body" part="body">
        <slot></slot>
      </div>
      <div class="bottom-sheet-footer" part="footer">
        <slot name="footer"></slot>
      </div>
    </div>
    <div class="bottom-sheet-floating" part="floating-content">
      <slot name="floating-content"></slot>
    </div>
  </dialog>
`;

function isVisibleElement(element: HTMLElement): boolean {
  const style = getComputedStyle(element);

  if (
    element.hasAttribute("hidden") ||
    element.getAttribute("aria-hidden") === "true" ||
    style.display === "none" ||
    style.visibility === "hidden"
  ) {
    return false;
  }

  if (element.tagName.includes("-")) {
    return style.display === "contents" || !!element.getClientRects().length;
  }

  return element.offsetParent !== null || style.position === "fixed" || !!element.getClientRects().length;
}

function getFocusableElements(host: HTMLElement): HTMLElement[] {
  if (!host.shadowRoot) {
    return [];
  }

  const results: HTMLElement[] = [];
  const seen = new Set<HTMLElement>();

  function visit(node: Node) {
    if (node instanceof HTMLSlotElement) {
      const assigned = node.assignedElements({ flatten: true });
      if (assigned.length > 0) {
        assigned.forEach(visit);
      } else {
        Array.from(node.children).forEach(visit);
      }
      return;
    }

    if (!(node instanceof HTMLElement)) {
      return;
    }

    if (!isVisibleElement(node)) {
      return;
    }

    // If the element has an open shadow root, always pierce into it.
    // This handles custom elements like <ui-input> that set tabIndex=-1 on
    // the host but have multiple focusable elements inside (e.g. input +
    // password toggle button). We collect the shadow children directly rather
    // than treating the host as an atomic tab stop.
    if (node.shadowRoot) {
      Array.from(node.shadowRoot.childNodes).forEach(visit);
      return;
    }

    // Plain element — add it if it's a tab stop.
    if (!seen.has(node) && node.matches(FOCUSABLE_SELECTOR) && isVisibleElement(node)) {
      seen.add(node);
      results.push(node);
    }

    Array.from(node.children).forEach(visit);
  }

  Array.from(host.shadowRoot.childNodes).forEach(visit);
  return results;
}

function isTargetWithinElement(target: HTMLElement, candidate: HTMLElement): boolean {
  if (candidate === target || candidate.contains(target)) {
    return true;
  }

  let node: Node = target;
  while (node) {
    if (node === candidate) {
      return true;
    }

    const root = node.getRootNode();
    if (!(root instanceof ShadowRoot)) {
      break;
    }

    node = root.host;
  }

  return false;
}

function hasAssignedContent(slot: HTMLSlotElement): boolean {
  return slot.assignedNodes({ flatten: true }).some(node => {
    if (node.nodeType === Node.TEXT_NODE) {
      return node.textContent?.trim().length;
    }

    return node.nodeType === Node.ELEMENT_NODE;
  });
}

export class BottomSheet extends HTMLElement {
  static get observedAttributes() {
    return ["description", "open", "title"];
  }

  private isAnimatingClose = false;
  private isSyncingOpenAttribute = false;
  private previousDocumentOverflow = "";
  private previousBodyOverflow = "";
  private triggerElement: HTMLElement | null = null;
  private readonly descriptionElement: HTMLParagraphElement;
  private readonly dialog: HTMLDialogElement;
  private readonly floatingContainer: HTMLDivElement;
  private readonly floatingSlot: HTMLSlotElement;
  private readonly footerContainer: HTMLDivElement;
  private readonly footerSlot: HTMLSlotElement;
  private readonly headerActionsContainer: HTMLDivElement;
  private readonly headerActionsSlot: HTMLSlotElement;
  private readonly titleElement: HTMLHeadingElement;
  private readonly titleId: string;
  private readonly descriptionId: string;
  private readonly handleAnimationEndBound: (event: AnimationEvent) => void;
  private readonly handleCancelBound: (event: Event) => void;
  private readonly handleClickBound: (event: MouseEvent) => void;
  private readonly handleKeyDownBound: (event: KeyboardEvent) => void;
  private readonly handleSlotChangeBound: () => void;

  constructor() {
    super();
    const shadow = this.attachShadow({ mode: "open" });
    shadow.append(BOTTOM_SHEET_TEMPLATE.content.cloneNode(true));

    this.dialog = shadow.querySelector(".bottom-sheet-dialog")!;
    this.titleElement = shadow.querySelector(".bottom-sheet-title")!;
    this.descriptionElement = shadow.querySelector(".bottom-sheet-description")!;
    this.headerActionsContainer = shadow.querySelector(".bottom-sheet-header-actions")!;
    this.footerContainer = shadow.querySelector(".bottom-sheet-footer")!;
    this.floatingContainer = shadow.querySelector(".bottom-sheet-floating")!;
    this.headerActionsSlot = shadow.querySelector('slot[name="header-actions"]')!;
    this.footerSlot = shadow.querySelector('slot[name="footer"]')!;
    this.floatingSlot = shadow.querySelector('slot[name="floating-content"]')!;

    bottomSheetInstanceCount += 1;
    this.titleId = `bottomSheetTitle${bottomSheetInstanceCount}`;
    this.descriptionId = `bottomSheetDescription${bottomSheetInstanceCount}`;

    this.handleAnimationEndBound = this.handleAnimationEnd.bind(this);
    this.handleCancelBound = this.handleCancel.bind(this);
    this.handleClickBound = this.handleClick.bind(this);
    this.handleKeyDownBound = this.handleKeyDown.bind(this);
    this.handleSlotChangeBound = this.syncSlotVisibility.bind(this);
  }

  connectedCallback() {
    this.dataset.component = BOTTOM_SHEET_TAG;

    this.dialog.addEventListener("animationend", this.handleAnimationEndBound);
    this.dialog.addEventListener("cancel", this.handleCancelBound);
    this.dialog.addEventListener("click", this.handleClickBound);
    this.dialog.addEventListener("keydown", this.handleKeyDownBound); // catches keys from shadow DOM content
    this.addEventListener("keydown", this.handleKeyDownBound);        // catches keys from slotted light DOM content
    this.headerActionsSlot.addEventListener("slotchange", this.handleSlotChangeBound);
    this.footerSlot.addEventListener("slotchange", this.handleSlotChangeBound);
    this.floatingSlot.addEventListener("slotchange", this.handleSlotChangeBound);

    this.syncAttributes();
    this.syncSlotVisibility();

    if (this.hasAttribute("open")) {
      this.openInternal();
    }
  }

  disconnectedCallback() {
    this.dialog.removeEventListener("animationend", this.handleAnimationEndBound);
    this.dialog.removeEventListener("cancel", this.handleCancelBound);
    this.dialog.removeEventListener("click", this.handleClickBound);
    this.dialog.removeEventListener("keydown", this.handleKeyDownBound);
    this.removeEventListener("keydown", this.handleKeyDownBound);
    this.headerActionsSlot.removeEventListener("slotchange", this.handleSlotChangeBound);
    this.footerSlot.removeEventListener("slotchange", this.handleSlotChangeBound);
    this.floatingSlot.removeEventListener("slotchange", this.handleSlotChangeBound);

    if (this.dialog.open) {
      this.dialog.close();
    }

    this.unlockDocumentScroll();
  }

  attributeChangedCallback(name: string) {
    if (name === "open") {
      if (!this.isConnected) {
        return;
      }

      if (this.isSyncingOpenAttribute) {
        return;
      }

      if (this.hasAttribute("open")) {
        this.openInternal();
      } else {
        this.startClosing();
      }

      return;
    }

    this.syncAttributes();
  }

  get open() {
    return this.hasAttribute("open");
  }

  set open(isOpen: boolean) {
    this.toggleAttribute("open", isOpen);
  }

  openBottomSheet() {
    if (this.open && !this.isAnimatingClose) {
      return;
    }

    this.triggerElement =
      document.activeElement instanceof HTMLElement ? document.activeElement : null;
    this.open = true;
  }

  closeBottomSheet() {
    if (!this.open && !this.dialog.open) {
      return;
    }

    this.open = false;
  }

  focus(options?: FocusOptions) {
    const focusableElements = getFocusableElements(this);
    const nextTarget = focusableElements[0] ?? this.dialog;
    nextTarget.focus(options);
  }

  private syncAttributes() {
    const title = this.getAttribute("title") || "";
    const description = this.getAttribute("description") || "";

    this.titleElement.id = this.titleId;
    this.titleElement.textContent = title;
    this.titleElement.hidden = title.length === 0;

    this.descriptionElement.id = this.descriptionId;
    this.descriptionElement.textContent = description;
    this.descriptionElement.hidden = description.length === 0;

    if (title.length > 0) {
      this.dialog.setAttribute("aria-labelledby", this.titleId);
    } else {
      this.dialog.removeAttribute("aria-labelledby");
    }

    if (description.length > 0) {
      this.dialog.setAttribute("aria-describedby", this.descriptionId);
    } else {
      this.dialog.removeAttribute("aria-describedby");
    }
  }

  private syncSlotVisibility() {
    this.headerActionsContainer.hidden = !hasAssignedContent(this.headerActionsSlot);
    this.footerContainer.hidden = !hasAssignedContent(this.footerSlot);
    this.floatingContainer.hidden = !hasAssignedContent(this.floatingSlot);
  }

  private handleAnimationEnd(event: AnimationEvent) {
    if (!this.isAnimatingClose || event.target !== this.dialog) {
      return;
    }

    this.dialog.classList.remove("closing");
    this.dialog.close();
    this.isAnimatingClose = false;
    this.unlockDocumentScroll();
    this.restoreTriggerFocus();
    this.dispatchEvent(new Event("close", { bubbles: true, composed: true }));
  }

  private handleCancel(event: Event) {
    event.preventDefault();
    this.closeBottomSheet();
  }

  private handleClick(event: MouseEvent) {
    if (this.isAnimatingClose) {
      return;
    }

    if (event.target === this.dialog) {
      this.closeBottomSheet();
    }
  }

  private handleKeyDown(event: KeyboardEvent) {
    // Prevent double-firing when the event bubbles through both
    // this.dialog (shadow DOM) and this (host/light DOM)
    if (event.currentTarget === this && event.composedPath().includes(this.dialog)) {
      return;
    }

    if (event.key === "Escape") {
      event.preventDefault();
      this.closeBottomSheet();
      return;
    }

    if (event.key !== "Tab") {
      return;
    }

    const focusableElements = getFocusableElements(this);
    if (focusableElements.length === 0) {
      event.preventDefault();
      this.dialog.focus();
      return;
    }

    const eventTarget = event.composedPath()[0];
    const activeElement =
      eventTarget instanceof HTMLElement
        ? eventTarget
        : document.activeElement instanceof HTMLElement
          ? document.activeElement
          : null;
    const currentIndex =
      activeElement === null
        ? -1
        : focusableElements.findIndex(element => isTargetWithinElement(activeElement, element));
    const direction = event.shiftKey ? -1 : 1;
    const fallbackIndex = event.shiftKey ? focusableElements.length - 1 : 0;
    const nextIndex =
      currentIndex === -1
        ? fallbackIndex
        : (currentIndex + direction + focusableElements.length) % focusableElements.length;

    event.preventDefault();
    focusableElements[nextIndex].focus();
  }

  private openInternal() {
    this.syncAttributes();
    this.syncSlotVisibility();

    if (this.isAnimatingClose) {
      this.dialog.classList.remove("closing");
      this.isAnimatingClose = false;
    }

    if (!this.dialog.open) {
      this.lockDocumentScroll();
      this.dialog.showModal();
    } else {
      this.lockDocumentScroll();
    }

    requestAnimationFrame(() => {
      if (!this.dialog.open) {
        return;
      }

      const focusableElements = getFocusableElements(this);
      const nextTarget = focusableElements[0] ?? this.dialog;
      nextTarget.focus();
    });
  }

  private startClosing() {
    if (!this.dialog.open || this.isAnimatingClose) {
      return;
    }

    this.dialog.classList.add("closing");
    this.isAnimatingClose = true;
  }

  private lockDocumentScroll() {
    // Lock both <html> and <body>: which one is the scroll container varies by browser (and on
    // touch, html-only overflow:hidden can still let the page rubber-band behind the sheet).
    if (this.previousDocumentOverflow.length === 0 && this.previousBodyOverflow.length === 0) {
      this.previousDocumentOverflow = document.documentElement.style.overflow;
      this.previousBodyOverflow = document.body.style.overflow;
    }

    document.documentElement.style.overflow = "hidden";
    document.body.style.overflow = "hidden";
  }

  private unlockDocumentScroll() {
    document.documentElement.style.overflow = this.previousDocumentOverflow;
    document.body.style.overflow = this.previousBodyOverflow;
    this.previousDocumentOverflow = "";
    this.previousBodyOverflow = "";
  }

  private restoreTriggerFocus() {
    if (this.triggerElement && document.contains(this.triggerElement)) {
      this.triggerElement.focus();
    }

    this.triggerElement = null;
  }
}

export function defineBottomSheet() {
  if (customElements.get(BOTTOM_SHEET_TAG)) {
    return;
  }

  customElements.define(BOTTOM_SHEET_TAG, BottomSheet);
}
