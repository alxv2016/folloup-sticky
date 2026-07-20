export type GradualBlurPosition = 'top' | 'bottom' | 'left' | 'right';
export type GradualBlurCurve =
  | 'linear'
  | 'bezier'
  | 'ease-in'
  | 'ease-out'
  | 'ease-in-out';

export interface GradualBlurOptions {
  mount: HTMLElement;
  mode?: 'absolute' | 'fixed';
  position?: GradualBlurPosition;
  height?: string;
  width?: string;
  strength?: number;
  divCount?: number;
  exponential?: boolean;
  opacity?: number;
  curve?: GradualBlurCurve;
  zIndex?: number;
  className?: string;
  fallbackColor?: string;
}

interface GradualBlurHandle {
  element: HTMLDivElement;
  setVisible: (visible: boolean) => void;
  destroy: () => void;
}

const DEFAULTS: Required<
  Omit<GradualBlurOptions, 'mount' | 'width' | 'className' | 'fallbackColor'>
> = {
  mode: 'absolute',
  position: 'bottom',
  height: '6rem',
  strength: 2,
  divCount: 5,
  exponential: false,
  opacity: 1,
  curve: 'linear',
  zIndex: 2,
};

const CURVE_FUNCTIONS: Record<GradualBlurCurve, (progress: number) => number> = {
  linear: progress => progress,
  bezier: progress => progress * progress * (3 - 2 * progress),
  'ease-in': progress => progress * progress,
  'ease-out': progress => 1 - Math.pow(1 - progress, 2),
  'ease-in-out': progress =>
    progress < 0.5
      ? 2 * progress * progress
      : 1 - Math.pow(-2 * progress + 2, 2) / 2,
};

function getGradientDirection(position: GradualBlurPosition): string {
  switch (position) {
    case 'top':
      return 'to top';
    case 'left':
      return 'to left';
    case 'right':
      return 'to right';
    default:
      return 'to bottom';
  }
}

function supportsBlurMask(): boolean {
  if (typeof CSS === 'undefined' || typeof CSS.supports !== 'function') {
    return false;
  }

  const supportsBlur =
    CSS.supports('backdrop-filter', 'blur(1px)') ||
    CSS.supports('-webkit-backdrop-filter', 'blur(1px)');
  const supportsMask =
    CSS.supports('mask-image', 'linear-gradient(to bottom, transparent, black)') ||
    CSS.supports('-webkit-mask-image', 'linear-gradient(to bottom, transparent, black)');

  return supportsBlur && supportsMask;
}

export function createGradualBlur(options: GradualBlurOptions): GradualBlurHandle {
  const config = { ...DEFAULTS, ...options };
  const root = document.createElement('div');
  const inner = document.createElement('div');
  const direction = getGradientDirection(config.position);
  const canUseBackdropBlur = supportsBlurMask();
  const isVertical = config.position === 'top' || config.position === 'bottom';

  root.className = 'gradual-blur';
  if (config.className) {
    root.classList.add(config.className);
  }
  if (!canUseBackdropBlur) {
    root.classList.add('gradual-blur--fallback');
    root.style.background = `linear-gradient(${direction}, transparent 0%, ${config.fallbackColor || 'rgba(255, 255, 255, 0.95)'} 100%)`;
  }

  root.style.position = config.mode;
  root.style.pointerEvents = 'none';
  root.style.zIndex = String(config.zIndex);
  root.style.opacity = '0';
  root.style.borderRadius = 'inherit';

  if (isVertical) {
    root.style.height = config.height;
    root.style.width = config.width || '100%';
    root.style[config.position] = '0';
    if (config.mode === 'fixed') {
      root.style.left = '50%';
      root.style.right = 'auto';
      root.style.transform = 'translateX(-50%)';
    } else {
      root.style.left = '0';
      root.style.right = '0';
    }
  } else {
    root.style.width = config.width || config.height;
    root.style.height = '100%';
    root.style.top = '0';
    root.style.bottom = '0';
    root.style[config.position] = '0';
  }

  inner.className = 'gradual-blur__inner';
  root.appendChild(inner);

  if (canUseBackdropBlur) {
    const increment = 100 / config.divCount;
    const curveFunction = CURVE_FUNCTIONS[config.curve] || CURVE_FUNCTIONS.linear;

    for (let index = 1; index <= config.divCount; index += 1) {
      const layer = document.createElement('div');
      const progress = curveFunction(index / config.divCount);
      let blurValue = 0;

      if (config.exponential) {
        blurValue = Math.pow(2, progress * 4) * 0.0625 * config.strength;
      } else {
        blurValue = 0.0625 * (progress * config.divCount + 1) * config.strength;
      }

      const p1 = Math.round((increment * index - increment) * 10) / 10;
      const p2 = Math.round(increment * index * 10) / 10;
      const p3 = Math.round((increment * index + increment) * 10) / 10;
      const p4 = Math.round((increment * index + increment * 2) * 10) / 10;

      let gradient = `transparent ${p1}%, black ${p2}%`;
      if (p3 <= 100) {
        gradient += `, black ${p3}%`;
      }
      if (p4 <= 100) {
        gradient += `, transparent ${p4}%`;
      }

      layer.className = 'gradual-blur__layer';
      layer.style.maskImage = `linear-gradient(${direction}, ${gradient})`;
      layer.style.webkitMaskImage = `linear-gradient(${direction}, ${gradient})`;
      layer.style.backdropFilter = `blur(${blurValue.toFixed(3)}rem)`;
      layer.style.setProperty(
        '-webkit-backdrop-filter',
        `blur(${blurValue.toFixed(3)}rem)`
      );
      layer.style.opacity = String(config.opacity);
      inner.appendChild(layer);
    }
  }

  if (window.getComputedStyle(config.mount).position === 'static') {
    config.mount.style.position = 'relative';
  }

  config.mount.appendChild(root);

  return {
    element: root,
    setVisible(visible: boolean) {
      root.style.opacity = visible ? '1' : '0';
    },
    destroy() {
      root.remove();
    },
  };
}
