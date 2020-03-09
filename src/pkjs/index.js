//require clay package for settings
var Clay = require('pebble-clay');
var clayConfig = require('./config.json');
//override the handleEvents feature in Clay. This way we can intercept values here in JS and then figure out what to do with them
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

//declare initial values or lackthereof
var storedSettings = null;
var personOneData;
var personTwoData;
var fetchInterval = 30000; 

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
    //call for data
});

//this function is called when the watchface is ready
Pebble.addEventListener("ready",
    function(e) {
        //get persons data from local storage if set. If not set, make dummy object that can be compared later.
        var pOneData = JSON.parse(localStorage.getItem("personOneData"));
        if (pOneData){personOneData = pOneData} else {personOneData={bgs:[{datetime:""}]}}
        
        var pTwoData = JSON.parse(localStorage.getItem("personTwoData"));
        if (pTwoData){personTwoData = pTwoData} else {personTwoData={bgs:[{datetime:""}]}}

        //get settings from local storage
        var settings = JSON.parse(localStorage.getItem("storedSettings"));

        //if there are settings assign them and begin the fetch loop
        if (settings) {
            // console.log('found settings');
            storedSettings = settings;
            //define fetch inteval based on stored settings
            fetchInterval = (storedSettings.IntervalTime.value * 60 * 1000);
            //start fetchLoop
            fetchLoop();
        } else {
            // console.log('no settings');
            //show notification stating that settings must be configured. Should only ever happen once.
            Pebble.showSimpleNotificationOnPebble("Configuration needed","Go to the watchface settings inside the Pebble phone app to configure this watchface.")
        }

    }
);


function fetchLoop(){
    //fetch data for person 1
    fetchNightscoutData(0, personOneData, storedSettings.FirstPersonURL.value);
    //fetch data for person 2 after 3 seconds
    setTimeout(()=>{fetchNightscoutData(1, personTwoData, storedSettings.SecondPersonURL.value);},3000);

    //call this function again every fetchInterval ms
    setTimeout(()=>{fetchLoop()},fetchInterval);
}

function fetchNightscoutData(id, personData, url){
    //open request
    var req = new XMLHttpRequest();
    req.open('GET', url, true);
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
                        "MinutesAgo": Math.round((response.status[0].now - response.bgs[0].datetime) / 1000 / 60)
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

