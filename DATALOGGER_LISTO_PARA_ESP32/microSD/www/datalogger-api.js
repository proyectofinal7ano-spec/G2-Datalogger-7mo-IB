/* Cliente HTTP del DataLogger ESP32. Todas las rutas son del mismo dispositivo. */
(function () {
  const API_TIMEOUT_MS = 20000;

  async function request(path, options = {}) {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), API_TIMEOUT_MS);

    try {
      const response = await fetch(path, {
        cache: 'no-store',
        ...options,
        signal: controller.signal,
        headers: {
          Accept: 'application/json',
          ...(options.headers || {})
        }
      });

      if (!response.ok) {
        const detail = await response.text();
        throw new Error(detail || `El DataLogger respondio con HTTP ${response.status}`);
      }

      return response.json();
    } catch (error) {
      if (error.name === 'AbortError') {
        throw new Error('El DataLogger no respondio dentro del tiempo esperado.');
      }
      throw error;
    } finally {
      clearTimeout(timeout);
    }
  }

  function measurement(item) {
    const result = {
      created_at: item?.created_at || '',
      temperatura: Number(item?.temperatura),
      humedad: Number(item?.humedad),
      presion: Number(item?.presion),
      estado: item?.estado || 'Normal'
    };

    return Number.isFinite(result.temperatura) &&
      Number.isFinite(result.humedad) &&
      Number.isFinite(result.presion)
      ? result
      : null;
  }

  async function obtenerMedicionActual() {
    const data = await request('/api/actual');
    return measurement(data);
  }

  async function obtenerHistorialDatalogger(days = 1) {
    const safeDays = Math.max(1, Math.min(365, Number(days) || 1));
    const data = await request(`/api/historial?days=${safeDays}`);
    if (!Array.isArray(data)) return [];
    return data.map(measurement).filter(Boolean);
  }

  function obtenerEstadoDatalogger() {
    return request('/api/status');
  }

  function obtenerAlmacenamientoDatalogger() {
    return request('/api/storage');
  }

  function obtenerConfiguracionDatalogger() {
    return request('/api/config');
  }

  function guardarConfiguracionDatalogger({ samplingIntervalMs, retentionDays }) {
    const body = new URLSearchParams({
      sampling_ms: String(samplingIntervalMs),
      retention_days: String(retentionDays)
    });

    return request('/api/config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8' },
      body
    });
  }

  function reiniciarDatalogger() {
    return request('/api/reiniciar', { method: 'POST' });
  }

  function descargarCSV(days = 30) {
    const safeDays = Math.max(1, Math.min(365, Number(days) || 30));
    window.location.href = `/api/export?days=${safeDays}`;
  }

  window.DataLoggerAPI = {
    obtenerMedicionActual,
    obtenerHistorialDatalogger,
    obtenerEstadoDatalogger,
    obtenerAlmacenamientoDatalogger,
    obtenerConfiguracionDatalogger,
    guardarConfiguracionDatalogger,
    reiniciarDatalogger,
    descargarCSV
  };
})();
