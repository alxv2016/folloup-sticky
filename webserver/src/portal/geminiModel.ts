import { fetchGeminiModuleJson } from './api';
import type { PortalDom } from './dom';
import { createCardNotifier } from './uiHelpers';

export function createGeminiModelController(dom: PortalDom) {
  const select = dom.geminiModelSelect;
  const custom = dom.geminiCustomModel;
  const save = dom.geminiModelSaveBtn;
  const notify = createCardNotifier(dom.geminiModelCard);
  let savedModel = '';
  let busy = false;

  function modelName() {
    const value = select.value === 'custom' ? custom.value.trim() : select.value;
    const id = value.replace(/^models\//, '');
    return /^gemini-[a-z0-9.-]{1,93}$/.test(id) ? `models/${id}` : '';
  }

  function update() {
    const model = modelName();
    dom.geminiCustomModelField.hidden = select.value !== 'custom';
    select.disabled = !savedModel || busy;
    custom.disabled = busy;
    save.disabled = !savedModel || busy || !model || model === savedModel;
  }

  function apply(model?: string) {
    if (!model) return;
    savedModel = model;
    select.value = model;
    if (!select.value) {
      select.value = 'custom';
      custom.value = model;
    }
    update();
  }

  select.addEventListener('change', update);
  custom.addEventListener('input', update);
  save.addEventListener('click', async () => {
    if (save.disabled) return;
    const model = modelName();
    busy = true;
    update();
    notify('Saving Gemini model…', 'info');
    try {
      const data = await fetchGeminiModuleJson('/api/settings/gemini', {
        method: 'PATCH',
        body: JSON.stringify({ model_name: model }),
      });
      apply(data.settings?.model_name ?? model);
      notify('Model saved. Record a short clip to verify it.', 'success');
    } catch (error) {
      notify(error instanceof Error ? error.message : 'Failed to save Gemini model.', 'error');
    } finally {
      busy = false;
      update();
    }
  });

  return { apply };
}
