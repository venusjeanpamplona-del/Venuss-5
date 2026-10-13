// =====================================================
// VENUS ACT 4
// DHT11 FIREBASE MONITOR
// =====================================================

import {
    initializeApp
} from "https://www.gstatic.com/firebasejs/10.12.2/firebase-app.js";

import {
    getDatabase,
    ref,
    onValue
} from "https://www.gstatic.com/firebasejs/10.12.2/firebase-database.js";


// =====================================================
// FIREBASE CONFIG
// VENUS
// =====================================================

const firebaseConfig = {

    apiKey:
        "AIzaSyD69UthfpO3blXr-bkYJ4f-vK9CiZDSsR8",

    authDomain:
        "venusjean-253b5.firebaseapp.com",

    databaseURL:
        "https://venusjean-253b5-default-rtdb.europe-west1.firebasedatabase.app/",

    projectId:
        "venusjean-253b5",

    storageBucket:
        "venusjean-253b5.firebasestorage.app",

    messagingSenderId:
        "363163389168",

    appId:
        "1:363163389168:web:25db157498b3a917e5c635",

    measurementId:
        "G-6YSNGE4KBQ"
};


// =====================================================
// INITIALIZE FIREBASE
// =====================================================

const firebaseApp =
    initializeApp(firebaseConfig);


// =====================================================
// DATABASE
// =====================================================

const database =
    getDatabase(firebaseApp);


// =====================================================
// DATABASE REFERENCE
// =====================================================

const dataRef =
    ref(database, "ESP32_Data");


// =====================================================
// GLOBAL DATA
// =====================================================

let allSensorData = {};

let selectedDate = "";

let sensorChart = null;


// =====================================================
// HTML ELEMENTS
// =====================================================

const statusElement =
    document.getElementById(
        "firebaseStatus"
    );

const statusText =
    document.getElementById(
        "statusText"
    );

const currentTemperature =
    document.getElementById(
        "currentTemperature"
    );

const currentHumidity =
    document.getElementById(
        "currentHumidity"
    );

const graphDate =
    document.getElementById(
        "graphDate"
    );

const historyDate =
    document.getElementById(
        "historyDate"
    );

const historyBody =
    document.getElementById(
        "historyTableBody"
    );

const recordCount =
    document.getElementById(
        "recordCount"
    );

const toggleHistory =
    document.getElementById(
        "toggleHistory"
    );

const historyContent =
    document.getElementById(
        "historyContent"
    );


// =====================================================
// FIREBASE STATUS
// =====================================================

function setStatus(
    text,
    connected
) {

    if (!statusElement) {
        return;
    }


    if (statusText) {

        statusText.textContent =
            text.toUpperCase();

    }


    statusElement.classList.remove(
        "status-connected",
        "status-error"
    );


    if (connected === true) {

        statusElement.classList.add(
            "status-connected"
        );

    }

    else if (connected === false) {

        statusElement.classList.add(
            "status-error"
        );

    }

}


// =====================================================
// NUMBER HELPER
// =====================================================

function toNumber(value) {

    const number =
        Number(value);


    if (
        Number.isFinite(number)
    ) {

        return number;

    }


    return null;

}


// =====================================================
// FORMAT NUMBER
// =====================================================

function formatNumber(value) {

    if (
        value === null ||
        value === undefined
    ) {

        return "--";

    }


    return Number(value).toFixed(1);

}


// =====================================================
// GET DATE LIST
// =====================================================

function getDateList(data) {

    return Object.keys(
        data || {}
    )

    .filter(
        key =>
            data[key] &&
            typeof data[key] === "object"
    )

    .sort()

    .reverse();

}


// =====================================================
// GET LATEST READING
// =====================================================

function getLatestReading(data) {

    let latest = null;


    const dates =
        Object.keys(
            data || {}
        ).sort();


    for (
        const date of dates
    ) {

        const times =
            Object.keys(
                data[date] || {}
            ).sort();


        for (
            const time of times
        ) {

            const reading =
                data[date][time];


            if (
                !reading ||
                typeof reading !== "object"
            ) {

                continue;

            }


            const temperature =
                toNumber(
                    reading.temperature
                );


            const humidity =
                toNumber(
                    reading.humidity
                );


            if (
                temperature === null &&
                humidity === null
            ) {

                continue;

            }


            latest = {

                date:
                    date,

                time:
                    time,

                temperature:
                    temperature,

                humidity:
                    humidity

            };

        }

    }


    return latest;

}


// =====================================================
// GET READINGS FOR DATE
// =====================================================

function getReadingsForDate(
    date
) {

    const result = [];


    if (!date) {

        return result;

    }


    const dayData =
        allSensorData[date];


    if (
        !dayData ||
        typeof dayData !== "object"
    ) {

        return result;

    }


    const times =
        Object.keys(dayData)

            .filter(
                time =>
                    dayData[time] &&
                    typeof dayData[time] === "object"
            )

            .sort();


    times.forEach(
        time => {

            const reading =
                dayData[time];


            const temperature =
                toNumber(
                    reading.temperature
                );


            const humidity =
                toNumber(
                    reading.humidity
                );


            if (
                temperature === null &&
                humidity === null
            ) {

                return;

            }


            result.push({

                time:
                    time,

                temperature:
                    temperature,

                humidity:
                    humidity

            });

        }
    );


    return result;

}


