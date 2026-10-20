const char index_html[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">

<title>YL_Device</title>

<style>
*{margin:0;padding:0;box-sizing:border-box;}
body{background:#e4e7ef;font-family:Arial,Helvetica,sans-serif;}

.main-heading{text-align:center;padding-top:20px;}
.logo img{width:200px;margin-top:20px;}

.main-heading h1{
font-size:2rem;
font-weight:700;
padding-top:30px;
color:rgb(10,46,212);
}

.login{padding:40px 15px;}

.container{
max-width:450px;
margin:0 auto;
}

.form{
background:#ffffff;
padding:30px;
border-radius:7px;
box-shadow:rgba(0,0,0,0.18) 0px 2px 4px;
}

.form h2{
text-align:center;
margin-bottom:25px;
font-size:1.2rem;
}

.form-control{
width:100%;
padding:12px 15px;
font-size:1rem;
border-radius:50px;
border:1px solid #ccc;
outline:none;
margin-bottom:20px;
}

.form-control:focus{
border-color:rgb(10,46,212);
}

.btn{
display:block;
width:40%;
margin:0 auto;
padding:10px;
font-size:1rem;
font-weight:600;
border-radius:60px;
border:none;
cursor:pointer;
background:rgb(223,56,56);
color:white;
}

.btn:hover{background:brown;}

@media(max-width:480px){
.main-heading h1{font-size:1.5rem;}
.btn{width:60%;}
}
</style>
</head>

<body>

<div class="main-heading">
<div class="logo">
<img src="yllogo.png" alt="Logo">
</div>
<h1>वायु Cast - Device Configuration</h1>
</div>

<section class="login">
<div class="container">
<form class="form" method="POST">
<h2>Provide Your Credentials</h2>

<input type="text"
class="form-control"
name="deviceID"
placeholder="Enter Device ID"
required>

<button type="submit" class="btn">Submit</button>

</form>
</div>
</section>

</body>
</html>
)rawliteral";

void AP_Mode() {
  LOG(F("Starting AP Mode..."));
uint64_t chipid = ESP.getEfuseMac();

char apName[32];
sprintf(apName, "Wayu-Cast-%04X", (uint16_t)(chipid >> 32));
  WiFi.disconnect(true);
  WiFi.softAP(apName, "wayu12345");

  LOG(F("AP IP: "));
  Serial.println(WiFi.softAPIP());

  // Start DNS server (redirect all domains to ESP IP)
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  // Serve your existing HTML page for root GET
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    //    request->send(SPIFFS, "/AP.html", "text/html");/
    request->send_P(200, "text/html", index_html);
  });

  // Handle form submission
  server.on("/", HTTP_POST, [](AsyncWebServerRequest * request) {
    if (request->hasParam(PARAM_INPUT_1, true)) {
   deviceID = request->getParam(PARAM_INPUT_1, true)->value();

      if (writePref(DEVICE_ID_KEY, deviceID.c_str())) {
        LOG(F("Device ID saved: "));
        LOG(deviceID);
      } else {
        LOG(F("Device ID save failed"));
      }
    }

    // Show confirmation text
    request->send(200, "text/html", R"rawliteral(
      <h2 style='text-align:center;margin-top:50px;'>Saved. Restarting...</h2>
      <p style='text-align:center;'>Please wait while the device reboots.</p>
    )rawliteral");

// Schedule safe restart
restartPending = true;
restartTime = millis();
});

// Catch-all route for captive portal
server.onNotFound([](AsyncWebServerRequest * request) {
  // Redirect all unknown requests to root page
  request->redirect("/");
});

server.begin();
LOG(F("Web server started"));
}
