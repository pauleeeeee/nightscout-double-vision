//require clay package for settings
var Clay = require('pebble-clay');
var clayConfig = require('./config.json');
//override the handleEvents feature in Clay. This way we can intercept values here in JS and then figure out what to do with them
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

//include MessageQueue system
var MessageQueue = require('message-queue-pebble');

//declare initial values or lackthereof
var personOneData;
var personTwoData;

//holding objects for settings
var storedSettings = null;
var fetchInterval = 30000; 

// localStorage.clear();

//array of alert objects; first item is person one and second is person two
lastAlerts = [
    {
        low: new Date("1-1-1990").getTime(),
        high: new Date("1-1-1990").getTime()
    },
    {
        low: new Date("1-1-1990").getTime(),
        high: new Date("1-1-1990").getTime()
    }
];

//because we overrid the default behavior of clay we must now tell it to open the config page
Pebble.addEventListener('showConfiguration', function(e) {
    Pebble.openURL(clay.generateUrl());
});

//when the config page is closed...
Pebble.addEventListener('webviewclosed', function(e) {

    if (e && !e.response) {
        return;
    }

    //get settings from response object
    storedSettings = clay.getSettings(e.response, false);
    //save the settings directly to local storage.
    localStorage.setItem("storedSettings", JSON.stringify(storedSettings));
    // console.log(JSON.stringify(storedSettings));

    if (storedSettings.FirstPersonName.value && storedSettings.FirstPersonName.value != "") {
        // console.log('sending app message');
        //if the name values are not null or blank, send them to the pebble
        Pebble.sendAppMessage({
            "PersonID": 0,
            "PersonName": storedSettings.FirstPersonName.value
        });
    }

    if (storedSettings.SecondPersonName.value && storedSettings.SecondPersonName.value != "") {
        //short 1000ms timeout to avoid confusing AppMessage on Pebble (there's probably a smarter way to do this)
        setTimeout(()=>{
            // console.log('sending app message');
            Pebble.sendAppMessage({
                "PersonID": 1,
                "PersonName": storedSettings.SecondPersonName.value
            });
        },1000)
    }


});

//this function is called when the watchface is ready
Pebble.addEventListener("ready",
    function(e) {
        // setTimeout(()=>{fetchGraphData(0," ")}, 5000);
        //get persons data from local storage if set. If not set, make dummy object that can be compared later.
        var pOneData = JSON.parse(localStorage.getItem("personOneData"));
        if (pOneData){personOneData = pOneData} else {personOneData={bgs:[{datetime:""}]}};
        
        var pTwoData = JSON.parse(localStorage.getItem("personTwoData"));
        if (pTwoData){personTwoData = pTwoData} else {personTwoData={bgs:[{datetime:""}]}};

        lAlert = JSON.parse(localStorage.getItem("lastAlerts"));
        if(lAlert){
            lastAlerts = lAlert;
        }

        //get settings from local storage
        var settings = JSON.parse(localStorage.getItem("storedSettings"));

        //if there are settings assign them and begin the fetch loop
        if (settings) {
            // console.log('found settings');
            storedSettings = settings;
            //start fetchLoop
            fetchLoop();

        } else {
            // console.log('no settings');
            //show notification stating that settings must be configured. Should only ever happen once.
            Pebble.showSimpleNotificationOnPebble("Configuration needed","Go to the watchface settings inside the Pebble phone app to configure this watchface.")
        }

    }
);

//listen for the watch to call for graphs after being tapped
Pebble.addEventListener('appmessage', function(e) {
    var getGraphs = e.payload["GetGraphs"];
    if (getGraphs) {
        fetchGraphData(0,storedSettings.FirstPersonURL.value);
        setTimeout(()=>{fetchGraphData(1,storedSettings.SecondPersonURL.value);},1500);
    }
});


function fetchLoop(){
    //fetch data for person 1
    fetchNightscoutData(0, personOneData, storedSettings.FirstPersonURL.value);
    //fetch data for person 2 after 3 seconds
    setTimeout(()=>{fetchNightscoutData(1, personTwoData, storedSettings.SecondPersonURL.value);},3000);

    //call this function again every fetchInterval ms
    setTimeout(()=>{fetchLoop()},fetchInterval);
}

