interface ImportMetaEnv {
  readonly VITE_PORTAL_DEMO?: string;
}

interface ImportMeta {
  readonly env: ImportMetaEnv;
}

declare module 'non-typed-module';

declare module '*.svg' {
  const content: string;
  export default content;
}

declare module '*.css' {
  const content: string;
  export default content;
}

declare module '*.css?raw' {
  const content: string;
  export default content;
}

declare module '*.svg?raw' {
  const content: string;
  export default content;
}
