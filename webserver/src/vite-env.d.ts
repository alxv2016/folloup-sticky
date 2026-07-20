/// <reference types="vite/client" />

declare module '*.svg?react' {
  import React from 'react';
  const Component: React.FunctionComponent<React.SVGProps<SVGSVGElement>>;
  export default Component;
}