function fetchGraphData(id, url){
    //open request
    var req = new XMLHttpRequest();
    req.open('GET', url + "/api/v1/entries.json?count=" + storedSettings.GraphTimeScale.value, true);
    req.onload = function(e) {
        if (req.readyState == 4) {
          // 200 - HTTP OK
            if(req.status == 200) {
                var entries = JSON.parse(req.responseText);
                entries.reverse();
                var graphData = [];
                var offset = 270;
                var correction = 10;
                for (let entry of entries) {
                    graphData.push({
                        time: Math.round(((entry.date - entries[0].date) / 1000 / 60)),
                        sgv: offset-entry.sgv
                    });
                }

                // console.log(JSON.stringify(entries));
                
                var sampled = largestTriangleThreeBuckets(graphData, 20, "time", "sgv");
                console.log(JSON.stringify(sampled));

                var xscale = 144 / sampled[sampled.length-1].time;
                var yscale = 72 / offset;


                let xbuffer = new ArrayBuffer(40);
                let xview16 = new Uint16Array(xbuffer);
                let ybuffer = new ArrayBuffer(40);
                let yview16 = new Uint16Array(ybuffer);

                for (let i = 0; i < 20; i++) {
                    xview16[i] = Math.round(sampled[i].time * xscale);
                    yview16[i] = Math.round(sampled[i].sgv * yscale) + correction;
                }

                let xview8 = new Uint8Array(xbuffer);
                let yview8 = new Uint8Array(ybuffer);

                let sendingObject = {
                    "PersonID": id,
                    "GraphHighPoint": Math.round((offset - storedSettings.HighAlertValue.value) * yscale) + correction,
                    "GraphLowPoint": Math.round((offset - storedSettings.LowAlertValue.value) * yscale) + correction,
                    "GraphXPoints": Array.from(xview8),
                    "GraphYPoints": Array.from(yview8)
                };

                MessageQueue.sendAppMessage(sendingObject);

            }
        }
    }
    req.send();
}

function fetchNightscoutData(id, personData, url){
    //open request
    var req = new XMLHttpRequest();
    req.open('GET', url + "/pebble", true);
    req.onload = function(e) {
        if (req.readyState == 4) {
          // 200 - HTTP OK
            if(req.status == 200) {

                //get response text from XMLHttpRequest / this repsonse object is the JSON blob returned by the /pebble endpoint from the nightscout herokuapp
                //example object returned: {"status":[{"now":1583651712777}],"bgs":[{"sgv":"287","trend":3,"direction":"FortyFiveUp","datetime":1583651514273,"filtered":370454.8800221163,"unfiltered":383020.389040932,"noise":1,"bgdelta":6,"battery":"34","iob":"2.62","bwp":"1.22","bwpo":156,"cob":0}],"cals":[{"slope":912.6609622111511,"intercept":157029.0133658369,"scale":1}]}
                var response = JSON.parse(req.responseText);
                // console.log(JSON.stringify(response));

                //check to see if this data is old. If it's old... do nothing.
                if (response.bgs[0].datetime == personData.bgs[0].datetime) {
                    // console.log('did not send appmessage');
                    return

                } else {
                    //if it's new, construct an appmessage using the appropriate keys defined in package.json and in nightscout-double-vision.h
                    //minutes ago is simple math that subtracts the datetime from now and rounds to nearest whole minute
                    //direction is an int defined by directionStringToInt function
                    //transform delta does some formatting
                    var message = {
                        "PersonID":id,
                        "SGV": response.bgs[0].sgv,
                        "Delta": transformDelta(response.bgs[0].bgdelta),
                        "IOB": response.bgs[0].iob,
                        "Battery": "%"+response.bgs[0].battery,
                        "Direction": directionStringToInt(response.bgs[0].direction),
                        "MinutesAgo": Math.round((response.status[0].now - response.bgs[0].datetime) / 1000 / 60),
                        "SendAlert": makeAlert(id, response.bgs[0].sgv),
                        "RespectQuietTime": (storedSettings.RespectQuietTime.value ? 1: 0)
                    };

                    // console.log(JSON.stringify(message));

                    //send the app message and use a success callback. On success, store the message to localStorage so that it can be compared on the next loop.
                    Pebble.sendAppMessage(message,()=>{
                        if (id === 0){
                            localStorage.setItem("personOneData", JSON.stringify(response));
                            personOneData = response;
                        } else if (id === 1) {
                            localStorage.setItem("personTwoData", JSON.stringify(response));
                            personTwoData = response;
                        }
                    });
                }

            } else {
                return null;
            }
        }
    }
    //send the request!
    req.send();
}

