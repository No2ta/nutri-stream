var CH   = "3361937";
var RKEY = "D5T6CCSB14JOK8WG";
// 20 is enough idk how it works but it did i guess
var FEED = "https://api.thingspeak.com/channels/" + CH + "/feeds.json?api_key=" + RKEY + "&results=20";

var frozenStreak = 0;   // consecutive polls where temp didn't change
var prevTemp     = null;
var lastSeen     = null; // ISO string of most recent feed entry

document.getElementById("right").onclick = function() {
  document.getElementById("slider").scrollBy({ left: window.innerWidth, behavior: "smooth" });
};
document.getElementById("left").onclick = function() {
  document.getElementById("slider").scrollBy({ left: -window.innerWidth, behavior: "smooth" });
};


// reuse the same chart options for all four — they're identical except the label
// (tried making them look different once, looked awful, reverted)
function mkChart(id, lbl) {
  return new Chart(document.getElementById(id), {
    type: "line",
    data: {
      labels: [],
      datasets: [{
        label: lbl,
        data: [],
        borderColor: "#f1f0cc",
        backgroundColor: "rgba(241,240,204,0.09)",
        borderWidth: 2,
        tension: 0.35
      }]
    },
    options: {
      responsive: true,
      animation: false,
      scales: {
        x: { ticks: {color:"#000000ff"}, grid: {color:"rgba(255,255,255,0.05)"} },
        y: { ticks: {color:"#000000ff"}, grid: {color:"rgba(255,255,255,0.05)"} }
      }
    }
  });
}

var charts = {
  temp:  mkChart("cTemp", "Temp C"),
  hum:   mkChart("cHum",  "Humidity %"),
  lux:   mkChart("cLux",  "Light lux"),
  tds:   mkChart("cTDS",  "TDS ppm")
};


function hhMM(iso) {
  var d = new Date(iso);
  var m = d.getMinutes();
  return d.getHours() + ":" + (m < 10 ? "0"+m : m);
}

// field1 (temp) sometimes spits out -127 ل when the wires move
// dropping those points instead of letting them wreck the y-axis scale
function badTemp(v) {
  var n = +v;
  return isNaN(n) || n < 0 || n > 60;
}

function pushToChart(chart, feeds, fNum, reject) {
  var lbls = [], pts = [];
  for (var i = 0; i < feeds.length; i++) {
    lbls.push(hhMM(feeds[i].created_at));
    var v = feeds[i]["field" + fNum];
    pts.push((!v || (reject && reject(v))) ? null : +v);
  }
  chart.data.labels = lbls;
  chart.data.datasets[0].data = pts;
  chart.update();
}

function applyRelay(togId, lblId, raw) {
  var tog = document.getElementById(togId);
  var lbl = document.getElementById(lblId);
  if (!tog || !lbl) return;

  var on = (raw === "1");
  tog.classList.toggle("live", on);
  lbl.textContent = on ? "ON" : "OFF";
  lbl.className   = "relay-state " + (on ? "is-on" : "is-off");
}


async function poll() {
  var res;
  try {
    res = await fetch(FEED, { cache: "no-cache" });
  } catch(e) {
    console.warn("fetch failed:", e.message);
    return;
  }

  if (!res.ok) { console.warn("TS returned", res.status); return; }

  var data;
  try { data = await res.json(); }
  catch(e) { console.warn("json parse error — empty response?", e.message); return; }

  if (!data.feeds || !data.feeds.length) {
    console.warn("no feeds in response");
    return;
  }

  var latest = data.feeds[data.feeds.length - 1];
  lastSeen = latest.created_at;

  document.getElementById("valTemp").textContent = latest.field1 || "--";
  document.getElementById("valHum").textContent  = latest.field2 || "--";
  document.getElementById("valLux").textContent  = latest.field3 || "--";
  document.getElementById("valTDS").textContent  = latest.field4 || "--";

  // DS18B20 freeze detection — sensor locks at last-known value instead of erroring
  // eight identical readings in a row at 20s intervals = 2+ min with no change, that's wrong
  if (latest.field1 === prevTemp) {
    if (++frozenStreak === 8)
      console.warn("temp stuck at", prevTemp, "— check sensor wire in res tank");
  } else {
    frozenStreak = 0;
  }
  prevTemp = latest.field1;

  applyRelay("togH", "lblH", latest.field5);
  applyRelay("togF", "lblF", latest.field6);
  applyRelay("togM", "lblM", latest.field7);
  applyRelay("togV", "lblV", latest.field8);

  pushToChart(charts.temp, data.feeds, 1, badTemp);
  pushToChart(charts.hum,  data.feeds, 2, null);
  pushToChart(charts.lux,  data.feeds, 3, null);
  pushToChart(charts.tds,  data.feeds, 4, null);
}

poll();

setInterval(function() {
  poll();

  if (!lastSeen) return;
  var staleMin = Math.floor((Date.now() - new Date(lastSeen)) / 60000);
  if (staleMin >= 15)
    console.warn("last feed was", staleMin, "min ago — Pi probably locked up again");

}, 20000);