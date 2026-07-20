(function(){const e=document.createElement("link").relList;if(e&&e.supports&&e.supports("modulepreload"))return;for(const o of document.querySelectorAll('link[rel="modulepreload"]'))i(o);new MutationObserver(o=>{for(const a of o)if(a.type==="childList")for(const l of a.addedNodes)l.tagName==="LINK"&&l.rel==="modulepreload"&&i(l)}).observe(document,{childList:!0,subtree:!0});function n(o){const a={};return o.integrity&&(a.integrity=o.integrity),o.referrerPolicy&&(a.referrerPolicy=o.referrerPolicy),o.crossOrigin==="use-credentials"?a.credentials="include":o.crossOrigin==="anonymous"?a.credentials="omit":a.credentials="same-origin",a}function i(o){if(o.ep)return;o.ep=!0;const a=n(o);fetch(o.href,a)}})();const Ot={mode:"absolute",position:"bottom",height:"6rem",strength:2,divCount:5,exponential:!1,opacity:1,curve:"linear",zIndex:2},ot={linear:t=>t,bezier:t=>t*t*(3-2*t),"ease-in":t=>t*t,"ease-out":t=>1-Math.pow(1-t,2),"ease-in-out":t=>t<.5?2*t*t:1-Math.pow(-2*t+2,2)/2};function Nt(t){switch(t){case"top":return"to top";case"left":return"to left";case"right":return"to right";default:return"to bottom"}}function Vt(){if(typeof CSS>"u"||typeof CSS.supports!="function")return!1;const t=CSS.supports("backdrop-filter","blur(1px)")||CSS.supports("-webkit-backdrop-filter","blur(1px)"),e=CSS.supports("mask-image","linear-gradient(to bottom, transparent, black)")||CSS.supports("-webkit-mask-image","linear-gradient(to bottom, transparent, black)");return t&&e}function Dt(t){const e={...Ot,...t},n=document.createElement("div"),i=document.createElement("div"),o=Nt(e.position),a=Vt(),l=e.position==="top"||e.position==="bottom";if(n.className="gradual-blur",e.className&&n.classList.add(e.className),a||(n.classList.add("gradual-blur--fallback"),n.style.background=`linear-gradient(${o}, transparent 0%, ${e.fallbackColor||"rgba(255, 255, 255, 0.95)"} 100%)`),n.style.position=e.mode,n.style.pointerEvents="none",n.style.zIndex=String(e.zIndex),n.style.opacity="0",n.style.borderRadius="inherit",l?(n.style.height=e.height,n.style.width=e.width||"100%",n.style[e.position]="0",e.mode==="fixed"?(n.style.left="50%",n.style.right="auto",n.style.transform="translateX(-50%)"):(n.style.left="0",n.style.right="0")):(n.style.width=e.width||e.height,n.style.height="100%",n.style.top="0",n.style.bottom="0",n.style[e.position]="0"),i.className="gradual-blur__inner",n.appendChild(i),a){const d=100/e.divCount,c=ot[e.curve]||ot.linear;for(let f=1;f<=e.divCount;f+=1){const p=document.createElement("div"),k=c(f/e.divCount);let S=0;e.exponential?S=Math.pow(2,k*4)*.0625*e.strength:S=.0625*(k*e.divCount+1)*e.strength;const v=Math.round((d*f-d)*10)/10,y=Math.round(d*f*10)/10,T=Math.round((d*f+d)*10)/10,E=Math.round((d*f+d*2)*10)/10;let s=`transparent ${v}%, black ${y}%`;T<=100&&(s+=`, black ${T}%`),E<=100&&(s+=`, transparent ${E}%`),p.className="gradual-blur__layer",p.style.maskImage=`linear-gradient(${o}, ${s})`,p.style.webkitMaskImage=`linear-gradient(${o}, ${s})`,p.style.backdropFilter=`blur(${S.toFixed(3)}rem)`,p.style.setProperty("-webkit-backdrop-filter",`blur(${S.toFixed(3)}rem)`),p.style.opacity=String(e.opacity),i.appendChild(p)}}return window.getComputedStyle(e.mount).position==="static"&&(e.mount.style.position="relative"),e.mount.appendChild(n),{element:n,setVisible(d){n.style.opacity=d?"1":"0"},destroy(){n.remove()}}}const Ht=`:host {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  min-height: 1.75rem;
  padding: 0 var(--spacing-sm);
  border: var(--border-01) solid var(--color-border);
  border-radius: var(--radius-full);
  background-color: var(--color-bg-selected);
  color: var(--color-text-secondary);
  font-size: var(--font-size-caption);
  font-weight: var(--font-weight-semibold);
  line-height: 1;
  white-space: nowrap;
}

*,
*::before,
*::after {
  box-sizing: border-box;
}

.badge {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  white-space: inherit;
}
`,$t=document.createElement("template");$t.innerHTML=`
  <style>${Ht}</style>
  <span class="badge">
    <slot></slot>
  </span>
`;const Kt=`:host {
  display: inline-flex;
  height: 44px;
  width: auto;
  min-width: 96px;
  vertical-align: middle;
}

*,
*::before,
*::after {
  box-sizing: border-box;
}

button {
  all: unset;
  box-sizing: border-box;
  position: relative;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  height: 44px;
  width: auto;
  min-height: 44px;
  min-width: 96px;
  padding: var(--spacing-3xs) var(--spacing-sm);
  border: var(--border-01) solid transparent;
  border-radius: var(--button-radius);
  background-color: var(--color-text-secondary);
  color: var(--color-text-inverse);
  cursor: pointer;
  font-family: var(--font-family);
  font-size: var(--font-size-label);
  font-weight: var(--font-weight-regular);
  line-height: 1.25;
  text-align: center;
}

button:focus-visible {
  outline: var(--focus-ring-width) solid var(--focus-ring-color);
  outline-offset: var(--focus-ring-offset);
}

button:disabled {
  background-color: var(--color-bg-mute);
  color: var(--color-text-mute);
  cursor: default;
}

:host([variant="primary"]) button {
  background-color: var(--color-bg-brand);
  color: var(--color-text-inverse);
}

:host([variant="primary"]) button:not(:disabled):is(:hover, :active) {
  background-color: var(--color-bg-brand-hover);
}

:host([variant="secondary"]) button {
  background-color: var(--color-bg-active);
  color: var(--color-fg-active);
}

:host([variant="secondary"]) button:not(:disabled):is(:hover, :active) {
  background-color: var(--color-bg-active);
  color: var(--color-fg-active);
}

:host([variant="outline"]) button {
  background-color: transparent;
  border-color: var(--color-border);
  color: var(--color-text-secondary);
}

:host([variant="outline"]) button:not(:disabled):is(:hover, :active) {
  background-color: var(--color-border);
  border-color: var(--color-border);
}

:host([variant="ghost"]) button {
  background-color: transparent;
  border-color: transparent;
  color: var(--color-text-secondary);
}

:host([variant="ghost"]) button:not(:disabled):is(:hover, :active) {
  background-color: var(--color-overlay);
}

:host([inverse]) button {
  color: var(--color-text-inverse);
}

:host([variant="outline"][inverse]) button {
  border-color: var(--color-text-inverse);
}

:host([variant="outline"][inverse]) button:not(:disabled):is(:hover, :active),
:host([variant="ghost"][inverse]) button:not(:disabled):is(:hover, :active) {
  background-color: var(--color-overlay);
  border-color: var(--color-text-inverse);
}

:host([rounded]) button {
  border-radius: var(--radius-full);
  padding: var(--spacing-xs) var(--spacing-lg);
}

::slotted(svg) {
  flex-shrink: 0;
}
`,F="ui-button",gt=document.createElement("template");gt.innerHTML=`
  <style>${Kt}</style>
  <button type="button" part="button">
    <slot></slot>
  </button>
`;function rt(t){return t==="submit"||t==="reset"?t:"button"}class Ft extends HTMLElement{static get observedAttributes(){return["disabled","aria-label","type"]}constructor(){super();const e=this.attachShadow({mode:"open",delegatesFocus:!0});e.append(gt.content.cloneNode(!0)),this.button=e.querySelector("button")}connectedCallback(){this.dataset.component=F,this.syncAttributes()}attributeChangedCallback(){this.syncAttributes()}get disabled(){return this.hasAttribute("disabled")}set disabled(e){this.toggleAttribute("disabled",e)}get type(){return rt(this.getAttribute("type"))}set type(e){this.setAttribute("type",rt(e))}focus(e){this.button.focus(e)}click(){this.button.click()}syncAttributes(){this.button.disabled=this.disabled,this.button.type=this.type,this.tabIndex=this.disabled?-1:0;const e=this.getAttribute("aria-label");e?this.button.setAttribute("aria-label",e):this.button.removeAttribute("aria-label")}}function Zt(){customElements.get(F)||customElements.define(F,Ft)}const Gt=`:host {
  display: contents;
}

:host([hidden]) {
  display: none;
}

*,
*::before,
*::after {
  box-sizing: border-box;
}

.bottom-sheet-dialog {
  position: fixed;
  inset: auto 0 0 0;
  display: flex;
  width: 100%;
  max-height: 96vh;
  padding: 0;
  border: 0;
  background: transparent;
  color: var(--color-text);
  overflow: visible;
  transform: translateY(100%);
  max-width: 768px;
  margin: auto;
}

.bottom-sheet-dialog:not([open]) {
  pointer-events: none;
  visibility: hidden;
}

.bottom-sheet-dialog[open] {
  animation: bottom-sheet-open 320ms ease forwards;
}

.bottom-sheet-dialog.closing {
  animation: bottom-sheet-close 320ms ease forwards;
}

.bottom-sheet-dialog::backdrop {
  background: var(--color-overlay);
  backdrop-filter: blur(4px) saturate(180%);
  -webkit-backdrop-filter: blur(4px) saturate(180%);
}

.bottom-sheet-content {
  display: flex;
  flex-direction: column;
  width: 100%;
  min-height: 0;
  border-top: var(--border-01) solid var(--color-border);
  border-radius: var(--radius-xl) var(--radius-xl) 0 0;
  background: var(--color-bg);
  box-shadow: 0 -8px 24px rgb(0 0 0 / 0.16);
  overflow: hidden;
}

.bottom-sheet-header {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: var(--spacing-sm);
  padding: var(--spacing-sm) var(--spacing-md);
  padding-right: var(--spacing-sm);
  border-bottom: var(--border-01) solid var(--color-border);
}

.bottom-sheet-header-copy {
  display: grid;
  gap: var(--spacing-3xs);
  min-width: 0;
}

.bottom-sheet-title,
.bottom-sheet-description {
  margin: 0;
}

.bottom-sheet-title {
  font-size: var(--font-size-heading);
  font-weight: var(--font-weight-semibold);
  line-height: 1.2;
}

.bottom-sheet-description {
  color: var(--color-text-secondary);
  font-size: var(--font-size-label);
  line-height: 1.5;
}

.bottom-sheet-header-actions {
  display: flex;
  align-items: center;
  justify-content: flex-end;
  gap: var(--spacing-xs);
  flex-shrink: 0;
}

.bottom-sheet-body {
  display: flex;
  flex-direction: column;
  gap: var(--spacing-sm);
  min-height: 0;
  padding: var(--spacing-md);
  overflow: auto;
}

.bottom-sheet-footer {
  display: flex;
  flex-wrap: wrap;
  justify-content: flex-end;
  gap: var(--spacing-sm);
  /* Extra bottom padding so the footer buttons clear the iOS home indicator / safe area and
     aren't clipped at the bottom of the sheet. */
  padding: var(--spacing-md) var(--spacing-md) var(--spacing-6xl);
  border-top: var(--border-01) solid var(--color-border);
  background: var(--color-bg);
}

.bottom-sheet-title[hidden],
.bottom-sheet-description[hidden],
.bottom-sheet-header-actions[hidden],
.bottom-sheet-footer[hidden] {
  display: none;
}

::slotted([slot="footer"]) {
  flex-shrink: 0;
}

@keyframes bottom-sheet-open {
  from {
    transform: translateY(100%);
  }

  to {
    transform: translateY(0);
  }
}

@keyframes bottom-sheet-close {
  from {
    transform: translateY(0);
  }

  to {
    transform: translateY(100%);
  }
}
`,Z="ui-bottom-sheet",ft=document.createElement("template"),Rt=["a[href]","button:not([disabled])","details summary","input:not([disabled])","select:not([disabled])","textarea:not([disabled])",'[tabindex]:not([tabindex="-1"])'].join(", ");let H=0;ft.innerHTML=`
  <style>${Gt}</style>
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
`;function st(t){const e=getComputedStyle(t);return t.hasAttribute("hidden")||t.getAttribute("aria-hidden")==="true"||e.display==="none"||e.visibility==="hidden"?!1:t.tagName.includes("-")?e.display==="contents"||!!t.getClientRects().length:t.offsetParent!==null||e.position==="fixed"||!!t.getClientRects().length}function $(t){if(!t.shadowRoot)return[];const e=[],n=new Set;function i(o){if(o instanceof HTMLSlotElement){const a=o.assignedElements({flatten:!0});a.length>0?a.forEach(i):Array.from(o.children).forEach(i);return}if(o instanceof HTMLElement&&st(o)){if(o.shadowRoot){Array.from(o.shadowRoot.childNodes).forEach(i);return}!n.has(o)&&o.matches(Rt)&&st(o)&&(n.add(o),e.push(o)),Array.from(o.children).forEach(i)}}return Array.from(t.shadowRoot.childNodes).forEach(i),e}function Ut(t,e){if(e===t||e.contains(t))return!0;let n=t;for(;n;){if(n===e)return!0;const i=n.getRootNode();if(!(i instanceof ShadowRoot))break;n=i.host}return!1}function K(t){return t.assignedNodes({flatten:!0}).some(e=>e.nodeType===Node.TEXT_NODE?e.textContent?.trim().length:e.nodeType===Node.ELEMENT_NODE)}class Wt extends HTMLElement{constructor(){super(),this.isAnimatingClose=!1,this.isSyncingOpenAttribute=!1,this.previousDocumentOverflow="",this.previousBodyOverflow="",this.triggerElement=null;const e=this.attachShadow({mode:"open"});e.append(ft.content.cloneNode(!0)),this.dialog=e.querySelector(".bottom-sheet-dialog"),this.titleElement=e.querySelector(".bottom-sheet-title"),this.descriptionElement=e.querySelector(".bottom-sheet-description"),this.headerActionsContainer=e.querySelector(".bottom-sheet-header-actions"),this.footerContainer=e.querySelector(".bottom-sheet-footer"),this.floatingContainer=e.querySelector(".bottom-sheet-floating"),this.headerActionsSlot=e.querySelector('slot[name="header-actions"]'),this.footerSlot=e.querySelector('slot[name="footer"]'),this.floatingSlot=e.querySelector('slot[name="floating-content"]'),H+=1,this.titleId=`bottomSheetTitle${H}`,this.descriptionId=`bottomSheetDescription${H}`,this.handleAnimationEndBound=this.handleAnimationEnd.bind(this),this.handleCancelBound=this.handleCancel.bind(this),this.handleClickBound=this.handleClick.bind(this),this.handleKeyDownBound=this.handleKeyDown.bind(this),this.handleSlotChangeBound=this.syncSlotVisibility.bind(this)}static get observedAttributes(){return["description","open","title"]}connectedCallback(){this.dataset.component=Z,this.dialog.addEventListener("animationend",this.handleAnimationEndBound),this.dialog.addEventListener("cancel",this.handleCancelBound),this.dialog.addEventListener("click",this.handleClickBound),this.dialog.addEventListener("keydown",this.handleKeyDownBound),this.addEventListener("keydown",this.handleKeyDownBound),this.headerActionsSlot.addEventListener("slotchange",this.handleSlotChangeBound),this.footerSlot.addEventListener("slotchange",this.handleSlotChangeBound),this.floatingSlot.addEventListener("slotchange",this.handleSlotChangeBound),this.syncAttributes(),this.syncSlotVisibility(),this.hasAttribute("open")&&this.openInternal()}disconnectedCallback(){this.dialog.removeEventListener("animationend",this.handleAnimationEndBound),this.dialog.removeEventListener("cancel",this.handleCancelBound),this.dialog.removeEventListener("click",this.handleClickBound),this.dialog.removeEventListener("keydown",this.handleKeyDownBound),this.removeEventListener("keydown",this.handleKeyDownBound),this.headerActionsSlot.removeEventListener("slotchange",this.handleSlotChangeBound),this.footerSlot.removeEventListener("slotchange",this.handleSlotChangeBound),this.floatingSlot.removeEventListener("slotchange",this.handleSlotChangeBound),this.dialog.open&&this.dialog.close(),this.unlockDocumentScroll()}attributeChangedCallback(e){if(e==="open"){if(!this.isConnected||this.isSyncingOpenAttribute)return;this.hasAttribute("open")?this.openInternal():this.startClosing();return}this.syncAttributes()}get open(){return this.hasAttribute("open")}set open(e){this.toggleAttribute("open",e)}openBottomSheet(){this.open&&!this.isAnimatingClose||(this.triggerElement=document.activeElement instanceof HTMLElement?document.activeElement:null,this.open=!0)}closeBottomSheet(){!this.open&&!this.dialog.open||(this.open=!1)}focus(e){($(this)[0]??this.dialog).focus(e)}syncAttributes(){const e=this.getAttribute("title")||"",n=this.getAttribute("description")||"";this.titleElement.id=this.titleId,this.titleElement.textContent=e,this.titleElement.hidden=e.length===0,this.descriptionElement.id=this.descriptionId,this.descriptionElement.textContent=n,this.descriptionElement.hidden=n.length===0,e.length>0?this.dialog.setAttribute("aria-labelledby",this.titleId):this.dialog.removeAttribute("aria-labelledby"),n.length>0?this.dialog.setAttribute("aria-describedby",this.descriptionId):this.dialog.removeAttribute("aria-describedby")}syncSlotVisibility(){this.headerActionsContainer.hidden=!K(this.headerActionsSlot),this.footerContainer.hidden=!K(this.footerSlot),this.floatingContainer.hidden=!K(this.floatingSlot)}handleAnimationEnd(e){!this.isAnimatingClose||e.target!==this.dialog||(this.dialog.classList.remove("closing"),this.dialog.close(),this.isAnimatingClose=!1,this.unlockDocumentScroll(),this.restoreTriggerFocus(),this.dispatchEvent(new Event("close",{bubbles:!0,composed:!0})))}handleCancel(e){e.preventDefault(),this.closeBottomSheet()}handleClick(e){this.isAnimatingClose||e.target===this.dialog&&this.closeBottomSheet()}handleKeyDown(e){if(e.currentTarget===this&&e.composedPath().includes(this.dialog))return;if(e.key==="Escape"){e.preventDefault(),this.closeBottomSheet();return}if(e.key!=="Tab")return;const n=$(this);if(n.length===0){e.preventDefault(),this.dialog.focus();return}const i=e.composedPath()[0],o=i instanceof HTMLElement?i:document.activeElement instanceof HTMLElement?document.activeElement:null,a=o===null?-1:n.findIndex(f=>Ut(o,f)),l=e.shiftKey?-1:1,d=e.shiftKey?n.length-1:0,c=a===-1?d:(a+l+n.length)%n.length;e.preventDefault(),n[c].focus()}openInternal(){this.syncAttributes(),this.syncSlotVisibility(),this.isAnimatingClose&&(this.dialog.classList.remove("closing"),this.isAnimatingClose=!1),this.dialog.open?this.lockDocumentScroll():(this.lockDocumentScroll(),this.dialog.showModal()),requestAnimationFrame(()=>{if(!this.dialog.open)return;($(this)[0]??this.dialog).focus()})}startClosing(){!this.dialog.open||this.isAnimatingClose||(this.dialog.classList.add("closing"),this.isAnimatingClose=!0)}lockDocumentScroll(){this.previousDocumentOverflow.length===0&&this.previousBodyOverflow.length===0&&(this.previousDocumentOverflow=document.documentElement.style.overflow,this.previousBodyOverflow=document.body.style.overflow),document.documentElement.style.overflow="hidden",document.body.style.overflow="hidden"}unlockDocumentScroll(){document.documentElement.style.overflow=this.previousDocumentOverflow,document.body.style.overflow=this.previousBodyOverflow,this.previousDocumentOverflow="",this.previousBodyOverflow=""}restoreTriggerFocus(){this.triggerElement&&document.contains(this.triggerElement)&&this.triggerElement.focus(),this.triggerElement=null}}function Jt(){customElements.get(Z)||customElements.define(Z,Wt)}const jt=`:host {
  display: block;
  width: 100%;
}

:host([hidden]) {
  display: none;
  visibility: hidden;
}

*,
*::before,
*::after {
  box-sizing: border-box;
}

h2 {
  margin: 0;
}

.card-component {
  background-color: var(--color-bg-input);
  border-radius: var(--radius-lg);
  border: var(--border-01) solid var(--color-border);
  box-shadow: none;
}

.card-component__container {
  position: relative;
  display: flex;
  flex-direction: column;
  width: 100%;
  background-color: var(--color-bg);
  border-radius: inherit;
  padding: var(--spacing-lg);
  box-shadow: none;
}

.card-component__header {
  position: relative;
  display: flex;
  align-items: center;
  width: 100%;
  margin-bottom: var(--spacing-sm);
}

.card-icon {
  display: flex;
  align-items: center;
  justify-content: center;
}

.card-component__icon {
  width: 24px;
  height: 24px;
  fill: var(--color-text-secondary);
  margin-right: var(--spacing-xs);
}

.card-component__title {
  font-size: var(--font-size-heading);
  font-weight: var(--font-weight-semibold);
  margin: 0;
}

.card-component__content {
  position: relative;
  display: flex;
  flex-direction: column;
  width: 100%;
  margin-top: var(--spacing-xs);
  gap: var(--spacing-md);
}

.connection-notification {
  position: relative;
  display: flex;
  width: 100%;
  flex-direction: column;
  margin-bottom: var(--spacing-sm);
  color: var(--color-text-secondary);
  font-size: var(--font-size-label);
}

.connection-notification[hidden] {
  display: none;
}

.connection-notification[data-status="warning"] {
  color: var(--color-text-warning);
}

.connection-notification[data-status="success"] {
  color: var(--color-meter-green);
}

.connection-notification[data-status="error"],
.connection-notification[data-status="danger"] {
  color: var(--color-text-danger);
}

.card-actions {
  margin-top: var(--spacing-xl);
}

.wifi-connection-form__actions {
  position: relative;
  display: flex;
  align-items: center;
  width: 100%;
  gap: var(--spacing-sm);
}

.card-component__footer[hidden] {
  display: none;
}

::slotted(.card-component__description) {
  font-size: var(--font-size-label);
  line-height: 1.5;
  margin-bottom: var(--spacing-md);
  color: var(--color-text-secondary);
}

::slotted([slot="footer"]) {
  width: auto;
}
`,G="ui-card",mt=document.createElement("template");mt.innerHTML=`
  <style>${jt}</style>
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
`;function Yt(t,e){return t.includes("class=")?t.replace("<svg",`<svg class="${e}"`):t.replace("<svg",`<svg class="${e}"`)}class Xt extends HTMLElement{static get observedAttributes(){return["title"]}constructor(){super();const e=this.attachShadow({mode:"open"});e.append(mt.content.cloneNode(!0)),this.footerContainer=e.querySelector(".card-component__footer"),this.footerSlot=e.querySelector('slot[name="footer"]'),this.footerSlot.addEventListener("slotchange",()=>{this.syncFooterVisibility()}),this.iconElement=e.querySelector(".card-component__icon"),this.notificationElement=e.querySelector(".connection-notification"),this.titleElement=e.querySelector(".card-component__title")}connectedCallback(){this.dataset.component=G,this.syncAttributes(),this.syncFooterVisibility()}attributeChangedCallback(){this.syncAttributes()}get iconSvg(){return this.iconElement.innerHTML}set iconSvg(e){this.iconElement.innerHTML=e?Yt(e,"card-component__icon"):""}setNotification(e,n="info"){if(!e){this.notificationElement.hidden=!0,this.notificationElement.textContent="",this.notificationElement.removeAttribute("data-status"),this.notificationElement.setAttribute("role","status"),this.notificationElement.setAttribute("aria-live","polite");return}this.notificationElement.hidden=!1,this.notificationElement.textContent=e,this.notificationElement.setAttribute("data-status",n),n==="error"||n==="danger"?(this.notificationElement.setAttribute("role","alert"),this.notificationElement.setAttribute("aria-live","assertive")):n==="warning"?(this.notificationElement.setAttribute("role","alert"),this.notificationElement.setAttribute("aria-live","polite")):(this.notificationElement.setAttribute("role","status"),this.notificationElement.setAttribute("aria-live","polite"))}syncAttributes(){this.titleElement.textContent=this.getAttribute("title")||""}syncFooterVisibility(){const e=this.footerSlot.assignedElements({flatten:!0}).length>0;this.footerContainer.hidden=!e}}function Qt(){customElements.get(G)||customElements.define(G,Xt)}const te=`:host {
  display: block;
  width: 100%;
}

*,
*::before,
*::after {
  box-sizing: border-box;
}

.form-control {
  position: relative;
  display: flex;
  flex-direction: column;
  width: 100%;
}

.form-control label {
  font-size: var(--font-size-label);
  font-weight: var(--font-weight-regular);
  margin-bottom: var(--spacing-xs);
}

.form-control__input-wrapper {
  position: relative;
  display: flex;
  align-items: center;
  width: 100%;
  gap: var(--spacing-sm);
}

.duration-fields {
  align-items: center;
}

.duration-fields ui-input {
  min-width: 0;
  flex: 1 1 0;
}
`,ee=document.createElement("template");ee.innerHTML=`
  <style>${te}</style>
  <div class="form-control">
    <label></label>
    <div class="form-control__input-wrapper duration-fields">
      <ui-input variant="number" inputmode="numeric"></ui-input>
      <ui-input variant="number" inputmode="numeric"></ui-input>
    </div>
  </div>
`;const ne=`:host {
  display: block;
  width: 100%;
}

*,
*::before,
*::after {
  box-sizing: border-box;
}

input,
textarea {
  margin: 0;
  font: inherit;
  color: inherit;
  -webkit-user-select: auto;
}

p {
  margin: 0;
}

.form-control {
  position: relative;
  display: flex;
  flex-direction: column;
  width: 100%;
}

.form-control label {
  font-size: var(--font-size-label);
  font-weight: var(--font-weight-regular);
  margin-bottom: var(--spacing-xs);
}

.form-control__input-wrapper {
  position: relative;
  display: flex;
  align-items: center;
  width: 100%;
  gap: var(--spacing-sm);
}

.form-control__input-wrapper input {
  position: relative;
  display: flex;
  align-items: center;
  width: 100%;
  border: var(--border-01) solid var(--color-border);
  min-height: 44px;
  padding: var(--spacing-sm);
  border-radius: var(--input-radius);
  background-color: var(--color-bg-input);
  color: var(--color-text);
  font-size: var(--font-size-label);
}

.form-control__input-wrapper input:focus-visible {
  outline: var(--focus-ring-width) solid var(--focus-ring-color);
  outline-offset: var(--focus-ring-offset);
}

.form-control__input-wrapper input::file-selector-button {
  background-color: transparent;
  border: var(--border-01) solid var(--color-border);
  background-color: var(--color-bg);
  color: var(--color-text);
  border-radius: var(--radius-sm);
  min-height: 24px;
  font-size: var(--font-size-label);
  padding: var(--spacing-xs) var(--spacing-md);
  margin-right: var(--spacing-md);
}

.progress-bar {
  position: relative;
  width: 100%;
}

.progress-bar[hidden] {
  display: none;
}

.progress-bar progress[value] {
  -webkit-appearance: none;
  appearance: none;
  border: none;
  width: 100%;
  height: 12px;
  border-radius: var(--radius-lg);
  overflow: hidden;
  margin-top: var(--spacing-sm);
}

.progress-bar progress[value]::-webkit-progress-bar {
  background-color: var(--color-bg-active);
  border-radius: var(--radius-lg);
}

.progress-bar progress[value]::-webkit-progress-value {
  background-color: var(--color-meter-green);
  border-radius: var(--radius-lg);
}

.progress-bar__message {
  font-size: var(--font-size-label);
  line-height: 1.25;
  color: var(--color-text-secondary);
  margin-top: var(--spacing-xs);
}

.form-helper-text {
  margin-top: var(--spacing-lg);
  font-size: var(--font-size-caption);
  line-height: 1.5;
  color: var(--color-text-secondary);
}
`,ie=document.createElement("template");ie.innerHTML=`
  <style>${ne}</style>
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
`;const oe=`:host {
  display: inline-flex;
  width: 44px;
  height: 44px;
  min-width: 44px;
  vertical-align: middle;
}

*,
*::before,
*::after {
  box-sizing: border-box;
}

button {
  all: unset;
  box-sizing: border-box;
  position: relative;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 44px;
  height: 44px;
  min-width: 44px;
  min-height: 44px;
  padding: 0;
  border: var(--border-01) solid transparent;
  border-radius: var(--button-radius);
  background-color: var(--color-text-secondary);
  color: var(--color-text-inverse);
  cursor: pointer;
}

button:focus-visible {
  outline: var(--focus-ring-width) solid var(--focus-ring-color);
  outline-offset: var(--focus-ring-offset);
}

button:disabled {
  background-color: var(--color-bg-mute);
  color: var(--color-text-mute);
  cursor: default;
}

:host([variant="primary"]) button {
  background-color: var(--color-bg-brand);
  color: var(--color-text-inverse);
}

:host([variant="primary"]) button:not(:disabled):is(:hover, :active) {
  background-color: var(--color-bg-brand-hover);
}

:host([variant="secondary"]) button {
  background-color: var(--color-bg-active);
  color: var(--color-fg-active);
}

:host([variant="secondary"]) button:not(:disabled):is(:hover, :active) {
  background-color: var(--color-bg-active);
  color: var(--color-fg-active);
}

:host([variant="outline"]) button {
  background-color: transparent;
  border-color: var(--color-border);
  color: var(--color-text-secondary);
}

:host([variant="outline"]) button:not(:disabled):is(:hover, :active) {
  background-color: var(--color-border);
  border-color: var(--color-border);
}

:host([variant="ghost"]) button {
  background-color: transparent;
  border-color: transparent;
  color: var(--color-text-secondary);
}

:host([variant="ghost"]) button:not(:disabled):is(:hover, :active) {
  background-color: var(--color-overlay);
}

:host([inverse]) button {
  color: var(--color-text-inverse);
}

:host([variant="outline"][inverse]) button {
  border-color: var(--color-text-inverse);
}

:host([variant="outline"][inverse]) button:not(:disabled):is(:hover, :active),
:host([variant="ghost"][inverse]) button:not(:disabled):is(:hover, :active) {
  background-color: var(--color-overlay);
  border-color: var(--color-text-inverse);
}

:host([rounded]) button {
  border-radius: var(--radius-full);
}

::slotted(svg) {
  width: 24px;
  height: 24px;
  flex-shrink: 0;
  fill: currentColor;
}
`,R="ui-icon-button",pt=document.createElement("template");pt.innerHTML=`
  <style>${oe}</style>
  <button type="button" part="button">
    <slot></slot>
  </button>
`;function at(t){return t==="submit"||t==="reset"?t:"button"}class re extends HTMLElement{static get observedAttributes(){return["disabled","aria-label","type"]}constructor(){super();const e=this.attachShadow({mode:"open",delegatesFocus:!0});e.append(pt.content.cloneNode(!0)),this.button=e.querySelector("button")}connectedCallback(){this.dataset.component=R,this.syncAttributes()}attributeChangedCallback(){this.syncAttributes()}get disabled(){return this.hasAttribute("disabled")}set disabled(e){this.toggleAttribute("disabled",e)}get type(){return at(this.getAttribute("type"))}set type(e){this.setAttribute("type",at(e))}focus(e){this.button.focus(e)}click(){this.button.click()}syncAttributes(){this.button.disabled=this.disabled,this.button.type=this.type,this.tabIndex=this.disabled?-1:0;const e=this.getAttribute("aria-label");e?this.button.setAttribute("aria-label",e):this.button.removeAttribute("aria-label")}}function se(){customElements.get(R)||customElements.define(R,re)}const ae='<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="#e8eaed"><path d="m644-428-58-58q9-47-27-88t-93-32l-58-58q17-8 34.5-12t37.5-4q75 0 127.5 52.5T660-500q0 20-4 37.5T644-428Zm128 126-58-56q38-29 67.5-63.5T832-500q-50-101-143.5-160.5T480-720q-29 0-57 4t-55 12l-62-62q41-17 84-25.5t90-8.5q151 0 269 83.5T920-500q-23 59-60.5 109.5T772-302Zm20 246L624-222q-35 11-70.5 16.5T480-200q-151 0-269-83.5T40-500q21-53 53-98.5t73-81.5L56-792l56-56 736 736-56 56ZM222-624q-29 26-53 57t-41 67q50 101 143.5 160.5T480-280q20 0 39-2.5t39-5.5l-36-38q-11 3-21 4.5t-21 1.5q-75 0-127.5-52.5T300-500q0-11 1.5-21t4.5-21l-84-82Zm319 93Zm-151 75Z"/></svg>',le='<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="#e8eaed"><path d="M480-320q75 0 127.5-52.5T660-500q0-75-52.5-127.5T480-680q-75 0-127.5 52.5T300-500q0 75 52.5 127.5T480-320Zm0-72q-45 0-76.5-31.5T372-500q0-45 31.5-76.5T480-608q45 0 76.5 31.5T588-500q0 45-31.5 76.5T480-392Zm0 192q-146 0-266-81.5T40-500q54-137 174-218.5T480-800q146 0 266 81.5T920-500q-54 137-174 218.5T480-200Zm0-300Zm0 220q113 0 207.5-59.5T832-500q-50-101-144.5-160.5T480-720q-113 0-207.5 59.5T128-500q50 101 144.5 160.5T480-280Z"/></svg>',ce=`:host {
  display: block;
  width: 100%;
}

*,
*::before,
*::after {
  box-sizing: border-box;
}

input,
button,
select,
textarea {
  margin: 0;
  font: inherit;
  color: inherit;
}

input,
textarea {
  -webkit-user-select: auto;
}

p {
  margin: 0;
}

.form-control__input-wrapper input,
.password-toggle-btn {
  font: inherit;
}

.form-control {
  position: relative;
  display: flex;
  flex-direction: column;
  width: 100%;
}

.form-control label {
  font-size: var(--font-size-label);
  font-weight: var(--font-weight-regular);
  margin-bottom: var(--spacing-xs);
}

.form-helper-text {
  margin-top: var(--spacing-xs);
  font-size: var(--font-size-caption);
  line-height: 1.25;
  color: var(--color-text-secondary);
}

.form-control__input-wrapper {
  position: relative;
  display: flex;
  align-items: center;
  width: 100%;
  gap: var(--spacing-sm);
}

.form-control__input-wrapper input {
  position: relative;
  display: flex;
  align-items: center;
  width: 100%;
  border: var(--border-01) solid var(--color-border);
  min-height: 44px;
  padding: var(--spacing-xs) var(--spacing-sm);
  border-radius: var(--input-radius);
  background-color: var(--color-bg-input);
  color: var(--color-text);
  font-size: var(--font-size-label);
  line-height: normal;
  appearance: none;
  -webkit-appearance: none;
}

.form-control__input-wrapper input::placeholder {
  color: var(--color-text-mute);
}

.form-control__input-wrapper input[type="time"]::-webkit-calendar-picker-indicator,
.form-control__input-wrapper input[type="date"]::-webkit-calendar-picker-indicator {
  display: none;
  -webkit-appearance: none;
}

.form-control__input-wrapper input:focus-visible,
.password-toggle-btn:focus-visible {
  outline: var(--focus-ring-width) solid var(--focus-ring-color);
  outline-offset: var(--focus-ring-offset);
}

:host(.error) input,
:host([invalid]) input,
:host([aria-invalid="true"]) input,
.form-control__input-wrapper input.error {
  outline: var(--focus-ring-width) solid var(--color-meter-red);
  outline-offset: var(--focus-ring-offset);
}

.duration-fields {
  align-items: center;
}

.duration-fields input {
  min-width: 0;
  flex: 1 1 0;
}

.duration-fields__unit {
  flex: 0 0 auto;
  font-size: var(--font-size-caption);
  font-weight: var(--font-weight-regular);
  color: var(--color-text-secondary);
  text-transform: uppercase;
}

.duration-fields__unit[hidden] {
  display: none;
}

.password-toggle-btn {
  position: absolute;
  right: 6px;
  display: flex;
  width: 36px;
  height: 36px;
  align-items: center;
  justify-content: center;
  padding: 0;
  border: none;
  border-radius: var(--radius-xs);
  background-color: transparent;
  color: var(--color-text-secondary);
  cursor: pointer;
  appearance: none;
  -webkit-appearance: none;
}

.password-toggle-btn:disabled {
  color: var(--color-text-mute);
  cursor: default;
}

.password-toggle-btn:not(:disabled):is(:hover, :active) {
  background-color: var(--color-overlay);
}

.password-icon {
  width: 24px;
  height: 24px;
  fill: var(--color-text-secondary);
}
`,U="ui-input",bt=document.createElement("template");bt.innerHTML=`
  <style>${ce}</style>
  <div class="form-control">
    <label></label>
    <div class="form-control__input-wrapper">
      <input />
      <span class="duration-fields__unit"></span>
    </div>
    <p class="form-helper-text"></p>
  </div>
`;function de(t){return t==="number"||t==="password"||t==="time"||t==="date"?t:"text"}function ue(t){return t==="password"||t==="number"||t==="time"||t==="date"?t:"text"}class he extends HTMLElement{constructor(){super(),this.isPasswordVisible=!1,this.customValidationMessage="";const e=this.attachShadow({mode:"open",delegatesFocus:!0});e.append(bt.content.cloneNode(!0)),this.input=e.querySelector("input"),this.helperText=e.querySelector(".form-helper-text"),this.label=e.querySelector("label"),this.suffix=e.querySelector(".duration-fields__unit"),this.toggleButton=document.createElement("button"),this.wrapper=e.querySelector(".form-control__input-wrapper"),this.toggleButton.className="password-toggle-btn",this.toggleButton.type="button",this.input.addEventListener("input",n=>{this.syncValidationState(),n.stopPropagation(),this.dispatchEvent(new Event("input",{bubbles:!0,composed:!0}))}),this.input.addEventListener("change",n=>{this.syncValidationState(),n.stopPropagation(),this.dispatchEvent(new Event("change",{bubbles:!0,composed:!0}))}),this.input.addEventListener("invalid",()=>{this.syncValidationState()}),this.toggleButton.addEventListener("click",()=>{this.togglePasswordVisibility()})}static get observedAttributes(){return["aria-label","aria-invalid","autocomplete","disabled","helper-text","invalid","inputmode","label","max","min","name","placeholder","readonly","required","step","suffix","type","value","variant"]}connectedCallback(){this.hasAttribute("variant")||this.setAttribute("variant","text"),this.dataset.component=U,this.syncAttributes()}attributeChangedCallback(){this.syncAttributes()}get disabled(){return this.hasAttribute("disabled")}set disabled(e){this.toggleAttribute("disabled",e)}get type(){return this.input.type}set type(e){this.setAttribute("type",e)}get value(){return this.input.value}set value(e){this.input.value=e,this.syncValidationState()}focus(e){this.input.focus(e)}select(){this.input.select()}get validationMessage(){return this.input.validationMessage}get validity(){return this.input.validity}get willValidate(){return this.input.willValidate}checkValidity(){const e=this.input.checkValidity();return this.syncValidationState(),e}reportValidity(){const e=this.input.reportValidity();return this.syncValidationState(),e}setCustomValidity(e){this.customValidationMessage=e,this.input.setCustomValidity(e),this.syncValidationState()}syncAttributes(){const e=de(this.getAttribute("variant")),n=this.getAttribute("helper-text")||"",i=this.id?`${this.id}Input`:"input",o=this.getAttribute("label")||"",a=this.getAttribute("suffix")||"",l=e==="password"?this.isPasswordVisible?"text":"password":this.getAttribute("type")||ue(e);if(this.input.id=i,this.input.type=l,this.input.disabled=this.disabled,this.tabIndex=this.disabled?-1:0,this.input.readOnly=this.hasAttribute("readonly"),this.input.required=this.hasAttribute("required"),this.syncStringAttribute("aria-label"),this.syncStringAttribute("autocomplete"),this.syncStringAttribute("inputmode"),this.syncStringAttribute("max"),this.syncStringAttribute("min"),this.syncStringAttribute("name"),this.syncStringAttribute("placeholder"),this.syncStringAttribute("step"),this.hasAttribute("value")&&this.input.value!==this.getAttribute("value")&&(this.input.value=this.getAttribute("value")||""),this.input.setCustomValidity(this.customValidationMessage),this.label.htmlFor=i,this.label.textContent=o,this.label.hidden=o.length===0,this.helperText.textContent=n,this.helperText.hidden=n.length===0,this.wrapper.classList.toggle("duration-fields",e==="number"),this.suffix.textContent=a,this.suffix.hidden=e!=="number"||a.length===0,n.length>0){const d=`${i}HelperText`;this.helperText.id=d,this.input.setAttribute("aria-describedby",d)}else this.helperText.removeAttribute("id"),this.input.removeAttribute("aria-describedby");e==="password"?(this.toggleButton.isConnected||this.wrapper.append(this.toggleButton),this.toggleButton.disabled=this.disabled,this.toggleButton.setAttribute("aria-label",this.isPasswordVisible?"Hide password":"Show password"),this.toggleButton.innerHTML=ge(this.isPasswordVisible?ae:le,"password-icon")):(this.isPasswordVisible=!1,this.toggleButton.remove()),this.syncValidationState()}syncStringAttribute(e){const n=this.getAttribute(e);if(n===null){this.input.removeAttribute(e);return}this.input.setAttribute(e,n)}togglePasswordVisibility(){this.isPasswordVisible=!this.isPasswordVisible,this.syncAttributes()}syncValidationState(){const e=this.hasAttribute("invalid")||this.getAttribute("aria-invalid")==="true"||!this.input.validity.valid;this.classList.toggle("error",e),this.input.classList.toggle("error",e),this.hasAttribute("invalid")!==e&&this.toggleAttribute("invalid",e),this.getAttribute("aria-invalid")!==String(e)&&this.setAttribute("aria-invalid",String(e)),this.input.getAttribute("aria-invalid")!==String(e)&&this.input.setAttribute("aria-invalid",String(e))}}function ge(t,e){return t.includes("class=")?t.replace("<svg",`<svg class="${e}"`):t.replace("<svg",`<svg class="${e}"`)}function fe(){customElements.get(U)||customElements.define(U,he)}const me=`:host {
  display: block;
  width: 100%;
}

:host([hidden]) {
  display: none;
}

*,
*::before,
*::after {
  box-sizing: border-box;
}

ol,
ul,
menu,
summary {
  margin: 0;
  padding: 0;
  list-style: none;
}

p {
  margin: 0;
}

.networks {
  position: relative;
  width: 100%;
  display: flex;
  flex-direction: column;
  margin-bottom: var(--spacing-lg);
}

.networks__list {
  position: relative;
  display: flex;
  width: 100%;
  flex-direction: column;
  height: 320px;
  overflow-y: auto;
  border: var(--border-01) solid var(--color-border);
  padding: var(--spacing-xs);
  border-radius: var(--radius-lg);
}

.networks__list:focus {
  outline: var(--focus-ring-width) solid var(--focus-ring-color);
  outline-offset: var(--focus-ring-offset);
}

.networks__list-empty-state {
  position: relative;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  width: 100%;
  height: 100%;
  color: var(--color-text-secondary);
  font-size: var(--font-size-heading);
}

.wifi-find-icon {
  width: 64px;
  height: 64px;
  fill: var(--color-text-mute);
}

.networks__item {
  position: relative;
  width: 100%;
  display: flex;
  align-items: center;
  cursor: pointer;
  padding: var(--spacing-sm) var(--spacing-xs);
  border: var(--border-01) solid transparent;
  min-height: 32px;
  font-size: var(--font-size-label);
  justify-content: space-between;
  border-radius: var(--radius-xs);
}

.networks__item-ssid {
  font-weight: var(--font-weight-semibold);
}

.networks__item-details {
  display: flex;
  align-items: center;
  gap: var(--spacing-md);
}

.networks__item ~ .networks__item {
  border-top: var(--border-01) solid var(--color-bg-selected);
}

.networks__item[aria-selected="true"] {
  background-color: var(--color-bg-active);
  outline: var(--focus-ring-width) solid var(--color-fg-active);
  outline-offset: var(--focus-ring-offset);
}

.wifi-security-icon,
.wifi-signal-icon,
.wifi-connected-icon {
  width: 16px;
  height: 16px;
  fill: var(--color-text-secondary);
}
`,W="ui-network-list",vt="networkList",J="network-list-label",yt=document.createElement("template");yt.innerHTML=`
  <style>${me}</style>
  <div class="networks">
    <span id="${J}" hidden>Available networks</span>
    <ul
      id="${vt}"
      class="networks__list"
      role="listbox"
      aria-labelledby="${J}"
      tabindex="0"
    ></ul>
  </div>
`;class pe extends HTMLElement{static get observedAttributes(){return["aria-label","label"]}constructor(){super();const e=this.attachShadow({mode:"open",delegatesFocus:!0});e.append(yt.content.cloneNode(!0)),this.labelElement=e.querySelector(`#${J}`),this.listElement=e.querySelector(`#${vt}`),this.listElement.addEventListener("keydown",n=>{(n.key==="ArrowDown"||n.key==="ArrowUp"||n.key==="Home"||n.key==="End"||n.key===" ")&&n.preventDefault()})}connectedCallback(){this.tabIndex=0,this.dataset.component=W,this.syncAttributes()}attributeChangedCallback(){this.syncAttributes()}appendItem(e){this.listElement.appendChild(e)}clearItems(){this.listElement.innerHTML=""}focus(e){this.listElement.focus(e)}getOptions(){return Array.from(this.listElement.querySelectorAll('[role="option"]'))}scrollOptionIntoView(e){const n=this.getOptions()[e];if(!n)return;const i=this.listElement.scrollTop,o=i+this.listElement.clientHeight,a=n.offsetTop,l=a+n.offsetHeight;if(a<i){this.listElement.scrollTop=a;return}l>o&&(this.listElement.scrollTop=l-this.listElement.clientHeight)}setActiveDescendant(e){if(e){this.listElement.setAttribute("aria-activedescendant",e);return}this.listElement.removeAttribute("aria-activedescendant")}setEmptyState(e){this.clearItems(),this.appendItem(e)}syncAttributes(){const e=this.getAttribute("label")||"Available networks",n=this.getAttribute("aria-label");this.labelElement.textContent=e,n?this.listElement.setAttribute("aria-label",n):this.listElement.removeAttribute("aria-label")}}function be(){customElements.get(W)||customElements.define(W,pe)}const ve=`:host {
  display: block;
  width: 100%;
}

:host([hidden]) {
  display: none;
}

*,
*::before,
*::after {
  box-sizing: border-box;
}

.wifi-status-card {
  position: relative;
  display: flex;
  width: 100%;
  align-items: center;
  background-color: var(--color-bg-brand);
  padding: var(--spacing-md) var(--spacing-md);
  border-radius: var(--radius-lg);
}

.wifi-status-card__content {
  position: relative;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: var(--spacing-sm);
  flex-wrap: wrap;
  width: 100%;
}

.wifi-status-card__content .wifi-icon {
  width: 32px;
  height: 32px;
  fill: var(--color-text-inverse);
}

.wifi-status-card__actions {
  position: relative;
  display: flex;
  align-items: center;
  gap: var(--spacing-sm);
  margin-left: auto;
}

.wifi-status-card__status {
  position: relative;
  display: flex;
  align-items: center;
  gap: var(--spacing-xs);
  min-width: 0;
}

.wifi-status-card__label {
  font-size: var(--font-size-label);
  color: var(--color-text-inverse);
  font-weight: var(--font-weight-medium);
}

.wifi-status-card__network {
  font-size: var(--font-size-label);
  color: var(--color-text-inverse);
  font-weight: var(--font-weight-semibold);
  min-width: 0;
  overflow-wrap: anywhere;
}
`,j="ui-network-status",wt="actionBtn",Ct="settingsBtn",xt="wifiStatusLabel",St="wifiStatusIcon",Tt="wifiStatusNetwork",kt=document.createElement("template");kt.innerHTML=`
  <style>${ve}</style>
  <div class="wifi-status-card">
    <div class="wifi-status-card__content">
      <div class="wifi-status-card__status">
        <span class="card-icon" id="${St}"></span>
        <span id="${xt}" class="wifi-status-card__label">
          Disconnected
        </span>
        <span
          id="${Tt}"
          class="wifi-status-card__network"
        ></span>
      </div>
      <div class="wifi-status-card__actions">
        <ui-button
          id="${wt}"
          variant="outline"
          inverse
          aria-label="Connect"
        >
          Connect
        </ui-button>
        <ui-button
          id="${Ct}"
          variant="outline"
          inverse
          aria-label="Settings"
        >
          Settings
        </ui-button>
      </div>
    </div>
  </div>
`;function ye(t,e){return t.includes("class=")?t.replace("<svg",`<svg class="${e}"`):t.replace("<svg",`<svg class="${e}"`)}class we extends HTMLElement{static get observedAttributes(){return["action-label","network","status-label"]}constructor(){super();const e=this.attachShadow({mode:"open"});e.append(kt.content.cloneNode(!0)),this.actionButton=e.querySelector(`#${wt}`),this.actionButton.addEventListener("click",n=>{n.stopPropagation(),this.dispatchEvent(new Event("action",{bubbles:!0,composed:!0}))}),this.iconElement=e.querySelector(`#${St}`),this.labelElement=e.querySelector(`#${xt}`),this.networkElement=e.querySelector(`#${Tt}`),this.settingsButton=e.querySelector(`#${Ct}`),this.settingsButton.addEventListener("click",n=>{n.stopPropagation(),this.dispatchEvent(new Event("settings",{bubbles:!0,composed:!0}))})}connectedCallback(){this.dataset.component=j,this.syncAttributes()}attributeChangedCallback(){this.syncAttributes()}get actionDisabled(){return!!this.actionButton.disabled}set actionDisabled(e){this.actionButton.disabled=e}get iconSvg(){return this.iconElement.innerHTML}set iconSvg(e){this.iconElement.innerHTML=e?ye(e,"wifi-icon"):""}get network(){return this.getAttribute("network")||""}set network(e){this.setAttribute("network",e)}get actionLabel(){return this.getAttribute("action-label")||"Connect"}set actionLabel(e){this.setAttribute("action-label",e)}get statusLabel(){return this.getAttribute("status-label")||"Disconnected"}set statusLabel(e){this.setAttribute("status-label",e)}syncAttributes(){this.actionButton.textContent=this.actionLabel,this.actionButton.setAttribute("aria-label",this.actionLabel),this.labelElement.textContent=this.statusLabel,this.networkElement.textContent=this.network,this.networkElement.hidden=this.network.length===0}}function Ce(){customElements.get(j)||customElements.define(j,we)}const xe=`:host {
  display: block;
  width: 100%;
}

*,
*::before,
*::after {
  box-sizing: border-box;
}

input,
textarea {
  margin: 0;
  font: inherit;
  color: inherit;
  -webkit-user-select: auto;
}

.form-control {
  position: relative;
  display: flex;
  flex-direction: column;
  width: 100%;
}

.form-control label {
  font-size: var(--font-size-label);
  font-weight: var(--font-weight-regular);
  margin-bottom: var(--spacing-xs);
}

.range-slider {
  width: 100%;
  display: flex;
  align-items: center;
  gap: var(--spacing-sm);
}

.range-slider__range {
  -webkit-appearance: none;
  appearance: none;
  width: 100%;
  height: 34px;
  border-radius: var(--radius-full);
  background: var(--color-bg-input);
  outline: none;
}

.range-slider__range:focus-visible {
  outline: var(--focus-ring-width) solid var(--focus-ring-color);
  outline-offset: var(--focus-ring-offset);
}

.range-slider__range::-webkit-slider-thumb {
  -webkit-appearance: none;
  appearance: none;
  width: 30px;
  height: 30px;
  border-radius: 50%;
  background: var(--color-bg);
  border: var(--border-03) solid var(--color-bg-brand);
  box-shadow: 0px 2px 6px -2px rgba(32, 32, 32, 0.4);
  cursor: pointer;
  transition: background 0.15s ease-in-out;
}

.range-slider__range::-webkit-slider-thumb:hover {
  background: var(--color-bg);
}

.range-slider__range:active::-webkit-slider-thumb {
  background: var(--color-bg);
}

.range-slider__range::-moz-range-thumb {
  width: 30px;
  height: 30px;
  border: var(--border-03) solid var(--color-bg-brand);
  border-radius: 50%;
  background: var(--color-bg);
  box-shadow: 0px 2px 6px -2px rgba(32, 32, 32, 0.4);
  cursor: pointer;
  transition: background 0.15s ease-in-out;
}

.range-slider__range::-moz-range-thumb:hover {
  background: var(--color-bg);
}

.range-slider__range:active::-moz-range-thumb {
  background: var(--color-bg);
}

.range-slider__value {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  min-width: 48px;
  height: 28px;
  font-size: var(--font-size-label);
  font-weight: var(--font-weight-semibold);
  color: var(--color-text-inverse);
  background: var(--color-bg-brand);
  border-radius: var(--radius-md);
}
`,Se=document.createElement("template");Se.innerHTML=`
  <style>${xe}</style>
  <div class="form-control">
    <label></label>
    <div class="range-slider">
      <input class="range-slider__range" type="range" />
      <span class="range-slider__value"></span>
    </div>
  </div>
`;const Te='<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="#e8eaed"><path d="M480-80 240-320l57-57 183 183 183-183 57 57L480-80ZM298-584l-58-56 240-240 240 240-58 56-182-182-182 182Z"/></svg>',ke=`:host {
  display: block;
  width: 100%;
}

*,
*::before,
*::after {
  box-sizing: border-box;
}

input,
button,
select,
textarea {
  margin: 0;
  font: inherit;
  color: inherit;
}

input,
textarea {
  -webkit-user-select: auto;
}

p {
  margin: 0;
}

.form-control {
  position: relative;
  display: flex;
  flex-direction: column;
  width: 100%;
}

.form-control label {
  font-size: var(--font-size-label);
  font-weight: var(--font-weight-regular);
  margin-bottom: var(--spacing-xs);
}

.form-helper-text {
  margin-top: var(--spacing-xs);
  font-size: var(--font-size-caption);
  line-height: 1.25;
  color: var(--color-text-secondary);
}

.form-control__input-wrapper {
  position: relative;
  display: flex;
  align-items: center;
  width: 100%;
  gap: var(--spacing-sm);
}

.form-control__input-wrapper select {
  position: relative;
  display: flex;
  align-items: center;
  width: 100%;
  border: var(--border-01) solid var(--color-border);
  min-height: 44px;
  padding: var(--spacing-xs) var(--spacing-sm);
  border-radius: var(--input-radius);
  background-color: var(--color-bg-input);
  color: var(--color-text);
  font-size: var(--font-size-label);
  line-height: normal;
  appearance: none;
  -webkit-appearance: none;
}

.select-icon {
  position: absolute;
  width: 32px;
  height: 18px;
  background-color: transparent;
  border-radius: var(--radius-xs);
  right: 2px;
  fill: var(--color-text-secondary);
  pointer-events: none;
}

.form-control__input-wrapper select:focus-visible {
  outline: var(--focus-ring-width) solid var(--focus-ring-color);
  outline-offset: var(--focus-ring-offset);
}

:host(.error) select,
:host([invalid]) select,
:host([aria-invalid="true"]) select,
.form-control__input-wrapper select.error {
  outline: var(--focus-ring-width) solid var(--color-meter-red);
  outline-offset: var(--focus-ring-offset);
}
`,Y="ui-select",At=document.createElement("template");At.innerHTML=`
  <style>${ke}</style>
  <div class="form-control">
    <label></label>
    <div class="form-control__input-wrapper">
      <select></select>
      <span class="select-icon"></span>
    </div>
    <p class="form-helper-text"></p>
  </div>
`;class Ae extends HTMLElement{constructor(){super(),this.customValidationMessage="";const e=this.attachShadow({mode:"open",delegatesFocus:!0});e.append(At.content.cloneNode(!0)),this.helperText=e.querySelector(".form-helper-text"),this.labelElement=e.querySelector("label"),this.select=e.querySelector("select");const n=e.querySelector(".select-icon");n.innerHTML=Le(Te,"select-icon"),this.select.addEventListener("change",i=>{this.syncValidationState(),i.stopPropagation(),this.dispatchEvent(new Event("change",{bubbles:!0,composed:!0}))}),this.select.addEventListener("input",i=>{this.syncValidationState(),i.stopPropagation(),this.dispatchEvent(new Event("input",{bubbles:!0,composed:!0}))}),this.select.addEventListener("invalid",()=>{this.syncValidationState()})}static get observedAttributes(){return["aria-invalid","aria-label","disabled","helper-text","invalid","label","name","required","value"]}connectedCallback(){this.moveLightDomOptions(),this.dataset.component=Y,this.syncAttributes()}attributeChangedCallback(){this.syncAttributes()}get disabled(){return this.hasAttribute("disabled")}set disabled(e){this.toggleAttribute("disabled",e)}get innerHTML(){return this.select.innerHTML}set innerHTML(e){this.select.innerHTML=e,this.syncValidationState()}get value(){return this.select.value}set value(e){this.select.value=e,this.syncValidationState()}appendChild(e){return e instanceof HTMLOptionElement||e instanceof HTMLOptGroupElement?(this.select.appendChild(e),e):super.appendChild(e)}focus(e){this.select.focus(e)}get validationMessage(){return this.select.validationMessage}get validity(){return this.select.validity}get willValidate(){return this.select.willValidate}checkValidity(){const e=this.select.checkValidity();return this.syncValidationState(),e}reportValidity(){const e=this.select.reportValidity();return this.syncValidationState(),e}setCustomValidity(e){this.customValidationMessage=e,this.select.setCustomValidity(e),this.syncValidationState()}moveLightDomOptions(){Array.from(this.children).forEach(e=>{(e instanceof HTMLOptionElement||e instanceof HTMLOptGroupElement)&&this.select.appendChild(e)})}syncAttributes(){const e=this.getAttribute("helper-text")||"",n=this.id?`${this.id}Select`:"select",i=this.getAttribute("label")||"";if(this.select.id=n,this.select.disabled=this.disabled,this.tabIndex=this.disabled?-1:0,this.select.required=this.hasAttribute("required"),this.syncStringAttribute("aria-label"),this.syncStringAttribute("name"),this.hasAttribute("value")&&(this.select.value=this.getAttribute("value")||""),this.select.setCustomValidity(this.customValidationMessage),this.labelElement.htmlFor=n,this.labelElement.textContent=i,this.labelElement.hidden=i.length===0,this.helperText.textContent=e,this.helperText.hidden=e.length===0,e.length>0){const o=`${n}HelperText`;this.helperText.id=o,this.select.setAttribute("aria-describedby",o)}else this.helperText.removeAttribute("id"),this.select.removeAttribute("aria-describedby");this.syncValidationState()}syncStringAttribute(e){const n=this.getAttribute(e);if(n===null){this.select.removeAttribute(e);return}this.select.setAttribute(e,n)}syncValidationState(){const e=this.hasAttribute("invalid")||this.getAttribute("aria-invalid")==="true"||!this.select.validity.valid;this.classList.toggle("error",e),this.select.classList.toggle("error",e),this.hasAttribute("invalid")!==e&&this.toggleAttribute("invalid",e),this.getAttribute("aria-invalid")!==String(e)&&this.setAttribute("aria-invalid",String(e)),this.select.getAttribute("aria-invalid")!==String(e)&&this.select.setAttribute("aria-invalid",String(e))}}function Ee(){customElements.get(Y)||customElements.define(Y,Ae)}function Le(t,e){return t.includes("class=")?t.replace("<svg",`<svg class="${e}"`):t.replace("<svg",`<svg class="${e}"`)}const _e=`:host {
  display: block;
  width: 100%;
}

*,
*::before,
*::after {
  box-sizing: border-box;
}

.form-control {
  position: relative;
  display: flex;
  flex-direction: column;
  width: 100%;
}

.switch-container {
  display: flex;
  align-items: center;
  gap: var(--spacing-sm);
}

.switch-label {
  font-size: var(--font-size-label);
  font-weight: var(--font-weight-medium);
  flex-shrink: 0;
}

button {
  all: unset;
  box-sizing: border-box;
  position: relative;
  display: inline-flex;
  align-items: center;
  flex-shrink: 0;
  width: 62px;
  height: 34px;
  padding: 0;
  border: none;
  border-radius: var(--radius-full);
  background-color: var(--color-bg-input);
  color: inherit;
  cursor: pointer;
  transition: background-color 0.2s ease;
}

button:disabled {
  cursor: not-allowed;
  opacity: 0.6;
}

.switch-label[hidden] {
  display: none;
}

button[data-checked] {
  background-color: var(--color-bg-brand);
}

button::before {
  content: "";
  position: absolute;
  inset: 0;
  border-radius: inherit;
  opacity: 0;
  box-shadow: 0 0 0 var(--focus-ring-width) currentColor;
  transition: opacity 0.2s ease;
}

button:focus-visible::before {
  opacity: 0.35;
}

.knob {
  position: absolute;
  left: 2px;
  width: 30px;
  height: 30px;
  border-radius: var(--radius-full);
  background-color: var(--color-fg-knob-off);
  box-shadow: 0 2px 6px -2px rgba(32, 32, 32, 0.4);
  transition:
    transform 0.2s cubic-bezier(0.175, 0.885, 0.32, 1.275),
    background-color 0.2s ease;
}

button[data-checked] .knob {
  background-color: var(--color-fg-knob-on);
}
`,Ie=document.createElement("template");Ie.innerHTML=`
  <style>${_e}</style>
  <div class="form-control">
    <div class="switch-container">
      <button type="button" part="button">
        <span class="knob"></span>
      </button>
      <span class="switch-label"></span>
    </div>
  </div>
`;const Me=`:host {
  display: contents;
}

*,
*::before,
*::after {
  box-sizing: border-box;
}

.toast-dialog {
  position: fixed;
  left: 50%;
  bottom: var(--spacing-4xl);
  margin: 0;
  padding: 0;
  border: none;
  background: transparent;
  color: inherit;
  width: min(50vw, calc(100vw - 2rem));
  max-width: calc(100vw - 2rem);
  pointer-events: none;
  z-index: 30;
}

.toast-dialog[open] {
  display: block;
  animation: toast-dialog-fade 2.6s ease forwards;
}

.toast-dialog::backdrop {
  display: none;
}

.toast-dialog__message {
  margin: 0;
  box-sizing: border-box;
  display: flex;
  align-items: center;
  justify-content: center;
  width: 100%;
  padding: var(--spacing-sm) var(--spacing-lg);
  min-height: 44px;
  overflow: hidden;
  border: var(--border-01) solid var(--color-meter-green);
  border-radius: var(--radius-lg);
  background: var(--color-bg-active);
  color: var(--color-text);
  box-shadow: 0 0.5rem 1.25rem var(--color-overlay);
  font-size: var(--font-size-label);
  line-height: 1.25;
  text-align: center;
  text-overflow: ellipsis;
}

.toast-dialog[data-status='error'] .toast-dialog__message,
.toast-dialog[data-status='danger'] .toast-dialog__message {
  border-color: var(--color-text-danger);
  background: var(--color-bg-danger);
  color: var(--color-text-danger);
}

.toast-dialog[data-status='warning'] .toast-dialog__message {
  border-color: var(--color-meter-yellow);
  background: var(--color-bg-selected);
  color: var(--color-text-warning);
}

@keyframes toast-dialog-fade {
  0% {
    opacity: 0;
    transform: translate(-50%, 0.75rem);
  }

  12%,
  78% {
    opacity: 1;
    transform: translate(-50%, 0);
  }

  100% {
    opacity: 0;
    transform: translate(-50%, -0.25rem);
  }
}
`,X="ui-toast",Et=document.createElement("template");Et.innerHTML=`
  <style>${Me}</style>
  <dialog class="toast-dialog" aria-live="polite">
    <p class="toast-dialog__message"></p>
  </dialog>
`;function lt(t){return t==="success"||t==="info"||t==="warning"||t==="error"||t==="danger"?t:null}class Be extends HTMLElement{static get observedAttributes(){return["aria-label","message","status"]}constructor(){super();const e=this.attachShadow({mode:"open"});e.append(Et.content.cloneNode(!0)),this.dialog=e.querySelector("dialog"),this.messageElement=e.querySelector(".toast-dialog__message")}connectedCallback(){this.dataset.component=X,this.syncAttributes()}attributeChangedCallback(){this.syncAttributes()}get message(){return this.messageElement.textContent||""}set message(e){this.setAttribute("message",e)}get status(){return lt(this.getAttribute("status"))}set status(e){if(e){this.setAttribute("status",e);return}this.removeAttribute("status")}show(e,n){typeof e=="string"&&(this.message=e),n&&(this.status=n),this.dialog.open||this.dialog.show()}close(e){this.dialog.close(e)}syncAttributes(){const e=this.getAttribute("aria-label"),n=this.getAttribute("message")||"",i=lt(this.getAttribute("status"));e?this.dialog.setAttribute("aria-label",e):this.dialog.removeAttribute("aria-label"),this.messageElement.textContent=n,this.messageElement.id=this.id?`${this.id}Message`:"statusToastMessage",this.dialog.id=this.id||"statusToast",i?this.dialog.setAttribute("data-status",i):this.dialog.removeAttribute("data-status")}}function ze(){customElements.get(X)||customElements.define(X,Be)}const qe=`<svg width="260" height="93" viewBox="0 0 260 93" fill="none" xmlns="http://www.w3.org/2000/svg">
<path d="M243.869 16.7734C246.778 16.7735 249.246 17.3268 251.272 18.4336C253.299 19.4666 254.961 21.1643 256.26 23.5254C257.559 25.8128 258.493 28.8385 259.064 32.6016C259.688 36.2908 260 40.7546 260 45.9932C260 51.2319 259.688 55.733 259.064 59.4961C258.493 63.1854 257.559 66.2111 256.26 68.5723C254.961 70.9334 253.272 72.631 251.194 73.6641C249.168 74.7708 246.726 75.3242 243.869 75.3242C240.908 75.3242 238.361 74.7709 236.231 73.6641C235.938 73.5216 235.653 73.3655 235.375 73.1992V90.708L221.659 92.2578V18.3232H228.438L231.045 23.6045C231.06 23.5782 231.074 23.5515 231.089 23.5254C232.388 21.1643 234.101 19.4666 236.231 18.4336C238.361 17.3268 240.908 16.7734 243.869 16.7734ZM77.5605 59.9199C79.3329 60.0681 80.9789 60.217 82.498 60.3652C84.0173 60.4393 85.4736 60.5128 86.8662 60.5869L85.1572 73.8164C81.7389 74.3352 77.9328 74.7426 76.6016 75.0391C75.2704 75.3354 73.9922 75.4844 72.7676 75.4844C69.6261 75.4843 67.2825 74.5949 65.7383 72.8164C64.3086 71.0399 63.5657 68.3427 63.5068 64.7266C64.0174 63.2715 64.435 61.7087 64.7617 60.0449L64.7637 60.0371C65.5282 56.0661 65.8984 51.4211 65.8984 46.1289C65.8984 41.1018 65.5728 36.7017 64.9004 32.9541L64.7617 32.2139C64.4345 30.5162 64.0166 28.9283 63.502 27.46V1.66797L77.5605 0V59.9199ZM103.92 59.9199C105.66 60.0654 107.278 60.211 108.773 60.3564C109.521 63.796 110.614 66.7901 112.103 69.2783L111.517 73.8164C108.098 74.3352 104.292 74.7426 102.961 75.0391C101.63 75.3354 100.352 75.4844 99.127 75.4844C95.9855 75.4843 93.6419 74.5949 92.0977 72.8164C90.6067 70.9636 89.8614 68.1095 89.8613 64.2559V1.66797L103.92 0V59.9199ZM43.7715 16.7734C47.3069 16.7734 50.3228 17.3295 52.8184 18.4414C55.3139 19.4792 57.3416 21.1845 58.9014 23.5566C60.5131 25.8546 61.6823 28.8943 62.4102 32.6748C63.138 36.3812 63.502 40.8659 63.502 46.1289C63.5019 51.3178 63.138 55.8025 62.4102 59.583C61.6823 63.2894 60.5131 66.3291 58.9014 68.7012C57.3417 70.9991 55.3138 72.7035 52.8184 73.8154C50.3228 74.9274 47.3069 75.4834 43.7715 75.4834C40.2363 75.4834 37.2211 74.9273 34.7256 73.8154C32.2301 72.7035 30.1762 70.9991 28.5645 68.7012C27.0047 66.3291 25.8346 63.2894 25.0547 59.583C24.3268 55.8025 23.9629 51.3178 23.9629 46.1289C23.9629 45.621 23.9678 45.1203 23.9746 44.627C23.9762 44.5097 23.9765 44.3928 23.9785 44.2764C23.9864 43.8165 23.9979 43.3631 24.0117 42.916C24.0153 42.8007 24.0194 42.6858 24.0234 42.5713C24.0396 42.1046 24.0592 41.645 24.082 41.1924C24.0901 41.0326 24.0985 40.8738 24.1074 40.7158C24.1128 40.6191 24.1183 40.5228 24.124 40.4268C24.1329 40.2782 24.1417 40.1304 24.1514 39.9834C24.328 37.2806 24.6287 34.8443 25.0547 32.6748C25.8346 28.8942 27.0047 25.8546 28.5645 23.5566C29.5167 22.1551 30.624 20.9875 31.8848 20.0518C31.9036 20.0377 31.9225 20.0237 31.9414 20.0098C32.0486 19.9311 32.1572 19.8546 32.2666 19.7793C32.2959 19.7591 32.325 19.7387 32.3545 19.7188C32.4538 19.6515 32.5542 19.586 32.6553 19.5215C32.6971 19.4948 32.7392 19.4686 32.7812 19.4424C32.8787 19.3818 32.9771 19.3228 33.0762 19.2646C33.1172 19.2406 33.1579 19.216 33.1992 19.1924C33.2949 19.1376 33.3911 19.0837 33.4883 19.0312C33.5403 19.0032 33.5931 18.9766 33.6455 18.9492C33.7419 18.8988 33.8387 18.8489 33.9365 18.8008C33.9803 18.7792 34.0243 18.7584 34.0684 18.7373C34.1661 18.6905 34.2641 18.6442 34.3633 18.5996C34.3848 18.5899 34.4062 18.5799 34.4277 18.5703L34.4268 18.5713C34.5257 18.5274 34.6252 18.4832 34.7256 18.4414C37.221 17.3296 40.2364 16.7735 43.7715 16.7734ZM129.773 16.7734C133.309 16.7734 136.325 17.3295 138.82 18.4414C141.316 19.4792 143.344 21.1845 144.903 23.5566C146.515 25.8546 147.685 28.8943 148.413 32.6748C148.784 34.5656 149.059 36.6592 149.241 38.9551L142.836 41.3262C137.822 43.1818 137.822 50.2732 142.836 52.1289L149.135 54.46C148.958 56.2912 148.718 57.999 148.413 59.583C147.685 63.2894 146.515 66.3291 144.903 68.7012C143.344 70.999 141.316 72.7035 138.82 73.8154C136.325 74.9273 133.309 75.4834 129.773 75.4834C126.238 75.4833 123.223 74.9273 120.728 73.8154C118.232 72.7035 116.178 70.9991 114.566 68.7012C113.846 67.6055 113.21 66.3668 112.656 64.9863V64.9883C112.643 64.955 112.63 64.9211 112.617 64.8877C112.567 64.7609 112.517 64.6331 112.468 64.5039C112.453 64.465 112.438 64.4259 112.424 64.3867C112.306 64.0717 112.192 63.7499 112.083 63.4209C112.042 63.2964 112.002 63.1705 111.962 63.0439C111.618 61.9632 111.316 60.8099 111.058 59.583C110.33 55.8025 109.966 51.3178 109.966 46.1289C109.966 40.8659 110.33 36.3812 111.058 32.6748C111.837 28.8943 113.007 25.8546 114.566 23.5566C116.178 21.1846 118.232 19.4792 120.728 18.4414C123.223 17.3296 126.238 16.7735 129.773 16.7734ZM194.655 61.9453H200.43C201.886 61.9453 202.9 61.6464 203.473 61.0498C204.045 60.453 204.331 59.4455 204.331 58.0283V17.9717H218.064V73.917H211.276L207.765 68.3223C205.58 70.7091 203.187 72.4999 200.586 73.6934C197.985 74.8868 195.201 75.4834 192.236 75.4834C188.335 75.4834 185.474 74.29 183.653 71.9033C181.833 69.4417 180.922 65.5627 180.922 60.2666V53.8076L185.459 52.1289C190.473 50.2731 190.473 43.1808 185.459 41.3252L180.922 39.6455V17.9717H194.655V61.9453ZM34.7266 15.8535C34.4142 15.9718 34.1065 16.0962 33.8047 16.2295V16.2285C31.4506 17.2075 29.4409 18.6911 27.7812 20.6367H13.9463V33.8584H22.4111C21.8428 37.4038 21.5664 41.5008 21.5664 46.1289C21.5664 46.746 21.5719 47.3546 21.582 47.9541H13.9463V73.0869H0V4.79199H35.9443L34.7266 15.8535ZM163.023 27C163.409 25.9569 164.885 25.9569 165.271 27L170.104 40.0625C170.226 40.3904 170.485 40.6491 170.812 40.7705L183.876 45.6045C183.995 45.6486 184.1 45.7071 184.192 45.7764C184.248 45.8181 184.297 45.8642 184.343 45.9131C184.364 45.9355 184.382 45.9597 184.4 45.9834C184.417 46.0043 184.434 46.0241 184.449 46.0459C184.465 46.0691 184.478 46.094 184.492 46.1182C184.509 46.1465 184.525 46.1746 184.539 46.2041C184.548 46.2237 184.556 46.2437 184.564 46.2637C184.577 46.2943 184.589 46.325 184.599 46.3564C184.607 46.3826 184.614 46.409 184.62 46.4355C184.627 46.4644 184.634 46.4932 184.639 46.5225C184.643 46.5487 184.646 46.5751 184.648 46.6016C184.651 46.6299 184.654 46.6581 184.655 46.6865C184.656 46.7136 184.656 46.7405 184.655 46.7676C184.654 46.8006 184.651 46.8334 184.647 46.8662C184.645 46.8881 184.642 46.9099 184.639 46.9316C184.633 46.9629 184.627 46.9936 184.619 47.0244C184.612 47.0514 184.605 47.078 184.597 47.1045C184.588 47.1306 184.579 47.1561 184.568 47.1816C184.557 47.2097 184.546 47.2374 184.532 47.2646C184.521 47.2881 184.507 47.3103 184.494 47.333C184.479 47.3585 184.466 47.3847 184.449 47.4092C184.432 47.435 184.411 47.4589 184.392 47.4834C184.374 47.5049 184.357 47.5265 184.338 47.5469C184.315 47.5708 184.29 47.592 184.266 47.6143C184.246 47.6322 184.227 47.6512 184.205 47.668C184.199 47.673 184.192 47.6777 184.186 47.6826C184.095 47.75 183.993 47.8084 183.876 47.8516L170.812 52.6855C170.485 52.807 170.226 53.0657 170.104 53.3936L165.271 66.4561C164.885 67.4992 163.409 67.4992 163.023 66.4561L158.19 53.3936C158.069 53.0657 157.81 52.8069 157.482 52.6855L144.419 47.8516C143.376 47.4655 143.376 45.9907 144.419 45.6045L144.762 45.4775L145.382 45.2471L149.477 43.7314V43.7324L157.482 40.7705C157.81 40.6491 158.069 40.3904 158.19 40.0625L163.023 27ZM238.491 31.6055C237.297 31.6055 236.466 31.8269 235.998 32.2695C235.583 32.6385 235.375 33.3759 235.375 34.4824V62.7734L242.076 60.4922C243.323 60.4922 244.155 60.3084 244.57 59.9395C245.038 59.4967 245.271 58.6845 245.271 57.5039V29.3242L238.491 31.6055ZM41.9004 31.6738C40.6526 31.6738 39.7941 31.896 39.3262 32.3408C38.9104 32.7115 38.7031 33.49 38.7031 34.6758V62.584L45.6436 60.584C46.8909 60.584 47.7226 60.3987 48.1387 60.0283C48.6065 59.5836 48.8408 58.7679 48.8408 57.582V29.6738L41.9004 31.6738ZM127.902 31.6738C126.655 31.6738 125.797 31.8962 125.329 32.3408C124.913 32.7115 124.705 33.4899 124.705 34.6758V62.584L131.646 60.584C132.893 60.584 133.726 60.3989 134.142 60.0283C134.609 59.5836 134.843 58.7678 134.843 57.582V29.6738L127.902 31.6738Z" fill="currentColor"/>
</svg>
`,Pe='<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="#e8eaed"><path d="m109-531-85-85q92-89 210-136.5T480-800q128 0 246 47.5T936-616l-85 85q-75-72-171-110.5T480-680q-104 0-200 38.5T109-531Zm169 169-84-84q59-55 132.5-84.5T480-560q80 0 153.5 29.5T766-446l-84 84q-42-38-93.5-58T480-440q-57 0-108.5 20T278-362Zm202 202q-33 0-56.5-23.5T400-240q0-33 23.5-56.5T480-320q33 0 56.5 23.5T560-240q0 33-23.5 56.5T480-160Z"/></svg>',Oe='<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="#e8eaed"><path d="M240-80q-33 0-56.5-23.5T160-160v-400q0-33 23.5-56.5T240-640h40v-80q0-83 58.5-141.5T480-920q83 0 141.5 58.5T680-720v80h40q33 0 56.5 23.5T800-560v400q0 33-23.5 56.5T720-80H240Zm0-80h480v-400H240v400Zm240-120q33 0 56.5-23.5T560-360q0-33-23.5-56.5T480-440q-33 0-56.5 23.5T400-360q0 33 23.5 56.5T480-280ZM360-640h240v-80q0-50-35-85t-85-35q-50 0-85 35t-35 85v80ZM240-160v-400 400Z"/></svg>',Ne='<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="#e8eaed"><path d="M480-120 0-600q96-98 220-149t260-51q137 0 261 51t219 149L480-120ZM361-353q25-18 55.5-28t63.5-10q33 0 63.5 10t55.5 28l245-245q-78-59-170.5-90.5T480-720q-101 0-193.5 31.5T116-598l245 245Z"/></svg>',Ve='<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="#e8eaed"><path d="M480-120 0-600q96-98 220-149t260-51q137 0 261 51t219 149L480-120ZM299-415q38-28 84-43.5t97-15.5q51 0 97 15.5t84 43.5l183-183q-78-59-170.5-90.5T480-720q-101 0-193.5 31.5T116-598l183 183Z"/></svg>',De='<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="#e8eaed"><path d="M480-120 0-600q96-98 220-149t260-51q137 0 261 51t219 149L480-120ZM232-482q53-38 116-59.5T480-563q69 0 132 21.5T728-482l116-116q-78-59-170.5-90.5T480-720q-101 0-193.5 31.5T116-598l116 116Z"/></svg>',He='<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="#e8eaed"><path d="M480-120 0-600q95-97 219.5-148.5T480-800q136 0 260.5 51.5T960-600L480-120Z"/></svg>',$e='<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="#e8eaed"><path d="M480-120 0-601q93-93 215.5-146T480-800q142 0 264.5 53T960-601l-56 57q-81-81-190-128.5T480-720q-103 0-195 32.5T117-597l419 420-56 57Zm384-40L761-262q-18 11-38 16.5t-43 5.5q-68 0-114-46t-46-114q0-68 46-114t114-46q68 0 114 46t46 114q0 23-5.5 43T818-319l102 103-56 56ZM680-320q34 0 57-23t23-57q0-34-23-57t-57-23q-34 0-57 23t-23 57q0 34 23 57t57 23ZM480-177Z"/></svg>',Ke='<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="#e8eaed"><path d="M280-240q-100 0-170-70T40-480q0-100 70-170t170-70q66 0 121 33t87 87h432v240h-80v120H600v-120H488q-32 54-87 87t-121 33Zm0-80q66 0 106-40.5t48-79.5h246v120h80v-120h80v-80H434q-8-39-48-79.5T280-640q-66 0-113 47t-47 113q0 66 47 113t113 47Zm0-80q33 0 56.5-23.5T360-480q0-33-23.5-56.5T280-560q-33 0-56.5 23.5T200-480q0 33 23.5 56.5T280-400Zm0-80Z"/></svg>',Fe='<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="#e8eaed"><path d="M382-240 154-468l57-57 171 171 367-367 57 57-424 424Z"/></svg>',Ze='<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="#e8eaed"><path d="m612-292 56-56-148-148v-184h-80v216l172 172ZM480-80q-83 0-156-31.5T197-197q-54-54-85.5-127T80-480q0-83 31.5-156T197-763q54-54 127-85.5T480-880q83 0 156 31.5T763-763q54 54 85.5 127T880-480q0 83-31.5 156T763-197q-54 54-127 85.5T480-80Zm0-400Zm0 320q133 0 226.5-93.5T800-480q0-133-93.5-226.5T480-800q-133 0-226.5 93.5T160-480q0 133 93.5 226.5T480-160Z"/></svg>',Ge={"Content-Type":"application/json",Accept:"application/json"},Re=2e3,Ue=8,We=2e3,Je=8,je=3e3,Ye=750,Xe=20,Qe="/api/settings",tn=`${Qe}/time`,en="/api/runtime/time";async function B(t,e){const n=await fetch(t,{cache:"no-store",...e,headers:{...Ge,...e?.headers||{}}}),i=n.headers.get("content-type")||"",o=await n.text();if(!i.includes("application/json"))throw new Error(`Unexpected response for ${t}. Make sure you are connected to the device AP and loading the portal from the ESP32.`);const a=JSON.parse(o);if(!n.ok||a.success!==!0)throw new Error(a.message||"Request failed.");return a}const nn=(t,e)=>B(t,e),on=(t,e)=>B(t,e),rn=(t,e)=>B(t,e),Lt=(t,e)=>B(t,e),sn=(t,e)=>B(t,e);function b(t){const e=document.getElementById(t);if(!e)throw new Error(`Required portal element "#${t}" was not found.`);return e}function an(){return{followupLogoEl:b("followupLogo"),wifiStatusCard:b("wifiStatusCard"),wifiSettingsSheet:b("wifiSettingsSheet"),wifiSettingsCloseBtn:b("wifiSettingsCloseBtn"),wifiSettingsNotification:b("wifiSettingsNotification"),networkList:b("networkList"),passwordInput:b("password"),scanBtn:b("scanBtn"),connectBtn:b("connectBtn"),geminiCard:b("geminiCard"),geminiApiKeyInput:b("geminiApiKeyInput"),geminiClearBtn:b("geminiClearBtn"),geminiSaveBtn:b("geminiSaveBtn"),timezoneLocationCard:b("timezoneLocationCard"),timezoneSelect:b("timezoneSelect"),manualTimeInput:b("manualTimeInput"),manualDateInput:b("manualDateInput"),timezoneLocationClearBtn:b("timezoneLocationClearBtn"),timezoneLocationSaveBtn:b("timezoneLocationSaveBtn")}}function _t(t){return(e,n="info")=>{t.setNotification(e,n)}}function ln(t){return(e,n="info")=>{if(!e){t.hidden=!0,t.textContent="",t.removeAttribute("data-status"),t.setAttribute("role","status"),t.setAttribute("aria-live","polite");return}t.hidden=!1,t.textContent=e,t.setAttribute("data-status",n),n==="error"||n==="danger"?(t.setAttribute("role","alert"),t.setAttribute("aria-live","assertive")):n==="warning"?(t.setAttribute("role","alert"),t.setAttribute("aria-live","polite")):(t.setAttribute("role","status"),t.setAttribute("aria-live","polite"))}}function tt(t){t.classList.remove("error"),t.removeAttribute("invalid"),t.setAttribute("aria-invalid","false"),t.setCustomValidity?.("")}function et(t,e){t.classList.add("error"),t.setAttribute("invalid",""),t.setAttribute("aria-invalid","true"),t.setCustomValidity?.(e)}function cn(t,e){t.setAttribute("aria-checked",e?"true":"false")}function dn(t){return t.getAttribute("aria-checked")==="true"}function It(t,e){return t.includes("class=")?t.replace("<svg",`<svg class="${e}"`):t.replace("<svg",`<svg class="${e}"`)}function un(t,e,n){t.innerHTML=It(e,n)}function hn(t){return new Promise(e=>{setTimeout(e,t)})}function gn(){const t=document.documentElement.lang?.trim();return t||"en-US"}function fn(t){try{return new Intl.DateTimeFormat(gn(),{hour:"numeric",timeZone:t,timeZoneName:"longGeneric"}).formatToParts(new Date).find(i=>i.type==="timeZoneName")?.value?.trim()||""}catch(e){return console.warn(`Failed to format timezone label for "${t}".`,e),""}}function ct(t){return/^(GMT|UTC)(?:[+-]\d{1,2}(?::\d{2})?)?$/.test(t)}function mn(t){return(t.split("/").pop()||t).replace(/_/g," ")}function Mt(t){return t.split("/").map(e=>e.replace(/_/g," ")).join(" / ")}function pn(t){const e=t.map(o=>{const a=fn(o.name);return{cityLabel:mn(o.name),genericLabel:a,name:o.name}}),n=new Map(e.map(o=>[o.name,o])),i=new Map;return e.forEach(o=>{!o.genericLabel||ct(o.genericLabel)||i.set(o.genericLabel,(i.get(o.genericLabel)||0)+1)}),new Map(t.map(o=>{const a=n.get(o.name),l=o.description?.trim()||"";if(a?.genericLabel&&!ct(a.genericLabel)){const c=(i.get(a.genericLabel)||0)>1?`${a.genericLabel} (${a.cityLabel})`:a.genericLabel;return[o.name,c]}return l?[o.name,l]:[o.name,Mt(o.name)]}))}function bn(t,e){return e.get(t.name)||t.description?.trim()||Mt(t.name)}function vn(t){const e=t.value.trim();if(!e)return null;const n=/^(\d{2}):(\d{2})$/.exec(e);if(!n)return null;const i=Number.parseInt(n[1],10),o=Number.parseInt(n[2],10);return!Number.isFinite(i)||!Number.isFinite(o)||i<0||i>23||o<0||o>59?null:i*60+o}function P(t,e,n){t.hasKey=n?.has_key===!0,t.last4=typeof n?.last4=="string"?n.last4:"",e.value=t.hasKey?t.last4?`******${t.last4}`:"******":""}function dt(t,e,n){t.hasKey=!1,t.last4="",e.value="",n("")}async function ut(t,e,n,i,o,a,l){const d=e.value.trim();if(!d){n(`${l} is required.`,"warning");return}t.isBusy=!0,n(a,"info"),i();try{const c=await o(t.settingsApi,{method:"PATCH",body:JSON.stringify({api_key:d})});P(t,e,c.settings),n(c.message||`${l} stored.`,"success")}catch(c){console.error(`${l} save failed:`,c),n(c instanceof Error?c.message:`Failed to store ${l}.`,"error")}finally{t.isBusy=!1,i()}}async function ht(t,e,n,i,o,a,l){t.isBusy=!0,n(a,"info"),i();try{const d=await o(t.resetApi,{method:"POST"});P(t,e,d.settings),n(d.message||l,"success")}catch(d){console.error(`${l} failed:`,d),n(d instanceof Error?d.message:`Failed to clear ${l}.`,"error")}finally{t.isBusy=!1,i()}}function yn(t){const e={hasKey:!1,isBusy:!1,last4:"",resetApi:"/api/settings/gemini/reset",settingsApi:"/api/settings/gemini"},n={hasKey:!1,isBusy:!1,last4:"",resetApi:"/api/settings/openai/reset",settingsApi:"/api/settings/openai"};function i(v){const y=v?.routes;e.settingsApi=y?.settings||"/api/settings/gemini",e.resetApi=y?.reset||"/api/settings/gemini/reset"}function o(v){const y=v?.routes;n.settingsApi=y?.settings||"/api/settings/openai",n.resetApi=y?.reset||"/api/settings/openai/reset"}function a(v){P(e,t.geminiApiKeyInput,v)}function l(){dt(e,t.geminiApiKeyInput,t.notifyGemini)}function d(v){P(n,t.openAiApiKeyInput,v)}function c(){dt(n,t.openAiApiKeyInput,t.notifyOpenAi)}async function f(){if(!t.isGeminiModuleActive()||e.isBusy||e.hasKey)return;if(!t.geminiApiKeyInput.value.trim()){t.notifyGemini("Gemini API key is required.","warning");return}await ut(e,t.geminiApiKeyInput,t.notifyGemini,t.updateButtons,t.fetchGeminiModuleJson,"Saving Gemini API key...","Gemini API key")}async function p(){!t.isGeminiModuleActive()||e.isBusy||await ht(e,t.geminiApiKeyInput,t.notifyGemini,t.updateButtons,t.fetchGeminiModuleJson,"Clearing Gemini API key...","Gemini API key cleared.")}async function k(){if(!t.isOpenAiModuleActive()||n.isBusy||n.hasKey)return;if(!t.openAiApiKeyInput.value.trim()){t.notifyOpenAi("OpenAI API key is required.","warning");return}await ut(n,t.openAiApiKeyInput,t.notifyOpenAi,t.updateButtons,t.fetchOpenAiModuleJson,"Saving OpenAI API key...","OpenAI API key")}async function S(){!t.isOpenAiModuleActive()||n.isBusy||await ht(n,t.openAiApiKeyInput,t.notifyOpenAi,t.updateButtons,t.fetchOpenAiModuleJson,"Clearing OpenAI API key...","OpenAI API key cleared.")}return{applyGeminiSettings:a,applyOpenAiSettings:d,clearGeminiKey:p,clearGeminiSettings:l,clearOpenAiKey:S,clearOpenAiSettings:c,getGeminiHasKey:()=>e.hasKey,getOpenAiHasKey:()=>n.hasKey,isGeminiBusy:()=>e.isBusy,isOpenAiBusy:()=>n.isBusy,saveGeminiKey:f,saveOpenAiKey:k,updateGeminiRoutes:i,updateOpenAiRoutes:o}}function wn(t){let e=!1,n=!1,i=!1,o=!1;async function a(){return t.fetchTimezoneListJson("/api/timezone/list")}function l(s){const h=s.settings,m=s.runtime;t.setSwitchChecked(t.clockModeToggle,!!h?.enabled),t.timezoneSelect.value=h?.timezone_name||"",t.clearFieldError(t.timezoneSelect),t.manualDateInput.value=typeof m?.current_date=="string"?m.current_date:"",t.manualTimeInput.value=typeof m?.current_time=="string"?m.current_time:""}function d(s){s&&(typeof s.clock_enabled=="boolean"&&t.setSwitchChecked(t.clockModeToggle,s.clock_enabled),t.manualDateInput.value=typeof s.current_date=="string"?s.current_date:"",t.manualTimeInput.value=typeof s.current_time=="string"?s.current_time:"")}async function c(){t.timezoneSelect.innerHTML="";const s=document.createElement("option");s.value="",s.textContent="Select timezone",t.timezoneSelect.appendChild(s);try{const h=await a(),m=Array.isArray(h.timezones)?h.timezones:[],x=t.buildTimezoneLabelMap(m);m.map(C=>({value:C.name,label:t.formatTimezoneLabel(C,x)})).sort((C,A)=>C.label.localeCompare(A.label)).forEach(C=>{const A=document.createElement("option");A.value=C.value,A.textContent=C.label,t.timezoneSelect.appendChild(A)})}catch(h){console.error("Timezone list fetch failed:",h),t.notify(h instanceof Error?h.message:"Failed to load timezones.","error")}}async function f(){if(!(e||n||i)){e=!0,n=!0,i=!0,t.onStateChange();try{const s=await t.fetchTimeSettingsJson(t.timeSettingsApi);l(s)}catch(s){console.error("Time settings status failed:",s),t.notify(s instanceof Error?s.message:"Time settings status failed.","error")}finally{e=!1,n=!1,i=!1,t.onStateChange()}}}async function p(){if(!(i||o)){o=!0;try{for(let s=0;s<t.clockSyncPollAttempts;s++){const h=await t.fetchTimeRuntimeJson(t.timeRuntimeApi);if(d(h.runtime),h.runtime?.time_valid===!0&&typeof h.runtime.current_time=="string"&&h.runtime.current_time.length>0)return;s<t.clockSyncPollAttempts-1&&await new Promise(m=>{setTimeout(m,t.clockSyncPollIntervalMs)})}}catch(s){console.error("Clock refresh after WiFi connect failed:",s)}finally{o=!1}}}async function k(){if(!(document.hidden||e||n||i)&&t.isSwitchChecked(t.clockModeToggle))try{const s=await t.fetchTimeRuntimeJson(t.timeRuntimeApi);d(s.runtime)}catch(s){console.error("Time runtime status failed:",s)}}function S(){const s=t.timezoneSelect.value.trim(),h=t.manualDateInput.value.trim(),m=t.manualTimeInput.value.trim(),x=t.parseClockTimeInputValue(t.wakeupTimeInput),C=t.parseClockTimeInputValue(t.bedtimeTimeInput);return!!h!=!!m?(t.notify("Provide both current date and time for a manual clock set.","error"),null):{timezoneName:s,manualDate:h,manualTime:m,wakeupMinutes:x??void 0,bedtimeMinutes:C??void 0}}async function v(s,h){return t.fetchTimeSettingsJson(t.timeSettingsApi,{method:"PATCH",body:JSON.stringify({timezone_name:s.timezoneName,enabled:h,manual_date:s.manualDate,manual_time:s.manualTime})})}async function y(){if(i||e||n){t.notifyClockMode("Clock mode update already in progress.","warning");return}const s=t.isSwitchChecked(t.clockModeToggle),h=!s,m=t.timezoneSelect.value.trim();t.setSwitchChecked(t.clockModeToggle,h),i=!0,t.notifyClockMode(h?"Enabling clock mode...":"Disabling clock mode...","info"),t.onStateChange();try{const x=await t.fetchTimeSettingsJson(t.timeSettingsApi,{method:"PATCH",body:JSON.stringify({enabled:h,timezone_name:h&&m?m:void 0})});l(x),t.notifyClockMode(x.message||(h?"Clock mode enabled.":"Clock mode disabled."),"success")}catch(x){t.setSwitchChecked(t.clockModeToggle,s),console.error("Clock mode toggle failed:",x);const C=x instanceof Error?x.message:"Failed to update clock mode.",A=C==="timezone_name required to enable clock"?"Please set timezone first.":C;C==="timezone_name required to enable clock"&&(t.setFieldError(t.timezoneSelect,"Select a timezone."),t.timezoneSelect.focus({preventScroll:!0})),t.notifyClockMode(A,"error")}finally{i=!1,t.onStateChange()}}async function T(){if(e||n||i||t.isTalkingClockModuleBusy()){t.notify("Time configuration update already in progress.","warning");return}const s=S();if(s){if(!s.timezoneName){t.setFieldError(t.timezoneSelect,"Select a timezone."),t.notify("Select a timezone.","error"),t.timezoneSelect.focus({preventScroll:!0});return}if(t.clearFieldError(t.timezoneSelect),t.isTalkingClockModuleActive()&&(s.wakeupMinutes===void 0||s.bedtimeMinutes===void 0)){t.notify("Wakeup and bedtime times are required for the talking clock module.","error"),t.focusTalkingClockTimeInput();return}e=!0,n=!0,i=!0,t.onTalkingClockBusyChange(t.isTalkingClockModuleActive()),t.notify("Saving time configuration...","info"),t.onStateChange();try{const h=await v(s,t.isSwitchChecked(t.clockModeToggle));if(l(h),t.isTalkingClockModuleActive()){const m=await t.patchTalkingClockSettings({wakeup_minutes:s.wakeupMinutes,bedtime_minutes:s.bedtimeMinutes});t.applyTalkingClockModuleSettings(m.settings)}t.notify("Time configuration saved successfully.","success")}catch(h){console.error("Timezone/location save failed:",h),t.notify(h instanceof Error?h.message:"Failed to save time configuration.","error")}finally{e=!1,n=!1,i=!1,t.onTalkingClockBusyChange(!1),t.onStateChange()}}}async function E(){if(e||n||i){t.notify("Time configuration update already in progress.","warning");return}e=!0,n=!0,i=!0,t.notify("Clearing timezone/location...","info"),t.onStateChange();try{const s=await t.fetchTimeSettingsJson(t.timeSettingsApi,{method:"PATCH",body:JSON.stringify({enabled:!1,timezone_name:""})});l(s),t.notify("Timezone/location cleared successfully.","success")}catch(s){console.error("Timezone/location clear failed:",s),t.notify(s instanceof Error?s.message:"Failed to clear timezone/location.","error")}finally{e=!1,n=!1,i=!1,t.onStateChange()}}return{applyTimeRuntimeStatus:d,applyTimeSettingsStatus:l,clearTimezoneLocation:E,fetchTimeRuntimeStatus:k,fetchTimeSettingsStatus:f,isClockBusy:()=>i,isLocationBusy:()=>n,isTimezoneBusy:()=>e,populateTimezoneOptions:c,refreshClockStatusAfterWifiConnect:p,saveTimezoneLocation:T,toggleClockModeSetting:y}}function Cn(t,e){return t>=-50?e.fourBar:t>=-60?e.threeBar:t>=-70?e.twoBar:e.oneBar}function xn(t){let e=[],n="",i=!1,o="",a=!1,l=!1,d=!1,c=-1;function f(){t.wifiStatusCard.hidden=!1,t.wifiStatusCard.statusLabel=i?"Connected":"Disconnected",t.wifiStatusCard.network=i?o:""}function p(){const r=document.createElement("li");r.className="networks__list-empty-state",r.innerHTML=`${t.svgWithClass(t.wifiFindIcon,"wifi-find-icon")}<p>Scan for available networks</p>`,t.networkList.setEmptyState(r)}function k(){if(e.length===0){n="",c=-1;return}const r=e.findIndex(u=>u.ssid===n);if(r>=0){c=r;return}n="",c=-1}function S(){const r=t.networkList.getOptions();r.forEach((u,g)=>{u.setAttribute("aria-selected",String(n===e[g]?.ssid))}),c>=0&&c<r.length?t.networkList.setActiveDescendant(r[c].id):t.networkList.setActiveDescendant(null)}function v(){const r=e.find(u=>u.ssid===n);return r?!r.is_open:!0}function y(r){n=r,c=e.findIndex(u=>u.ssid===r),v()||t.clearFieldError(t.passwordInput),S(),t.onStateChange()}function T(){if(k(),t.networkList.clearItems(),e.length===0){p();return}e.forEach((r,u)=>{const g=document.createElement("li");g.className="networks__item",g.id=`network-option-${u}`,g.setAttribute("role","option"),g.setAttribute("tabindex","-1"),g.setAttribute("aria-selected",String(n===r.ssid));const O=document.createElement("span");O.className="networks__item-ssid",O.textContent=r.ssid;const I=document.createElement("div");if(I.className="networks__item-details",o===r.ssid&&i){const D=document.createElement("span");D.title="Currently connected",D.innerHTML=t.svgWithClass(t.checkIcon,"wifi-connected-icon"),I.appendChild(D)}const N=document.createElement("span");N.title=r.is_open?"Open":"Secured",r.is_open||(N.innerHTML=t.svgWithClass(t.securityIcon,"wifi-security-icon")),I.appendChild(N);const V=document.createElement("span");V.title=`Signal strength: ${r.signal_strength} (${r.rssi} dBm)`,V.innerHTML=t.svgWithClass(Cn(r.rssi,t.signalIcons),"wifi-signal-icon"),I.appendChild(V),g.append(O,I),g.addEventListener("click",()=>{c=u,y(r.ssid),t.networkList.focus({preventScroll:!0})}),t.networkList.appendItem(g)}),S()}function E(r){const u=i,g=o;i=r.connected===!0,o=typeof r.ssid=="string"?r.ssid:"",f(),(u!==i||g!==o)&&T(),t.onConnectionStateChange?.({wasConnected:u,previousNetwork:g,isCurrentlyConnected:i,connectedNetwork:o})}async function s(){if(!a){a=!0,t.notify("Scanning for networks...","info"),t.onStateChange();try{let r="Scanning for networks...";for(let u=0;u<t.scanPollAttempts;u++){const g=await t.fetchPortalJson("/api/scan");if(r=g.message||r,g.scan_in_progress===!0){if(t.notify(r,"info"),u<t.scanPollAttempts-1){await t.delayMs(t.scanPollIntervalMs);continue}}else{e=Array.isArray(g.networks)?g.networks:[],t.notify(r||"Scan complete.","success"),T();return}}throw new Error("Network scan timed out. Please try again.")}catch(r){console.error("Scan failed:",r),t.notify(r instanceof Error?r.message:"Scan failed. Please try again.","error"),e=[],T()}finally{a=!1,t.onStateChange()}}}async function h(){if(!l){l=!0,t.onStateChange();try{const r=await t.fetchPortalJson("/api/status");E(r),a||t.notify(r.message||"Status updated.",r.connected?"success":"info")}catch(r){console.error("Status check failed:",r),t.notify(r instanceof Error?r.message:"Status check failed.","error")}finally{l=!1,t.onStateChange()}}}async function m(){if(!(d||!n)){if(v()&&t.passwordInput.value.trim().length===0){t.setFieldError(t.passwordInput,"Please enter a WiFi password."),t.passwordInput.focus({preventScroll:!0});return}t.clearFieldError(t.passwordInput),d=!0,t.notify(`Connecting to ${n}...`,"info"),t.onStateChange();try{const r=await t.fetchPortalJson("/api/configure",{method:"POST",body:JSON.stringify({ssid:n,password:t.passwordInput.value})});t.notify(r.message||"Connection request sent.","info");for(let u=0;u<t.statusPollAttempts;u++){await t.delayMs(t.statusPollIntervalMs);const g=await t.fetchPortalJson("/api/status");if(E(g),g.connected){t.notify(g.message||"Connected.","success");break}}i||t.notify("Connection in progress...","info")}catch(r){console.error("Connection failed:",r),t.notify(r instanceof Error?r.message:"Connection failed.","error")}finally{d=!1,t.onStateChange()}}}async function x(){if(!(d||l||!i)){l=!0,t.notify("Disconnecting...","info"),t.onStateChange();try{const r=await t.fetchPortalJson("/api/disconnect",{method:"POST"}),u=i,g=o;i=!1,o="",f(),T(),t.notify(r.message||"Disconnected.","success"),t.onConnectionStateChange?.({wasConnected:u,previousNetwork:g,isCurrentlyConnected:i,connectedNetwork:o})}catch(r){console.error("Disconnect failed:",r),t.notify(r instanceof Error?r.message:"Disconnect failed.","error")}finally{l=!1,t.onStateChange()}}}function C(){if(it()){x();return}if(d||l){t.notify("WiFi status is busy. Please wait.","warning");return}if(!n){t.notify("Select a WiFi network first.","error"),t.networkList.focus({preventScroll:!0});return}if(v()&&t.passwordInput.value.trim().length===0){t.setFieldError(t.passwordInput,"Please enter a WiFi password."),t.notify("Please check if your password is correct.","error"),t.passwordInput.focus({preventScroll:!0});return}t.clearFieldError(t.passwordInput),m()}function A(){t.passwordInput.value?.trim().length&&t.clearFieldError(t.passwordInput),t.onStateChange()}function Pt(r){if(e.length!==0)switch(k(),r.key){case"Tab":break;case"ArrowDown":{r.preventDefault();const u=c<e.length-1?c+1:0;c=u,y(e[u].ssid),t.networkList.scrollOptionIntoView(u);break}case"ArrowUp":{r.preventDefault();const u=c>0?c-1:e.length-1;c=u,y(e[u].ssid),t.networkList.scrollOptionIntoView(u);break}case"Home":{r.preventDefault(),c=0,y(e[0].ssid),t.networkList.scrollOptionIntoView(0);break}case"End":{r.preventDefault();const u=e.length-1;c=u,y(e[u].ssid),t.networkList.scrollOptionIntoView(u);break}case"Enter":case" ":{r.preventDefault(),c>=0&&c<e.length&&y(e[c].ssid);break}}}function it(){return i&&o.length>0&&n===o}return{applyPortalStatus:E,checkStatus:h,connect:m,disconnect:x,getConnectedNetwork:()=>o,getSelectedNetwork:()=>n,handleConnectAction:C,handleListboxKeyDown:Pt,handlePasswordInput:A,isCheckingStatus:()=>l,isConnecting:()=>d,isCurrentlyConnected:()=>i,isScanning:()=>a,isSelectedConnected:it,renderNetworkList:T,requiresPassword:v,scanNetworks:s,updateWifiStatusCard:f}}function z(t,e){e().finally(()=>{!t.disabled&&t.offsetParent!==null&&setTimeout(()=>{!t.disabled&&t.offsetParent!==null&&t.focus()},0)})}function Sn(t){const{controllers:e,dom:n,helpers:i}=t;function o(){n.wifiSettingsSheet.openBottomSheet(),e.wifiController.scanNetworks()}n.scanBtn.addEventListener("click",()=>{e.wifiController.scanNetworks()}),n.connectBtn.addEventListener("click",()=>{e.wifiController.handleConnectAction()}),n.wifiStatusCard.addEventListener("action",()=>{if(e.wifiController.isCurrentlyConnected()){e.wifiController.disconnect();return}o()}),n.wifiStatusCard.addEventListener("settings",()=>{o()}),n.wifiSettingsCloseBtn.addEventListener("click",()=>{n.wifiSettingsSheet.closeBottomSheet()}),n.passwordInput.addEventListener("input",()=>{e.wifiController.handlePasswordInput()}),n.networkList.addEventListener("keydown",a=>{e.wifiController.handleListboxKeyDown(a)}),n.geminiSaveBtn.addEventListener("click",()=>{z(n.geminiSaveBtn,()=>e.geminiController.saveGeminiKey())}),n.geminiClearBtn.addEventListener("click",()=>{z(n.geminiClearBtn,()=>e.geminiController.clearGeminiKey())}),n.geminiApiKeyInput.addEventListener("input",i.updateUi),n.timezoneLocationSaveBtn.addEventListener("click",()=>{if(!n.timezoneSelect.value.trim()){i.setFieldError(n.timezoneSelect,"Select a timezone."),i.setTimezoneLocationNotification("Select a timezone.","error"),n.timezoneSelect.focus({preventScroll:!0});return}i.clearFieldError(n.timezoneSelect),z(n.timezoneLocationSaveBtn,()=>e.timeController.saveTimezoneLocation())}),n.timezoneLocationClearBtn.addEventListener("click",()=>{z(n.timezoneLocationClearBtn,()=>e.timeController.clearTimezoneLocation())}),n.timezoneSelect.addEventListener("change",()=>{n.timezoneSelect.value?.trim().length&&i.clearFieldError(n.timezoneSelect),i.updateUi()}),n.manualDateInput.addEventListener("input",i.updateUi),n.manualTimeInput.addEventListener("input",i.updateUi)}function Tn(t){const{controllers:e,dom:n}=t,i=e.wifiController.isScanning()||e.wifiController.isConnecting()||e.wifiController.isCheckingStatus(),o=e.wifiController.isSelectedConnected(),a=e.wifiController.getSelectedNetwork().trim(),l=a.length>0&&!o&&e.wifiController.requiresPassword(),d=o||a.length>0&&(!l||n.passwordInput.value.trim().length>0);n.scanBtn.disabled=i,n.connectBtn.disabled=i||!d,n.wifiStatusCard.actionDisabled=i,n.passwordInput.disabled=e.wifiController.isConnecting(),e.wifiController.isConnecting()?(n.connectBtn.textContent="Connecting...",n.wifiStatusCard.actionLabel="Connecting..."):o?(n.connectBtn.textContent="Disconnect",n.wifiStatusCard.actionLabel="Disconnect"):(n.connectBtn.textContent="Connect",n.wifiStatusCard.actionLabel=e.wifiController.isCurrentlyConnected()?"Disconnect":"Connect");const c=e.timeController.isTimezoneBusy()||e.timeController.isLocationBusy()||e.timeController.isClockBusy();n.timezoneSelect.disabled=c,n.manualDateInput.disabled=c,n.manualTimeInput.disabled=c,n.timezoneLocationSaveBtn.disabled=c||n.timezoneSelect.value.trim().length===0,n.timezoneLocationClearBtn.disabled=c||n.timezoneSelect.value.trim().length===0;const f=e.geminiController.isGeminiBusy(),p=e.geminiController.getGeminiHasKey();n.geminiApiKeyInput.readOnly=p||f,n.geminiApiKeyInput.disabled=f,n.geminiSaveBtn.disabled=f||p||n.geminiApiKeyInput.value.trim().length===0,n.geminiSaveBtn.hidden=p,n.geminiClearBtn.disabled=f||!p,n.geminiClearBtn.hidden=!p}Jt();Zt();Qt();se();fe();be();Ce();Ee();ze();let Q=null;const w=an(),Bt=ln(w.wifiSettingsNotification),kn=_t(w.geminiCard),zt=_t(w.timezoneLocationCard),qt=()=>document.createElement("input"),An=document.createElement("button"),En=qt(),Ln=qt(),_n=document.createElement("input");function L(){Tn({controllers:{geminiController:nt,timeController:_,wifiController:M},dom:w})}const nt=yn({fetchGeminiModuleJson:Lt,fetchOpenAiModuleJson:()=>Promise.resolve({}),geminiApiKeyInput:w.geminiApiKeyInput,isGeminiModuleActive:()=>!0,isOpenAiModuleActive:()=>!1,notifyGemini:kn,notifyOpenAi:()=>{},openAiApiKeyInput:_n,updateButtons:L}),_=wn({applyTalkingClockModuleSettings:()=>{},bedtimeTimeInput:Ln,clearFieldError:tt,clockModeToggle:An,clockSyncPollAttempts:Je,clockSyncPollIntervalMs:We,fetchTimeRuntimeJson:rn,fetchTimeSettingsJson:on,fetchTimezoneListJson:sn,focusTalkingClockTimeInput:()=>{},buildTimezoneLabelMap:pn,formatTimezoneLabel:bn,isSwitchChecked:dn,isTalkingClockModuleActive:()=>!1,isTalkingClockModuleBusy:()=>!1,manualDateInput:w.manualDateInput,manualTimeInput:w.manualTimeInput,notifyClockMode:()=>{},notify:zt,onStateChange:L,onTalkingClockBusyChange:()=>{},parseClockTimeInputValue:vn,patchTalkingClockSettings:()=>Promise.resolve({success:!0}),setFieldError:et,setSwitchChecked:cn,timeRuntimeApi:en,timeSettingsApi:tn,timezoneSelect:w.timezoneSelect,wakeupTimeInput:En}),M=xn({checkIcon:Fe,clearFieldError:tt,delayMs:hn,fetchPortalJson:nn,networkList:w.networkList,notify:Bt,onConnectionStateChange:({wasConnected:t,isCurrentlyConnected:e})=>{!t&&e&&_.refreshClockStatusAfterWifiConnect()},onStateChange:L,passwordInput:w.passwordInput,scanPollAttempts:Xe,scanPollIntervalMs:Ye,securityIcon:Oe,setFieldError:et,signalIcons:{oneBar:Ne,twoBar:Ve,threeBar:De,fourBar:He},statusPollAttempts:Ue,statusPollIntervalMs:Re,svgWithClass:It,wifiFindIcon:$e,wifiStatusCard:w.wifiStatusCard});function q(){if(!Q)return;const t=document.documentElement,e=t.scrollHeight>window.innerHeight+1,n=window.scrollY+window.innerHeight>=t.scrollHeight-1;Q.setVisible(e&&!n)}function In(){un(w.followupLogoEl,qe,"followup-logo"),w.wifiStatusCard.iconSvg=Pe,w.geminiCard.iconSvg=Ke,w.timezoneLocationCard.iconSvg=Ze,M.renderNetworkList(),document.body&&(Q=Dt({mount:document.body,mode:"fixed",position:"bottom",height:"6rem",width:"100vw",strength:1.8,divCount:5,curve:"bezier",exponential:!0,opacity:1,zIndex:20,className:"page-gradual-blur",fallbackColor:"var(--color-bg-selected)"}),q(),window.addEventListener("scroll",q,{passive:!0}),window.addEventListener("resize",q),typeof ResizeObserver<"u"&&new ResizeObserver(()=>{q()}).observe(document.body)),L(),M.updateWifiStatusCard(),Bt(""),Sn({controllers:{geminiController:nt,timeController:_,wifiController:M},dom:w,helpers:{clearFieldError:tt,setFieldError:et,setTimezoneLocationNotification:zt,updateUi:L}}),Mn(),window.setInterval(()=>{_.fetchTimeRuntimeStatus()},je)}async function Mn(){await Promise.allSettled([_.populateTimezoneOptions(),_.fetchTimeSettingsStatus(),M.checkStatus(),Bn()])}async function Bn(){try{const t=await Lt("/api/settings/gemini");nt.applyGeminiSettings(t.settings)}catch(t){console.error("Gemini settings status failed:",t)}finally{L()}}In();
