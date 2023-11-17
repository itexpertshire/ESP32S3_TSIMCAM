
// Assist setup for new app installations
// original provided by gemi254
// 
// s60sc 2023

#include "appGlobals.h"

static fs::FS fp = STORAGE;

static bool wgetFile(const char* githubURL, const char* filePath, bool restart = false) {
  // download file from github
  bool res = false;
  if (fp.exists(filePath)) {
    // if file exists but is empty, delete it to allow re-download
    File f = fp.open(filePath, FILE_READ);
    size_t fSize = f.size();
    f.close();
    if (!fSize) fp.remove(filePath);
  }
  if (!fp.exists(filePath)) {
    if (WiFi.status() != WL_CONNECTED) return false;  
    char downloadURL[150];
    snprintf(downloadURL, 150, "%s%s", githubURL, filePath);
    File f = fp.open(filePath, FILE_WRITE);
    if (f) {
      HTTPClient https;
      WiFiClientSecure wclient;
      if (useSecure && strlen(git_rootCACertificate)) wclient.setCACert(git_rootCACertificate);
      else wclient.setInsecure(); // no cert check     
      if (!https.begin(wclient, downloadURL)) {
        char errBuf[100];
        wclient.lastError(errBuf, 100);
        checkMemory();
        LOG_ERR("Could not connect to github server, err: %s", errBuf);
      } else {
        LOG_INF("Downloading %s from %s", filePath, downloadURL);    
        int httpCode = https.GET();
        int fileSize = 0;
        if (httpCode == HTTP_CODE_OK) {
          fileSize = https.writeToStream(&f);
          if (fileSize <= 0) {
            httpCode = 0;
            LOG_ERR("Download failed: writeToStream");
          } else LOG_INF("Downloaded %s, size %s", filePath, fmtSize(fileSize));       
        } else LOG_ERR("Download failed, error: %s", https.errorToString(httpCode).c_str());    
        https.end();
        f.close();
        if (httpCode == HTTP_CODE_OK) res = true;
        else fp.remove(filePath);
      }
    } else LOG_ERR("Open failed: %s", filePath);
    if (restart) {
      if (loadConfig()) doRestart("config file downloaded");
    }
  } else res = true;
  return res;
}

bool checkDataFiles() {
  // Download any missing data files
  if (!fp.exists(DATA_DIR)) fp.mkdir(DATA_DIR);
  bool res = false;
  if (strlen(GITHUB_URL)) {
    res = wgetFile(GITHUB_URL, CONFIG_FILE_PATH, true);
    if (res) res = wgetFile(GITHUB_URL, COMMON_JS_PATH); 
    if (res) res = wgetFile(GITHUB_URL, INDEX_PAGE_PATH); 
    if (res) res = appDataFiles(); 
  }
  return res;
}

const char* defaultPage_html = R"~(
<!doctype html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <title>Application setup</title> 
</head>
<script>
function Config(){
  if (!window.confirm('This will reboot the device to activate new settings.'))  return false; 
  fetch('/control?ST_SSID=' + encodeURI(document.getElementById('ST_SSID').value))
  .then(r => { console.log(r); return fetch('/control?ST_Pass=' + encodeURI(document.getElementById('ST_Pass').value)) })
  .then(r => { console.log(r); return fetch('/control?save=1') })     
  .then(r => { console.log(r); return fetch('/control?reset=1') })
  .then(r => { console.log(r); }); 
  return false;
}
</script>
<body style="font-size:18px">
<br>
<center>
  <table border="0">
    <tr><th colspan="3">Wifi setup..</th></tr>
    <tr><td colspan="3"></td></tr>
    <tr>
    <td>SSID</td>
    <td>&nbsp;</td>
    <td><input id="ST_SSID" name="ST_SSID" length=32 placeholder="Router SSID" class="input"></td>
  </tr>
    <tr>
    <td>Password</td>
    <td>&nbsp;</td>
    <td><input id="ST_Pass" name="ST_Pass" length=64 placeholder="Router password" class="input"></td>
  </tr>
  <tr><td colspan="3"></td></tr>
    <tr><td colspan="3" align="center">
        <button type="button" onClick="return Config()">Connect</button>&nbsp;<button type="button" onclick="window.location.reload;">Cancel</button>
    </td></tr>
  </table>
  <br/><br/><a href="/web?OTA.htm"><button>OTA Update</button></a>
</center>      
</body>
</html>
)~";

// in case app html is not present, or corrupted
// <ip address>/web?OTA.htm
const char* otaPage_html = R"~(
<html>
  <head>
    <title>Built In OTA</title>
  </head>
  <body>
    <br></br>
    <form id="upload_form" enctype="multipart/form-data" method="post">
      <input type="file" name="otafile" id="otafile" onchange="otaUploadFile()"><br>
      <br></br>
      <progress id="progressBar" value="0" max="100" style="width:300px;"></progress>
      <h3 id="status"></h3>
      <p id="loaded_n_total"></p>
    </form>
    <script>
      const defaultPort = window.location.protocol == 'http:' ? 80 : 443; 
      const webPort = !window.location.port ? defaultPort : window.location.port; // in case alternative ports specified
      const webServer = window.location.protocol + '//' + document.location.hostname + ':' + webPort;
      const $ = document.querySelector.bind(document);
   
      async function otaUploadFile() {
        // notify server to start ota 
        let file = $("#otafile").files[0];
        const response = await fetch('/control?startOTA=' + file.name);
        if (response.ok) {
          // submit file for uploading
          let xhr = new XMLHttpRequest();
          xhr.upload.addEventListener("progress", progressHandler, false);
          xhr.addEventListener("load", completeHandler, false);
          xhr.addEventListener("error", errorHandler, false);
          xhr.addEventListener("abort", abortHandler, false);
          xhr.open("POST", webServer +  '/upload');
          xhr.send(file);
        } else console.log(response.status); 
      }

      function progressHandler(event) {
        $("#loaded_n_total").innerHTML = "Uploaded " + event.loaded + " of " + event.total + " bytes";
        let percent = (event.loaded / event.total) * 100;
        $("#progressBar").value = Math.round(percent);
        $("#status").innerHTML = Math.round(percent) + "% transferred";
        if (event.loaded  == event.total) $("#status").innerHTML = 'Uploaded, wait for completion result';
      }

      function completeHandler(event) {
        $("#status").innerHTML = event.target.responseText;
      }

      function errorHandler(event) {
        $("#status").innerHTML = "Upload Failed";
      }

      function abortHandler(event) {
        $("#status").innerHTML = "Upload Aborted";
      }
    </script>
  </body>
</html>
)~";
