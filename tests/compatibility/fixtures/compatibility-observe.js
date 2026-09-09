(() => {
  'use strict';

  const parameters = new URLSearchParams(window.location.search);
  const harnessToken = parameters.get('__ob_token') || '';
  const observerUrl = parameters.get('__ob_observer') || '';

  const canonicalUrl = new URL(window.location.href);
  canonicalUrl.searchParams.delete('__ob_token');
  canonicalUrl.searchParams.delete('__ob_observer');

  const domMarkers = {};
  document.querySelectorAll('[data-compatibility-marker]').forEach((element, index) => {
    const key = element.id || `marker-${index}`;
    domMarkers[key] = {
      marker: element.getAttribute('data-compatibility-marker') || '',
      text: (element.textContent || '').trim().replace(/\s+/g, ' ')
    };
  });

  const storage = {};
  for (let index = 0; index < localStorage.length; ++index) {
    const key = localStorage.key(index);
    if (key !== null) {
      storage[key] = localStorage.getItem(key);
    }
  }

  const sessionStorageResult = {};
  for (let index = 0; index < sessionStorage.length; ++index) {
    const key = sessionStorage.key(index);
    if (key !== null) {
      sessionStorageResult[key] = sessionStorage.getItem(key);
    }
  }

  const cookies = document.cookie
    .split(';')
    .map((value) => value.trim())
    .filter((value) => value.length > 0)
    .sort();

  const customEvents = Array.isArray(window.__openbrowserCompatibilityEvents)
    ? window.__openbrowserCompatibilityEvents.filter((value) => typeof value === 'string')
    : [];

  const observation = {
    schema_version: 1,
    scenario_id: document.documentElement.dataset.compatibilityScenario || '',
    final_url: canonicalUrl.href,
    title: document.title,
    dom_markers: domMarkers,
    events: ['navigation_committed', ...customEvents, 'fixture_ready'],
    storage,
    session_storage: sessionStorageResult,
    cookies
  };

  if (observerUrl) {
    const observerRequest = new XMLHttpRequest();
    observerRequest.open('POST', observerUrl, false);
    observerRequest.setRequestHeader('Content-Type', 'text/plain;charset=UTF-8');
    observerRequest.send(JSON.stringify(observation));
    if (observerRequest.status < 200 || observerRequest.status >= 300) {
      throw new Error(`Compatibility observer rejected observation: ${observerRequest.status}`);
    }
  }

  const readyRequest = new XMLHttpRequest();
  readyRequest.open(
    'GET',
    '/__compatibility__/ready?token=' + encodeURIComponent(harnessToken),
    false
  );
  readyRequest.send(null);
  if (readyRequest.status < 200 || readyRequest.status >= 300) {
    throw new Error(`Compatibility harness readiness failed: ${readyRequest.status}`);
  }
})();
