import type {
  GeminiModuleResponse,
  GeminiModuleSettings,
  ModuleStatusResponse,
  OpenAiModuleResponse,
  OpenAiModuleSettings,
  StatusType,
  ValidatableField,
} from './types';

type ProviderInput = ValidatableField & { readOnly: boolean };
type ProviderSettings = { has_key?: boolean; last4?: string };
type ProviderResponse = {
  message?: string;
  settings?: ProviderSettings;
};

interface ProviderState {
  hasKey: boolean;
  isBusy: boolean;
  last4: string;
  resetApi: string;
  settingsApi: string;
}

interface ProviderKeysDeps {
  fetchGeminiModuleJson: (
    path: string,
    init?: RequestInit
  ) => Promise<GeminiModuleResponse>;
  fetchOpenAiModuleJson: (
    path: string,
    init?: RequestInit
  ) => Promise<OpenAiModuleResponse>;
  geminiApiKeyInput: ProviderInput;
  isGeminiModuleActive: () => boolean;
  isOpenAiModuleActive: () => boolean;
  notifyGemini: (message: string, type?: StatusType) => void;
  notifyOpenAi: (message: string, type?: StatusType) => void;
  openAiApiKeyInput: ProviderInput;
  updateButtons: () => void;
}

function applyProviderSettings(
  state: ProviderState,
  input: ProviderInput,
  settings?: ProviderSettings
) {
  state.hasKey = settings?.has_key === true;
  state.last4 = typeof settings?.last4 === 'string' ? settings.last4 : '';
  input.value = state.hasKey
    ? state.last4
      ? `******${state.last4}`
      : '******'
    : '';
}

function clearProviderSettings(
  state: ProviderState,
  input: ProviderInput,
  notify: (message: string, type?: StatusType) => void
) {
  state.hasKey = false;
  state.last4 = '';
  input.value = '';
  notify('');
}

async function saveProviderKey(
  state: ProviderState,
  input: ProviderInput,
  notify: (message: string, type?: StatusType) => void,
  updateButtons: () => void,
  fetchJson: (path: string, init?: RequestInit) => Promise<ProviderResponse>,
  progressMessage: string,
  successMessage: string
) {
  const apiKey = input.value.trim();
  if (!apiKey) {
    notify(`${successMessage} is required.`, 'warning');
    return;
  }

  state.isBusy = true;
  notify(progressMessage, 'info');
  updateButtons();

  try {
    const data = await fetchJson(state.settingsApi, {
      method: 'PATCH',
      body: JSON.stringify({ api_key: apiKey }),
    });
    applyProviderSettings(state, input, data.settings);
    notify(data.message || `${successMessage} stored.`, 'success');
  } catch (error) {
    console.error(`${successMessage} save failed:`, error);
    notify(
      error instanceof Error ? error.message : `Failed to store ${successMessage}.`,
      'error'
    );
  } finally {
    state.isBusy = false;
    updateButtons();
  }
}

async function clearProviderKey(
  state: ProviderState,
  input: ProviderInput,
  notify: (message: string, type?: StatusType) => void,
  updateButtons: () => void,
  fetchJson: (path: string, init?: RequestInit) => Promise<ProviderResponse>,
  progressMessage: string,
  successMessage: string
) {
  state.isBusy = true;
  notify(progressMessage, 'info');
  updateButtons();

  try {
    const data = await fetchJson(state.resetApi, {
      method: 'POST',
    });
    applyProviderSettings(state, input, data.settings);
    notify(data.message || successMessage, 'success');
  } catch (error) {
    console.error(`${successMessage} failed:`, error);
    notify(
      error instanceof Error ? error.message : `Failed to clear ${successMessage}.`,
      'error'
    );
  } finally {
    state.isBusy = false;
    updateButtons();
  }
}