// =====================================================
// POPULATE DATE SELECTS
// =====================================================

function populateDateSelect() {

    const dates =
        getDateList(
            allSensorData
        );


    // GRAPH DATE

    if (graphDate) {

        graphDate.innerHTML = "";


        if (dates.length === 0) {

            const option =
                document.createElement(
                    "option"
                );

            option.value = "";

            option.textContent =
                "No dates available";

            graphDate.appendChild(
                option
            );

        }

        else {

            dates.forEach(
                date => {

                    const option =
                        document.createElement(
                            "option"
                        );

                    option.value =
                        date;

                    option.textContent =
                        date;

                    graphDate.appendChild(
                        option
                    );

                }
            );


            graphDate.value =
                selectedDate ||
                dates[0];

        }

    }


    // HISTORY DATE

    if (historyDate) {

        historyDate.innerHTML = "";


        if (dates.length === 0) {

            const option =
                document.createElement(
                    "option"
                );

            option.value = "";

            option.textContent =
                "No dates available";

            historyDate.appendChild(
                option
            );

        }

        else {

            dates.forEach(
                date => {

                    const option =
                        document.createElement(
                            "option"
                        );

                    option.value =
                        date;

                    option.textContent =
                        date;

                    historyDate.appendChild(
                        option
                    );

                }
            );


            historyDate.value =
                selectedDate ||
                dates[0];

        }

    }

}


// =====================================================
// UPDATE CURRENT READING
// =====================================================

function updateCurrentReading() {

    const latest =
        getLatestReading(
            allSensorData
        );


    if (!latest) {

        if (currentTemperature) {

            currentTemperature.textContent =
                "-- °C";

        }


        if (currentHumidity) {

            currentHumidity.textContent =
                "-- %";

        }


        return;

    }


    if (currentTemperature) {

        currentTemperature.textContent =
            formatNumber(
                latest.temperature
            ) + " °C";

    }


    if (currentHumidity) {

        currentHumidity.textContent =
            formatNumber(
                latest.humidity
            ) + " %";

    }


    console.log(
        "LATEST:",
        latest
    );

}


// =====================================================
// UPDATE GRAPH
// =====================================================

function updateChart() {

    if (
        typeof Chart === "undefined"
    ) {

        console.error(
            "Chart.js is not loaded."
        );

        return;

    }


    const date =
        graphDate
            ? graphDate.value
            : selectedDate;


    const readings =
        getReadingsForDate(
            date
        );


    const labels =
        readings.map(
            item => item.time
        );


    const temperatures =
        readings.map(
            item => item.temperature
        );


    const humidities =
        readings.map(
            item => item.humidity
        );


    const canvas =
        document.getElementById(
            "sensorChart"
        );


    if (!canvas) {

        return;

    }


    if (sensorChart) {

        sensorChart.destroy();

        sensorChart = null;

    }


    sensorChart =
        new Chart(
            canvas,
            {

                type:
                    "line",


                data:
                {

                    labels:
                        labels,


                    datasets:
                    [

                        {

                            label:
                                "Temperature (°C)",

                            data:
                                temperatures,

                            yAxisID:
                                "temperature",

                            tension:
                                0.3,

                            borderWidth:
                                3,

                            pointRadius:
                                3

                        },


                        {

                            label:
                                "Humidity (%)",

                            data:
                                humidities,

                            yAxisID:
                                "humidity",

                            tension:
                                0.3,

                            borderWidth:
                                3,

                            pointRadius:
                                3

                        }

                    ]

                },


                options:
                {

                    responsive:
                        true,

                    maintainAspectRatio:
                        false,


                    interaction:
                    {

                        mode:
                            "index",

                        intersect:
                            false

                    },


                    plugins:
                    {

                        legend:
                        {

                            labels:
                            {

                                color:
                                    "#f5f7ff"

                            }

                        }

                    },


                    scales:
                    {

                        x:
                        {

                            ticks:
                            {

                                color:
                                    "#8f98b3"

                            },

                            grid:
                            {

                                color:
                                    "rgba(255,255,255,0.05)"

                            }

                        },


                        temperature:
                        {

                            type:
                                "linear",

                            position:
                                "left",

                            ticks:
                            {

                                color:
                                    "#5ee7ff"

                            },

                            title:
                            {

                                display:
                                    true,

                                text:
                                    "Temperature (°C)",

                                color:
                                    "#5ee7ff"

                            },

                            grid:
                            {

                                color:
                                    "rgba(94,231,255,0.08)"

                            }

                        },


                        humidity:
                        {

                            type:
                                "linear",

                            position:
                                "right",

                            ticks:
                            {

                                color:
                                    "#7774ff"

                            },

                            title:
                            {

                                display:
                                    true,

                                text:
                                    "Humidity (%)",

                                color:
                                    "#7774ff"

                            },

                            grid:
                            {

                                drawOnChartArea:
                                    false

                            }

                        }

                    }

                }

            }
        );


    console.log(
        "GRAPH UPDATED:",
        date,
        readings.length
    );

}


