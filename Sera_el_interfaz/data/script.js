// Definición de la URL del servidor WebSocket
var gateway = `ws://${window.location.hostname}/ws`;
var websocket; // Instancia del objeto WebSocket
var canvas; // Elemento de lienzo HTML5
var ctx; // Contexto de dibujo en el lienzo

// Datos para la gráfica
var chartData = {
    temperatura1: [],
    temperatura2: [],
    temperatura3: [],
    temperatura4: [],
    setpoint: [],
    ciclo: []
};

// Datos para el CSV
var csvData = {
    temperatura1: [],
    temperatura2: [],
    temperatura3: [],
    temperatura4: [],
    temperatura5: [],
    temperatura6: [],
    setpoint: [],
    ciclo: []
};

var maxPoints = 1800; // Máximo de puntos en la gráfica

// Cuando la página se carga, ejecuta esta función
window.addEventListener('load', onload);

function onload(event) {
    canvas = document.getElementById('chartCanvas'); // Obtiene el elemento de lienzo
    ctx = canvas.getContext('2d'); // Obtiene el contexto de dibujo
    initWebSocket(); // Inicializa la conexión WebSocket
    drawChart(); // Dibuja la gráfica (no se muestra aquí)
}

// Envía un mensaje al servidor para obtener lecturas
function getReadings() {
    websocket.send("getReadings");
}

// Inicializa la conexión WebSocket
function initWebSocket() {
    console.log('Intentando abrir una conexión WebSocket…');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

// Se ejecuta cuando la conexión se abre
function onOpen(event) {
    console.log('Conexión abierta');
    getReadings(); // Obtiene las lecturas
}

// Se ejecuta cuando la conexión se cierra
function onClose(event) {
    console.log('Conexión cerrada');
    setTimeout(initWebSocket, 2000); // Intenta reconectar después de 2 segundos
}

// Se ejecuta cuando se recibe un mensaje del servidor
function onMessage(event) {
    console.log(event.data);
    var myObj = JSON.parse(event.data);
    var keys = Object.keys(myObj);

    for (var i = 0; i < keys.length; i++) {
        var key = keys[i];
        var value = myObj[key];

        if (key.startsWith('temperatura') || key === 'setpoint' || key === 'ciclo') {
            // Formatear a dos decimales si es un valor numérico
            var formattedValue = parseFloat(value).toFixed(2);
            document.getElementById(key).innerHTML = formattedValue;
            updateChart(key, formattedValue); // Actualiza la gráfica si es necesario
            updateCSV(key, value); // Actualiza los datos del CSV
        } else {
            // Si no es un valor numérico (por ejemplo, el estado del LED)
            document.getElementById(key).innerHTML = value;
        }
    }
}

// Envía un nuevo valor de setpoint al servidor
function updateSetpoint(element) {
    var setpoint = element.value;
    websocket.send("setpoint:" + setpoint);
}

// Envía los valores de los coeficientes PID al servidor
function updatePID() {
    var kp = document.getElementById('kp').value;
    var ki = document.getElementById('ki').value;
    var kd = document.getElementById('kd').value;
    var pidValues = { kp: kp, ki: ki, kd: kd };
    websocket.send("pid:" + JSON.stringify(pidValues));
}

function drawChart() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    // Dibujar ejes
    ctx.beginPath();
    ctx.moveTo(40, 0);
    ctx.lineTo(40, canvas.height);
    ctx.lineTo(canvas.width, canvas.height);
    ctx.strokeStyle = '#000000';
    ctx.stroke();

    // Dibujar leyenda
    var legend = document.getElementById('chartLegend');
    legend.innerHTML = `
        <span style="color: blue;">Temperatura 1</span> |
        <span style="color: green;">Temperatura 2</span> |
        <span style="color: red;">Temperatura 3</span> |
        <span style="color: purple;">Temperatura 4</span> |
        <span style="color: orange;">Setpoint</span> |
        <span style="color: cyan;">Ciclo</span>
    `;

    // Dibujar datos
    drawLine(chartData.temperatura1, 'blue', 0, 100);
    drawLine(chartData.temperatura2, 'green', 0, 100);
    drawLine(chartData.temperatura3, 'red', 0, 100);
    drawLine(chartData.temperatura4, 'purple', 0, 100);
    drawLine(chartData.setpoint, 'orange', 0, 100);
    drawLine(chartData.ciclo, 'cyan', 0, 100);
}

function drawLine(data, color, min, max) {
    if (data.length > 0) {
        var sliceWidth = (canvas.width - 40) / maxPoints;
        var x = 40;

        ctx.beginPath();
        ctx.strokeStyle = color;

        for (var i = 0; i < data.length; i++) {
            var y = ((data[i] - min) / (max - min)) * canvas.height;
            y = canvas.height - y;

            if (i === 0) {
                ctx.moveTo(x, y);
            } else {
                ctx.lineTo(x, y);
            }

            x += sliceWidth;
        }

        ctx.stroke();
    }
}

function updateChart(type, newData) {
    if (!chartData[type]) {
        chartData[type] = [];
    }
    chartData[type].push(newData);
    if (chartData[type].length > maxPoints) {
        chartData[type].shift();
    }
    drawChart();
}

function updateCSV(type, newData) {
    if (!csvData[type]) {
        csvData[type] = [];
    }
    csvData[type].push(newData);
}

function saveCSV() {
    var csvContent = "data:text/csv;charset=utf-8,";
    csvContent += "Temperatura 1,Temperatura 2,Temperatura 3,Temperatura 4,Setpoint,Ciclo\n";

    var maxLength = Math.max(
        csvData.temperatura1.length,
        csvData.temperatura2.length,
        csvData.temperatura3.length,
        csvData.temperatura4.length,
        csvData.temperatura5.length,
        csvData.temperatura6.length,
        csvData.setpoint.length,
        csvData.ciclo.length
    );

    for (var i = 0; i < maxLength; i++) {
        csvContent += (csvData.temperatura1[i] || "") + ",";
        csvContent += (csvData.temperatura2[i] || "") + ",";
        csvContent += (csvData.temperatura3[i] || "") + ",";
        csvContent += (csvData.temperatura4[i] || "") + ",";
        csvContent += (csvData.temperatura5[i] || "") + ",";
        csvContent += (csvData.temperatura6[i] || "") + ",";
        csvContent += (csvData.setpoint[i] || "") + ",";
        csvContent += (csvData.ciclo[i] || "") + "\n";
    }

    var encodedUri = encodeURI(csvContent);
    var link = document.createElement("a");
    link.setAttribute("href", encodedUri);
    link.setAttribute("download", "datos.csv");
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
}