export function createProviderKeysController(deps: ProviderKeysDeps) {
  const geminiState: ProviderState = {
    hasKey: false,
    isBusy: false,
    last4: '',
    resetApi: '/api/settings/gemini/reset',
    settingsApi: '/api/settings/gemini',
  };

  const openAiState: ProviderState = {
    hasKey: false,
    isBusy: false,
    last4: '',
    resetApi: '/api/settings/openai/reset',
    settingsApi: '/api/settings/openai',
  };

  function updateGeminiRoutes(module?: ModuleStatusResponse) {
    const routes = module?.routes;
    geminiState.settingsApi = routes?.settings || '/api/settings/gemini';
    geminiState.resetApi = routes?.reset || '/api/settings/gemini/reset';
  }

  function updateOpenAiRoutes(module?: ModuleStatusResponse) {
    const routes = module?.routes;
    openAiState.settingsApi = routes?.settings || '/api/settings/openai';
    openAiState.resetApi = routes?.reset || '/api/settings/openai/reset';
  }

  function applyGeminiSettings(settings?: GeminiModuleSettings) {
    applyProviderSettings(geminiState, deps.geminiApiKeyInput, settings);
  }

  function clearGeminiSettings() {
    clearProviderSettings(geminiState, deps.geminiApiKeyInput, deps.notifyGemini);
  }

  function applyOpenAiSettings(settings?: OpenAiModuleSettings) {
    applyProviderSettings(openAiState, deps.openAiApiKeyInput, settings);
  }

  function clearOpenAiSettings() {
    clearProviderSettings(openAiState, deps.openAiApiKeyInput, deps.notifyOpenAi);
  }

  async function saveGeminiKey() {
    if (!deps.isGeminiModuleActive() || geminiState.isBusy || geminiState.hasKey) {
      return;
    }

    const apiKey = deps.geminiApiKeyInput.value.trim();
    if (!apiKey) {
      deps.notifyGemini('Gemini API key is required.', 'warning');
      return;
    }

    await saveProviderKey(
      geminiState,
      deps.geminiApiKeyInput,
      deps.notifyGemini,
      deps.updateButtons,
      deps.fetchGeminiModuleJson,
      'Saving Gemini API key...',
      'Gemini API key'
    );
  }

  async function clearGeminiKey() {
    if (!deps.isGeminiModuleActive() || geminiState.isBusy) {
      return;
    }

    await clearProviderKey(
      geminiState,
      deps.geminiApiKeyInput,
      deps.notifyGemini,
      deps.updateButtons,
      deps.fetchGeminiModuleJson,
      'Clearing Gemini API key...',
      'Gemini API key cleared.'
    );
  }

  async function saveOpenAiKey() {
    if (!deps.isOpenAiModuleActive() || openAiState.isBusy || openAiState.hasKey) {
      return;
    }

    const apiKey = deps.openAiApiKeyInput.value.trim();
    if (!apiKey) {
      deps.notifyOpenAi('OpenAI API key is required.', 'warning');
      return;
    }

    await saveProviderKey(
      openAiState,
      deps.openAiApiKeyInput,
      deps.notifyOpenAi,
      deps.updateButtons,
      deps.fetchOpenAiModuleJson,
      'Saving OpenAI API key...',
      'OpenAI API key'
    );
  }

  async function clearOpenAiKey() {
    if (!deps.isOpenAiModuleActive() || openAiState.isBusy) {
      return;
    }

    await clearProviderKey(
      openAiState,
      deps.openAiApiKeyInput,
      deps.notifyOpenAi,
      deps.updateButtons,
      deps.fetchOpenAiModuleJson,
      'Clearing OpenAI API key...',
      'OpenAI API key cleared.'
    );
  }

  return {
    applyGeminiSettings,
    applyOpenAiSettings,
    clearGeminiKey,
    clearGeminiSettings,
    clearOpenAiKey,
    clearOpenAiSettings,
    getGeminiHasKey: () => geminiState.hasKey,
    getOpenAiHasKey: () => openAiState.hasKey,
    isGeminiBusy: () => geminiState.isBusy,
    isOpenAiBusy: () => openAiState.isBusy,
    saveGeminiKey,
    saveOpenAiKey,
    updateGeminiRoutes,
    updateOpenAiRoutes,
  };
}
