(function () {
  let readings = [];

  function renderSummary(item) {
    if (!item) return;
    document.getElementById('temperatura').textContent = item.temperatura.toFixed(1);
    document.getElementById('humedad').textContent = item.humedad.toFixed(1);
    document.getElementById('presion').textContent = Math.round(item.presion).toLocaleString('es-AR');
    document.getElementById('estado').textContent = item.estado || 'Normal';
  }

  function renderTable() {
    const table = document.getElementById('tablaMediciones');
    if (!readings.length) {
      table.innerHTML = '<tr><td colspan="5">No hay mediciones almacenadas.</td></tr>';
      return;
    }

    table.innerHTML = readings.slice(-20).reverse().map(item => `<tr>
      <td>${formatArgentinaDateTime(new Date(item.created_at))}</td>
      <td>${item.temperatura.toFixed(1)} &deg;C</td>
      <td>${item.humedad.toFixed(1)} %HR</td>
      <td>${Math.round(item.presion).toLocaleString('es-AR')} Pa</td>
      <td><span class="badge ${item.estado === 'Normal' ? 'ok' : 'error'}">${item.estado}</span></td>
    </tr>`).join('');
  }

  async function cargarDatosBD() {
    const errorBox = document.getElementById('errorBox');
    try {
      readings = await DataLoggerAPI.obtenerHistorialDatalogger(1);
      const current = await DataLoggerAPI.obtenerMedicionActual();
      if (current && !readings.some(item => item.created_at === current.created_at)) readings.push(current);
      readings.sort((a, b) => new Date(a.created_at) - new Date(b.created_at));
      renderSummary(readings[readings.length - 1]);
      renderTable();
      errorBox.style.display = 'none';
      document.getElementById('ultimaActualizacion').textContent = formatArgentinaTime();
    } catch (error) {
      errorBox.textContent = `No se pudieron leer los datos del ESP32: ${error.message}`;
      errorBox.style.display = 'block';
      throw error;
    }
  }

  window.cargarDatosBD = cargarDatosBD;
  window.agregarMedicionSimuladaBD = cargarDatosBD;
  cargarDatosBD().catch(console.error);
})();
