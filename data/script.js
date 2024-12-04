setValues();//initial values
setInterval(setValues, 5000);//every 5 seconds

let uptime = {
    days: document.querySelector("#uptime").querySelector(".content").querySelector("#days"),
    hours: document.querySelector("#uptime").querySelector(".content").querySelector("#hours"),
    minutes: document.querySelector("#uptime").querySelector(".content").querySelector("#minutes"),
    seconds: document.querySelector("#uptime").querySelector(".content").querySelector("#seconds")
};
let loading = document.querySelector("#loading").querySelector(".content");
let storage = document.querySelector("#storage").querySelector(".content");
let memory = {
    total: document.querySelector("#memory").querySelector(".content").querySelector("#total"),
    available: document.querySelector("#memory").querySelector(".content").querySelector("#available"),
    free: document.querySelector("#memory").querySelector(".content").querySelector("#free")
};
let temp = document.querySelector("#temperature").querySelector(".content");
let humidity = document.querySelector("#humidity").querySelector(".content");

let alarmRef = document.querySelector("#alarm");

let lastUptime = null;

let alarms = [];

async function setValues(){
    //get the data
    const data = await getData();

    alarms = [];//reset every call
    alarmRef.innerHTML = "";

    //check for alarms
    let currentUptime = uptimeToSeconds(data.uptime);
    if(lastUptime != null && lastUptime > currentUptime){
        alarms.push("Server has been restarted");
    }

    lastUptime = currentUptime;

    if(data.cpu_loading.last_minute > 1){
        alarms.push(`CPU loading over 1.00 (${(Math.round(data.cpu_loading.last_minute * 100) / 100).toFixed(2)})`);
    }

    if(data.temperature > 30){
        alarms.push(`Temperature over 30° C (${Math.round(data.temperature*10)/10 + "° C"})`);
    }

    let freeMemory = KBToMB(data.memory.free)
    if(freeMemory.substring(0, freeMemory.length-2) < 2){
        alarms.push(`Free memory below 2MB (${freeMemory})`);
    }

    if(data["disk space"].used.substring(0, data["disk space"].used.length-1) > data["disk space"].total.substring(0, data["disk space"].used.length-1)/2){
        alarms.push(`Available disk space over 50% of total (${data["disk space"].used})`);
    }

    //display the data
    uptime.days.innerHTML = data.uptime.days + " days";
    uptime.hours.innerHTML = data.uptime.hours + " hours";
    uptime.minutes.innerHTML = data.uptime.minutes + " minutes";
    uptime.seconds.innerHTML = data.uptime.seconds + " seconds";

    loading.innerHTML = (Math.round(data.cpu_loading.last_minute * 100) / 100).toFixed(2) + "/2.00";

    storage.innerHTML = `Used: ${data["disk space"].used}/${data["disk space"].total}`;

    memory.total.innerHTML = "Total: " + KBToMB(data.memory.total);
    memory.available.innerHTML = "Available: " + KBToMB(data.memory.available);
    memory.free.innerHTML = "Free: " + KBToMB(data.memory.free);

    temp.innerHTML = Math.round(data.temperature*10)/10 + "° C";

    humidity.innerHTML = Math.round(data.humidity) + "% RH";

    if(alarms.length > 0){
        alarmRef.appendChild(document.createTextNode("Alarm"));
        alarms.forEach((message)=>{
            const content = document.createElement("div");
            content.classList.add("content");
            content.appendChild(document.createTextNode(message));
            alarmRef.appendChild(content);
        });
    }
}

/**
 * requests the server data
 * @returns the server data
 */
async function getData(){
    const res = await fetch(window.location.origin + "/data");
    return await res.json();
}

/**
 * converts kilobytes to megabytes
 * @param {String} kilobytes The kilobytes to be converted (ends with KB)
 * @returns the converted megabytes ending with MB
 */
function KBToMB(kilobytes){
    return Math.round(kilobytes.substring(0, kilobytes.length-2) / 1024) + "MB";
}

/**
 * converts a JSON of uptime data to total seconds
 * @param {object} uptimeJson the uptime object containing days, hours, minutes, and seconds
 * @returns the number of seconds of uptime
 */
function uptimeToSeconds(uptimeJson){
    let days = uptimeJson.days;
    let hours = uptimeJson.hours + days * 24;
    let minutes = uptimeJson.minutes + hours * 60;
    let seconds = uptimeJson.seconds + minutes * 60;
    return seconds;
}