// =====================================================
// UPDATE HISTORY
// =====================================================

function updateHistory() {

    if (!historyBody) {

        return;

    }


    const date =
        historyDate
            ? historyDate.value
            : selectedDate;


    const readings =
        getReadingsForDate(
            date
        );


    historyBody.innerHTML = "";


    if (readings.length === 0) {

        const row =
            document.createElement(
                "tr"
            );


        const cell =
            document.createElement(
                "td"
            );


        cell.colSpan = 3;

        cell.className =
            "empty";

        cell.textContent =
            "No sensor data available.";


        row.appendChild(
            cell
        );


        historyBody.appendChild(
            row
        );


        if (recordCount) {

            recordCount.textContent =
                "0 records";

        }


        return;

    }


    readings
        .slice()
        .reverse()
        .forEach(
            reading => {

                const row =
                    document.createElement(
                        "tr"
                    );


                const timeCell =
                    document.createElement(
                        "td"
                    );


                const temperatureCell =
                    document.createElement(
                        "td"
                    );


                const humidityCell =
                    document.createElement(
                        "td"
                    );


                timeCell.textContent =
                    reading.time;


                temperatureCell.textContent =
                    formatNumber(
                        reading.temperature
                    ) + " °C";


                humidityCell.textContent =
                    formatNumber(
                        reading.humidity
                    ) + " %";


                row.appendChild(
                    timeCell
                );


                row.appendChild(
                    temperatureCell
                );


                row.appendChild(
                    humidityCell
                );


                historyBody.appendChild(
                    row
                );

            }
        );


    if (recordCount) {

        recordCount.textContent =
            readings.length +
            (
                readings.length === 1
                    ? " record"
                    : " records"
            );

    }

}


// =====================================================
// UPDATE DASHBOARD
// =====================================================

function updateDashboard() {

    const dates =
        getDateList(
            allSensorData
        );


    console.log(
        "AVAILABLE DATES:",
        dates
    );


    if (dates.length === 0) {

        if (currentTemperature) {

            currentTemperature.textContent =
                "-- °C";

        }


        if (currentHumidity) {

            currentHumidity.textContent =
                "-- %";

        }


        if (historyBody) {

            historyBody.innerHTML = `

                <tr>

                    <td
                        colspan="3"
                        class="empty"
                    >
                        No sensor data available.
                    </td>

                </tr>

            `;

        }


        if (recordCount) {

            recordCount.textContent =
                "0 records";

        }


        populateDateSelect();

        return;

    }


    if (
        !selectedDate ||
        !dates.includes(
            selectedDate
        )
    ) {

        selectedDate =
            dates[0];

    }


    populateDateSelect();

    updateCurrentReading();

    updateChart();

    updateHistory();

}


// =====================================================
// FIREBASE LISTENER
// =====================================================

console.log(
    "================================="
);

console.log(
    "VENUS ACT4 FIREBASE MONITOR"
);

console.log(
    "Database: /ESP32_Data"
);

console.log(
    "================================="
);


setStatus(
    "Connecting",
    false
);


onValue(

    dataRef,

    (snapshot) => {

        console.log(
            "Firebase data received."
        );


        allSensorData =
            snapshot.val() || {};


        setStatus(
            "Connected",
            true
        );


        updateDashboard();

    },


    (error) => {

        console.error(
            "FIREBASE ERROR:",
            error
        );


        setStatus(
            "Error",
            false
        );

    }

);


// =====================================================
// GRAPH DATE CHANGE
// =====================================================

if (graphDate) {

    graphDate.addEventListener(
        "change",
        function () {

            selectedDate =
                this.value;


            if (historyDate) {

                historyDate.value =
                    selectedDate;

            }


            updateChart();

            updateHistory();

        }
    );

}


// =====================================================
// HISTORY DATE CHANGE
// =====================================================

if (historyDate) {

    historyDate.addEventListener(
        "change",
        function () {

            selectedDate =
                this.value;


            if (graphDate) {

                graphDate.value =
                    selectedDate;

            }


            updateHistory();

            updateChart();

        }
    );

}


// =====================================================
// SHOW / HIDE HISTORY
// =====================================================

if (toggleHistory) {

    toggleHistory.addEventListener(
        "click",
        function () {

            if (
                historyContent.classList.contains(
                    "hidden"
                )
            ) {

                historyContent.classList.remove(
                    "hidden"
                );


                toggleHistory.textContent =
                    "HIDE HISTORY";


                updateHistory();

            }

            else {

                historyContent.classList.add(
                    "hidden"
                );


                toggleHistory.textContent =
                    "SHOW HISTORY";

            }

        }
    );

}