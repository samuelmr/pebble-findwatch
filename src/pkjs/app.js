var initialized = false;
var messageQueue = [];

function sendNextMessage() {
  if (messageQueue.length > 0) {
    Pebble.sendAppMessage(messageQueue[0], appMessageAck, appMessageNack);
    console.log("Sent message to Pebble! " + JSON.stringify(messageQueue[0]));
  }
}

function appMessageAck(e) {
  console.log("Message accepted by Pebble!");
  messageQueue.shift();
  sendNextMessage();
}

function appMessageNack(e) {
  console.log("Message rejected by Pebble! " + e.error);
}

Pebble.addEventListener("ready",
  function(e) {
    console.log("JavaScript app ready and running!");
    initialized = true;
  }
);

Pebble.addEventListener("webviewclosed",
  function(e) {
    var options = JSON.parse(decodeURIComponent(e.response));
    console.log("Webview window returned: " + JSON.stringify(options));
    var time = options["0"].split(":");
    var minutes = parseInt(time[time.length-2]);
    var seconds = parseInt(time[time.length-1]);
    var timeopts = {'0': 'time',
                    '1': minutes,
                    '2': seconds};
    console.log("options formed: " + minutes + ':' + seconds);
    messageQueue.push(timeopts);
    messageQueue.push({'0': 'vibes', '1': options["1"] ? 1 : 0});
    messageQueue.push({'0': 'flashes', '1': options["2"] ? 1 : 0});
    sendNextMessage();
  }
);

Pebble.addEventListener("showConfiguration",
  function() {
    var uri = "https://rawgithub.com/samuelmr/pebble-findwatch/master/configure.html";
    console.log("Configuration url: " + uri);
    Pebble.openURL(uri);
  }
);
