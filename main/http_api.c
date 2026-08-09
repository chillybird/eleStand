#include "http_api.h"
#include "cmd_queue.h"
#include "servo.h"
#include "wifi.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char INDEX_HTML[] =
"<!DOCTYPE html><html lang=zh><head>"
"<meta charset=UTF-8><meta name=viewport content='width=device-width,initial-scale=1'>"
"<title>eleStand</title><style>"
"*{margin:0;padding:0;box-sizing:border-box}"
"body{font-family:system-ui;background:#1a1a2e;color:#e0e0e0;display:flex;justify-content:center;align-items:center;min-height:100vh}"
".c{background:#16213e;border-radius:16px;padding:24px;width:380px}"
"h1{font-size:18px;text-align:center;margin-bottom:16px;color:#e94560}"
".a{text-align:center;font-size:48px;font-weight:700;color:#4ecca3;margin-bottom:12px}"
".a span{font-size:20px;color:#888}"
".l{display:flex;justify-content:space-around;margin-bottom:12px}"
".l>div{text-align:center}"
".l>.t{font-size:11px;color:#888}"
".l>.v{font-size:28px;font-weight:700;color:#4ecca3}"
".r{display:flex;align-items:center;gap:8px;margin-bottom:14px}"
".r input{flex:1;accent-color:#4ecca3}"
".r label{font-size:12px;color:#888;white-space:nowrap}"
"button{padding:14px;border:none;border-radius:8px;font-size:15px;font-weight:600;cursor:pointer;width:100%}"
"button:active{transform:scale(.96)}"
".g{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-bottom:12px}"
".g button{background:#1a1a2e;color:#e0e0e0;border:1px solid #333}"
".dn{background:#e94560!important;color:#fff!important;border:none}"
".up{background:#0f3460!important;color:#fff!important;border:none}"
".wf{background:#1a1a2e;border-radius:8px;padding:12px}"
".wf h2{font-size:13px;color:#888;margin-bottom:8px}"
".wf input{width:100%;padding:8px;margin-bottom:6px;border:none;border-radius:6px;background:#0d1117;color:#e0e0e0;font-size:13px}"
".wf button{background:#4ecca3;color:#1a1a2e;padding:10px;font-size:13px}"
"</style></head><body><div class=c>"
"<h1>MG90S 舵机控制</h1>"
"<div class=a id=ang>0<span>°</span></div>"
"<div class=l><div><div class=t>放下</div><div class=v>0°</div></div>"
"<div><div class=t>立起</div><div class=v>90°</div></div></div>"
"<div class=r><label>0</label><input type=range id=sl min=0 max=180 value=0 oninput='setA(this.value)'>"
"<label>180</label></div>"
"<div class=g>"
"<button class=dn onclick=go(0)>放下 0°</button>"
"<button class=up onclick=go(90)>立起 90°</button>"
"</div>"
"<div class=wf><h2>WiFi 配置</h2>"
"<input id=wfSSID placeholder='WiFi SSID'>"
"<input id=wfPASS type=password placeholder='WiFi 密码'>"
"<button onclick=wifiConnect()>连接</button></div>"
"</div><script>"
"function P(e,b){return fetch('/api/'+e,{method:'POST',body:b||''})}"
"function go(a){servo_set_angle(a)}"
"function setA(a){document.getElementById('ang').innerHTML=a+'<span>°</span>';sl.value=a}"
"function wifiConnect(){"
"var s=wfSSID.value,p=wfPASS.value;if(!s){alert('SSID required');return}"
"var b='ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p);"
"P('wifi',b).then(r=>r.json()).then(j=>alert(j.ok?'Connecting...':'Fail:'+j.msg))}"
"setInterval(()=>fetch('/api/angle').then(r=>r.json()).then(j=>{setA(j.angle)}),1000)"
"</script></body></html>";

static esp_err_t handle_index(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, sizeof(INDEX_HTML) - 1);
}

static esp_err_t handle_angle_get(httpd_req_t *req) {
    char buf[32];
    snprintf(buf, sizeof(buf), "{\"angle\":%d}", servo_get_angle());
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, strlen(buf));
}

static esp_err_t handle_angle_set(httpd_req_t *req) {
    char q[16];
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK) {
        char v[4];
        if (httpd_query_key_value(q, "val", v, sizeof(v)) == ESP_OK) {
            servo_set_angle(atoi(v));
        }
    }
    return handle_angle_get(req);
}

static esp_err_t handle_cmd(httpd_req_t *req, int cmd) {
    cmd_post(cmd);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"ok\":true}", 9);
}

static esp_err_t handle_wifi(httpd_req_t *req) {
    char body[128] = {0};
    httpd_req_recv(req, body, sizeof(body) - 1);
    char ssid[33] = {0}, pass[65] = {0};
    httpd_query_key_value(body, "ssid", ssid, sizeof(ssid));
    httpd_query_key_value(body, "pass", pass, sizeof(pass));
    if (ssid[0] == '\0') { httpd_resp_send(req, "{\"ok\":false}", 10); return ESP_FAIL; }
    ESP_LOGI("http", "WiFi: %s", ssid);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", 9);
    wifi_connect(ssid, pass);
    return ESP_OK;
}

static esp_err_t dispatch(httpd_req_t *req) {
    const char *uri = req->uri;
    if (strcmp(uri, "/") == 0)           return handle_index(req);
    if (strncmp(uri, "/api/angle?", 11)==0) return handle_angle_set(req);
    if (strcmp(uri, "/api/angle") == 0)  return handle_angle_get(req);
    if (strcmp(uri, "/api/down") == 0)   return handle_cmd(req, CMD_DOWN);
    if (strcmp(uri, "/api/up") == 0)     return handle_cmd(req, CMD_UP);
    if (strcmp(uri, "/api/stop") == 0)   return handle_cmd(req, CMD_STOP);
    if (strcmp(uri, "/api/wifi") == 0)   return handle_wifi(req);
    httpd_resp_send_404(req);
    return ESP_FAIL;
}

void http_server_start(void) {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    httpd_handle_t server;
    ESP_ERROR_CHECK(httpd_start(&server, &cfg));
    httpd_uri_t uri = { .uri = "/*", .method = HTTP_GET, .handler = dispatch };
    httpd_register_uri_handler(server, &uri);
    uri.method = HTTP_POST;
    httpd_register_uri_handler(server, &uri);
    ESP_LOGI("http", "Server started on %s", wifi_get_ip());
}