function makeAlert(id, sgv){

    if (!storedSettings.EnableAlerts.value) {
        // console.log ('alerts disabled');
        return 0;

    } else {

        var integer = 0;
    
        if (sgv < storedSettings.LowAlertValue.value && (new Date().getTime() - lastAlerts[id].low) > storedSettings.LowSnoozeTimeout.value * 60 * 1000){
            integer = Number(storedSettings.LowVibePattern.value);
            lastAlerts[id].low = new Date().getTime();
            // console.log('met low criteria');

        } else if (sgv > storedSettings.HighAlertValue.value && (new Date().getTime() - lastAlerts[id].high) > storedSettings.HighSnoozeTimeout.value * 60 * 1000){
            integer = Number(storedSettings.HighVibePattern.value);
            lastAlerts[id].high = new Date().getTime();
            // console.log('met high criteria');

        }
        // console.log(JSON.stringify(lastAlerts));
        localStorage.setItem("lastAlerts", JSON.stringify(lastAlerts));
    
        return integer;

    }

}

//utility functions
//formats the delta
function transformDelta(value){
    if (value >= 0){
        value = "+"+value;
    } else {
        value = ""+value;
    }
    return value;
}

//takes the string returned by nightscout and turns it into an int
function directionStringToInt(direction) {
    switch (direction) {
        case "NONE":
            return 0;
        case "DoubleUp":
            return 1;
        case "SingleUp":
            return 2;
        case "FortyFiveUp":
            return 3;
        case "Flat":
            return 4;
        case "FortyFiveDown":
            return 5;
        case "SingleDown":
            return 6;
        case "DoubleDown":
            return 7;
        case 'NOT COMPUTABLE':
            return 8;
        case 'RATE OUT OF RANGE':
            return 9;
        default:
            return 0;
    }
};


function largestTriangleThreeBuckets(data, threshold, xAccessor, yAccessor) {

    var floor = Math.floor,
      abs = Math.abs;

    var daraLength = data.length;
    if (threshold >= daraLength || threshold === 0) {
      return data; // Nothing to do
    }

    var sampled = [],
      sampledIndex = 0;

    // Bucket size. Leave room for start and end data points
    var every = (daraLength - 2) / (threshold - 2);

    var a = 0,  // Initially a is the first point in the triangle
      maxAreaPoint,
      maxArea,
      area,
      nextA;

    sampled[ sampledIndex++ ] = data[ a ]; // Always add the first point

    for (var i = 0; i < threshold - 2; i++) {

      // Calculate point average for next bucket (containing c)
      var avgX = 0,
        avgY = 0,
        avgRangeStart  = floor( ( i + 1 ) * every ) + 1,
        avgRangeEnd    = floor( ( i + 2 ) * every ) + 1;
      avgRangeEnd = avgRangeEnd < daraLength ? avgRangeEnd : daraLength;

      var avgRangeLength = avgRangeEnd - avgRangeStart;

      for ( ; avgRangeStart<avgRangeEnd; avgRangeStart++ ) {
        avgX += data[ avgRangeStart ][ xAccessor ] * 1; // * 1 enforces Number (value may be Date)
        avgY += data[ avgRangeStart ][ yAccessor ] * 1;
      }
      avgX /= avgRangeLength;
      avgY /= avgRangeLength;

      // Get the range for this bucket
      var rangeOffs = floor( (i + 0) * every ) + 1,
        rangeTo   = floor( (i + 1) * every ) + 1;

      // Point a
      var pointAX = data[ a ][ xAccessor ] * 1, // enforce Number (value may be Date)
        pointAY = data[ a ][ yAccessor ] * 1;

      maxArea = area = -1;

      for ( ; rangeOffs < rangeTo; rangeOffs++ ) {
        // Calculate triangle area over three buckets
        area = abs( ( pointAX - avgX ) * ( data[ rangeOffs ][ yAccessor ] - pointAY ) -
              ( pointAX - data[ rangeOffs ][ xAccessor ] ) * ( avgY - pointAY )
              ) * 0.5;
        if ( area > maxArea ) {
          maxArea = area;
          maxAreaPoint = data[ rangeOffs ];
          nextA = rangeOffs; // Next a is this b
        }
      }

      sampled[ sampledIndex++ ] = maxAreaPoint; // Pick this point from the bucket
      a = nextA; // This a is the next a (chosen b)
    }

    sampled[ sampledIndex++ ] = data[ daraLength - 1 ]; // Always add last

    return sampled;
  }