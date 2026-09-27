#include "barrier.h"
#include "database.h"
#include "parking.h"

#include <arpa/inet.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <cstdlib>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace {

struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
};

std::string jsonEscape(const std::string& value) {
    std::string out;
    for (char c : value) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out;
}

std::string urlDecode(const std::string& value) {
    std::string out;
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '+') {
            out += ' ';
        } else if (value[i] == '%' && i + 2 < value.size()) {
            auto hex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            int hi = hex(value[i + 1]);
            int lo = hex(value[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out += static_cast<char>(hi * 16 + lo);
                i += 2;
            } else {
                out += value[i];
            }
        } else {
            out += value[i];
        }
    }
    return out;
}

std::string formValue(const std::string& body, const std::string& key) {
    size_t start = 0;
    while (start <= body.size()) {
        size_t end = body.find('&', start);
        if (end == std::string::npos) end = body.size();
        std::string part = body.substr(start, end - start);
        size_t equals = part.find('=');
        std::string k = urlDecode(part.substr(0, equals));
        std::string v = equals == std::string::npos ? "" : urlDecode(part.substr(equals + 1));
        if (k == key) return v;
        if (end == body.size()) break;
        start = end + 1;
    }
    return "";
}

std::string trim(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
    return value;
}

void sendResponse(int client, int status, const std::string& statusText,
                  const std::string& body, const std::string& type) {
    std::ostringstream out;
    out << "HTTP/1.1 " << status << ' ' << statusText << "\r\n"
        << "Content-Type: " << type << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n"
        << "Cache-Control: no-store\r\n\r\n"
        << body;
    std::string response = out.str();
    send(client, response.c_str(), response.size(), 0);
}

void sendJson(int client, int status, const std::string& statusText, const std::string& body) {
    sendResponse(client, status, statusText, body, "application/json; charset=utf-8");
}

bool readRequest(int client, HttpRequest& request) {
    std::string raw;
    char buffer[4096];

    while (raw.find("\r\n\r\n") == std::string::npos && raw.size() < 32768) {
        ssize_t received = recv(client, buffer, sizeof(buffer), 0);
        if (received <= 0) return false;
        raw.append(buffer, static_cast<size_t>(received));
    }

    size_t headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos) return false;

    std::string headers = raw.substr(0, headerEnd);
    std::string body = raw.substr(headerEnd + 4);

    std::istringstream lines(headers);
    std::string requestLine;
    std::getline(lines, requestLine);
    if (!requestLine.empty() && requestLine.back() == '\r') requestLine.pop_back();

    std::istringstream first(requestLine);
    first >> request.method >> request.path;
    if (request.method.empty() || request.path.empty()) return false;

    size_t contentLength = 0;
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::string prefix = "Content-Length:";
        if (line.rfind(prefix, 0) == 0) {
            contentLength = static_cast<size_t>(std::stoul(trim(line.substr(prefix.size()))));
        }
    }

    while (body.size() < contentLength && body.size() < 32768) {
        ssize_t received = recv(client, buffer, sizeof(buffer), 0);
        if (received <= 0) break;
        body.append(buffer, static_cast<size_t>(received));
    }

    if (body.size() < contentLength) return false;
    request.body = body.substr(0, contentLength);
    return true;
}

std::string htmlPage() {
    return R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ParkEase Kenya</title>
<link rel="stylesheet" href="/style.css">
</head>
<body>
<header class="hero">
  <div>
    <p class="eyebrow">MODERN PARKING SYSTEM</p>
    <h1>ParkEase Kenya</h1>
    <p>Check available spaces, register arrivals and handle vehicle exit from one dashboard.</p>
  </div>
  <div class="barrier-card"><span>Barrier</span><strong id="barrierStatus">CLOSED</strong></div>
</header>

<main>
  <section class="stats">
    <div class="stat"><span>Total slots</span><strong id="total">0</strong></div>
    <div class="stat"><span>Available</span><strong id="available">0</strong></div>
    <div class="stat"><span>Occupied</span><strong id="occupied">0</strong></div>
  </section>

  <section class="panel">
    <div class="section-heading"><div><h2>Parking slots</h2><p>Drivers can see the current position of every slot before entry.</p></div><button class="secondary" id="refreshBtn">Refresh</button></div>
    <div id="slots" class="slots"></div>
  </section>

  <div class="two-column">
    <section class="panel">
      <h2>Vehicle arrival</h2>
      <p>Register a vehicle and the system will assign the first available slot.</p>
      <form id="entryForm">
        <label>Registration number<input id="registration" maxlength="12" placeholder="KDA 123A" required></label>
        <label>Vehicle type<select id="vehicleType"><option>Car</option><option>Motorcycle</option><option>Van</option><option>Truck</option></select></label>
        <button type="submit">Register arrival</button>
      </form>
      <div id="entryMessage" class="message"></div>
    </section>

    <section class="panel">
      <h2>Vehicle exit</h2>
      <p>Find the active session first. Payment is required before the simulated barrier opens.</p>
      <form id="exitForm">
        <label>Registration number<input id="exitRegistration" maxlength="12" placeholder="KDA 123A" required></label>
        <button type="submit">Calculate parking fee</button>
      </form>
      <div id="exitResult" class="message"></div>
    </section>
  </div>

  <section class="panel">
    <div class="section-heading"><div><h2>Recent parking records</h2><p>Completed and active sessions stored in SQLite.</p></div></div>
    <div class="table-wrap"><table><thead><tr><th>Vehicle</th><th>Slot</th><th>Entry</th><th>Exit</th><th>Duration</th><th>Amount</th><th>Status</th></tr></thead><tbody id="records"></tbody></table></div>
  </section>
</main>
<footer>ParkEase Kenya • C++17 • SQLite • DSA project</footer>
<script src="/app.js"></script>
</body>
</html>)HTML";
}

std::string css() {
    return R"CSS(:root{font-family:Inter,Arial,sans-serif;color:#17202a;background:#f4f6f8}*{box-sizing:border-box}body{margin:0}.hero{background:#17202a;color:#fff;padding:28px max(20px,calc((100% - 1100px)/2));display:flex;justify-content:space-between;gap:20px;align-items:center}.eyebrow{font-size:12px;letter-spacing:2px;opacity:.7;margin:0 0 7px}.hero h1{margin:0 0 7px;font-size:34px}.hero p:not(.eyebrow){margin:0;opacity:.82}.barrier-card{min-width:145px;padding:15px 18px;border:1px solid #ffffff33;border-radius:12px;background:#ffffff0d}.barrier-card span{display:block;font-size:12px;opacity:.7}.barrier-card strong{font-size:20px}main{max-width:1100px;margin:24px auto;padding:0 18px}.stats{display:grid;grid-template-columns:repeat(3,1fr);gap:14px;margin-bottom:18px}.stat,.panel{background:#fff;border-radius:14px;box-shadow:0 5px 20px #17202a10}.stat{padding:18px}.stat span{display:block;color:#667085;font-size:13px}.stat strong{display:block;font-size:28px;margin-top:5px}.panel{padding:20px;margin-bottom:18px}.section-heading{display:flex;justify-content:space-between;align-items:center;gap:10px}.panel h2{margin:0 0 6px}.panel p{color:#667085;margin-top:0}.slots{display:grid;grid-template-columns:repeat(auto-fit,minmax(100px,1fr));gap:10px;margin-top:15px}.slot{border-radius:10px;padding:16px 8px;text-align:center;font-weight:700}.slot small{display:block;font-weight:400;margin-top:4px}.slot.available{background:#e8f7ed;color:#176b38}.slot.occupied{background:#ffe8e8;color:#9d2020}.two-column{display:grid;grid-template-columns:1fr 1fr;gap:18px}form{display:grid;gap:12px}label{font-size:13px;font-weight:600;color:#344054}input,select,button{width:100%;padding:12px;border:1px solid #d0d5dd;border-radius:9px;font:inherit}input,select{margin-top:6px;background:#fff}button{border:0;background:#17202a;color:#fff;font-weight:700;cursor:pointer}button.secondary{width:auto;background:#eef1f4;color:#17202a;padding:9px 13px}.message{margin-top:14px;line-height:1.55}.success{padding:12px;border-radius:9px;background:#e8f7ed;color:#176b38}.error{padding:12px;border-radius:9px;background:#ffe8e8;color:#9d2020}.quote{border:1px solid #e0e4e8;border-radius:10px;padding:13px;margin-top:12px}.quote strong{font-size:23px}.table-wrap{overflow:auto}table{width:100%;border-collapse:collapse;font-size:13px}th,td{text-align:left;padding:10px;border-bottom:1px solid #eaecf0;white-space:nowrap}th{color:#667085}footer{text-align:center;padding:25px;color:#667085;font-size:12px}@media(max-width:720px){.hero{display:block}.barrier-card{margin-top:16px}.stats,.two-column{grid-template-columns:1fr}.hero h1{font-size:28px}}
)CSS";
}

std::string js() {
    return R"JS(const $=id=>document.getElementById(id);
const money=n=>`KSh ${Number(n).toLocaleString()}`;
const esc=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#039;'}[c]));

async function api(url,options={}){
  const r=await fetch(url,options);
  const data=await r.json();
  if(!r.ok) throw new Error(data.message||'Request failed');
  return data;
}

async function refresh(){
  try{
    const data=await api('/api/slots');
    $('total').textContent=data.total;
    $('available').textContent=data.available;
    $('occupied').textContent=data.occupied;
    $('slots').innerHTML=data.slots.map(s=>`<div class="slot ${s.status.toLowerCase()}">${esc(s.slot_number)}<small>${esc(s.status)}</small></div>`).join('');
    $('barrierStatus').textContent=data.barrier;
  }catch(e){ $('slots').innerHTML='<div class="error">Could not load parking data.</div>'; }
}

async function refreshRecords(){
  try{
    const data=await api('/api/records');
    $('records').innerHTML=data.records.map(r=>`<tr><td>${esc(r.registration)}</td><td>${esc(r.slot)}</td><td>${esc(r.entry_time)}</td><td>${esc(r.exit_time||'-')}</td><td>${r.duration_minutes==null?'-':r.duration_minutes+' min'}</td><td>${r.amount_due==null?'-':money(r.amount_due)}</td><td>${esc(r.status)}</td></tr>`).join('') || '<tr><td colspan="7">No records yet.</td></tr>';
  }catch(e){ $('records').innerHTML='<tr><td colspan="7">Could not load records.</td></tr>'; }
}

$('refreshBtn').addEventListener('click',()=>{refresh();refreshRecords();});
$('entryForm').addEventListener('submit',async e=>{
  e.preventDefault();
  $('entryMessage').className='message';
  try{
    const body=new URLSearchParams({registration:$('registration').value,vehicle_type:$('vehicleType').value});
    const data=await api('/api/entry',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
    $('entryMessage').className='message success';
    $('entryMessage').innerHTML=`Vehicle <strong>${esc(data.registration)}</strong> assigned to <strong>${esc(data.slot)}</strong>.<br>Entry time: ${esc(data.entry_time)}`;
    $('entryForm').reset();
    refresh();refreshRecords();
  }catch(e){ $('entryMessage').className='message error'; $('entryMessage').textContent=e.message; }
});

$('exitForm').addEventListener('submit',async e=>{
  e.preventDefault();
  $('exitResult').className='message';
  try{
    const body=new URLSearchParams({registration:$('exitRegistration').value});
    const data=await api('/api/exit-quote',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
    $('exitResult').className='message quote';
    $('exitResult').innerHTML=`<div>Vehicle: <strong>${esc(data.registration)}</strong></div><div>Slot: <strong>${esc(data.slot)}</strong></div><div>Duration: <strong>${data.duration_minutes} minutes</strong></div><div>Amount due: <strong>${money(data.amount_due)}</strong></div><button id="payBtn" style="margin-top:12px">Pay & open barrier</button>`;
    $('payBtn').onclick=()=>pay(data.session_id);
  }catch(e){ $('exitResult').className='message error'; $('exitResult').textContent=e.message; }
});

async function pay(sessionId){
  try{
    const body=new URLSearchParams({session_id:String(sessionId)});
    const data=await api('/api/payment',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
    $('exitResult').className='message success';
    $('exitResult').innerHTML=`Payment successful: <strong>${money(data.amount)}</strong>.<br>Barrier status: <strong>${data.barrier}</strong>.<br>Slot ${esc(data.slot)} is now available.`;
    $('exitForm').reset();
    refresh();refreshRecords();
  }catch(e){ $('exitResult').className='message error'; $('exitResult').textContent=e.message; }
}

refresh();refreshRecords();
setInterval(refresh,5000);
)JS";
}

void handleRequest(int client, const HttpRequest& request, Database& database, Barrier& barrier) {
    const std::string path = request.path.substr(0, request.path.find('?'));
    sqlite3* db = database.handle();

    if (request.method == "GET" && path == "/") {
        sendResponse(client, 200, "OK", htmlPage(), "text/html; charset=utf-8");
        return;
    }
    if (request.method == "GET" && path == "/style.css") {
        sendResponse(client, 200, "OK", css(), "text/css; charset=utf-8");
        return;
    }
    if (request.method == "GET" && path == "/app.js") {
        sendResponse(client, 200, "OK", js(), "application/javascript; charset=utf-8");
        return;
    }

    if (request.method == "GET" && path == "/api/slots") {
        sqlite3_stmt* stmt = nullptr;
        const char* sql = "SELECT slot_number,status FROM parking_slots ORDER BY slot_id;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            sendJson(client, 500, "Internal Server Error", R"({"message":"Could not read parking slots."})"); return;
        }
        int total = 0, available = 0, occupied = 0;
        std::string slots = "[";
        bool first = true;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ++total;
            std::string number = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            std::string status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            if (status == "AVAILABLE") ++available; else ++occupied;
            if (!first) slots += ",";
            first = false;
            slots += "{\"slot_number\":\"" + jsonEscape(number) + "\",\"status\":\"" + jsonEscape(status) + "\"}";
        }
        sqlite3_finalize(stmt);
        slots += "]";
        std::string body = "{\"total\":" + std::to_string(total) + ",\"available\":" + std::to_string(available) + ",\"occupied\":" + std::to_string(occupied) + ",\"barrier\":\"" + (barrier.isOpen() ? "OPEN" : "CLOSED") + "\",\"slots\":" + slots + "}";
        sendJson(client, 200, "OK", body);
        return;
    }

    if (request.method == "GET" && path == "/api/records") {
        sqlite3_stmt* stmt = nullptr;
        const char* sql = R"SQL(SELECT v.registration_number,s.slot_number,ps.entry_time,ps.exit_time,ps.duration_minutes,ps.amount_due,ps.session_status
FROM parking_sessions ps JOIN vehicles v ON v.vehicle_id=ps.vehicle_id JOIN parking_slots s ON s.slot_id=ps.slot_id
ORDER BY ps.session_id DESC LIMIT 50;)SQL";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            sendJson(client, 500, "Internal Server Error", R"({"message":"Could not read parking records."})"); return;
        }
        std::string records = "[";
        bool first = true;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            auto textCol = [&](int col) -> std::string {
                const unsigned char* value = sqlite3_column_text(stmt, col);
                return value ? reinterpret_cast<const char*>(value) : "";
            };
            if (!first) records += ",";
            first = false;
            records += "{\"registration\":\"" + jsonEscape(textCol(0)) + "\",\"slot\":\"" + jsonEscape(textCol(1)) + "\",\"entry_time\":\"" + jsonEscape(textCol(2)) + "\",\"exit_time\":\"" + jsonEscape(textCol(3)) + "\",\"duration_minutes\":" + (sqlite3_column_type(stmt,4)==SQLITE_NULL?"null":std::to_string(sqlite3_column_int(stmt,4))) + ",\"amount_due\":" + (sqlite3_column_type(stmt,5)==SQLITE_NULL?"null":std::to_string(sqlite3_column_int(stmt,5))) + ",\"status\":\"" + jsonEscape(textCol(6)) + "\"}";
        }
        sqlite3_finalize(stmt);
        records += "]";
        sendJson(client, 200, "OK", "{\"records\":" + records + "}");
        return;
    }

    if (request.method == "POST" && path == "/api/entry") {
        std::string registration = normalizeRegistration(formValue(request.body, "registration"));
        std::string vehicleType = trim(formValue(request.body, "vehicle_type"));
        if (!validRegistration(registration) || vehicleType.empty()) {
            sendJson(client, 400, "Bad Request", R"({"message":"Enter a valid registration number and vehicle type."})"); return;
        }

        sqlite3_stmt* stmt = nullptr;
        int vehicleId = 0;
        if (sqlite3_prepare_v2(db, "SELECT vehicle_id FROM vehicles WHERE registration_number=?;", -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, registration.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) == SQLITE_ROW) vehicleId = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);

        if (vehicleId > 0) {
            if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM parking_sessions WHERE vehicle_id=? AND session_status='ACTIVE';", -1, &stmt, nullptr) != SQLITE_OK) {
                sendJson(client, 500, "Internal Server Error", R"({"message":"Database error."})"); return;
            }
            sqlite3_bind_int(stmt, 1, vehicleId);
            bool active = sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt,0) > 0;
            sqlite3_finalize(stmt);
            if (active) {
                sendJson(client, 409, "Conflict", R"({"message":"This vehicle already has an active parking session."})"); return;
            }
            if (sqlite3_prepare_v2(db, "UPDATE vehicles SET vehicle_type=? WHERE vehicle_id=?;", -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(stmt,1,vehicleType.c_str(),-1,SQLITE_TRANSIENT); sqlite3_bind_int(stmt,2,vehicleId); sqlite3_step(stmt);
            }
            sqlite3_finalize(stmt);
        } else {
            if (sqlite3_prepare_v2(db, "INSERT INTO vehicles(registration_number,vehicle_type) VALUES(?,?);", -1, &stmt, nullptr) != SQLITE_OK) {
                sendJson(client, 500, "Internal Server Error", R"({"message":"Could not register vehicle."})"); return;
            }
            sqlite3_bind_text(stmt,1,registration.c_str(),-1,SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt,2,vehicleType.c_str(),-1,SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) != SQLITE_DONE) { sqlite3_finalize(stmt); sendJson(client,500,"Internal Server Error",R"({"message":"Could not register vehicle."})"); return; }
            vehicleId = static_cast<int>(sqlite3_last_insert_rowid(db));
            sqlite3_finalize(stmt);
        }

        int slotId = 0; std::string slotNumber;
        if (sqlite3_prepare_v2(db, "SELECT slot_id,slot_number FROM parking_slots WHERE status='AVAILABLE' ORDER BY slot_id LIMIT 1;", -1, &stmt, nullptr) != SQLITE_OK) {
            sendJson(client,500,"Internal Server Error",R"({"message":"Could not find an available slot."})"); return;
        }
        if (sqlite3_step(stmt) == SQLITE_ROW) { slotId=sqlite3_column_int(stmt,0); slotNumber=reinterpret_cast<const char*>(sqlite3_column_text(stmt,1)); }
        sqlite3_finalize(stmt);
        if (slotId == 0) { sendJson(client,409,"Conflict",R"({"message":"The parking area is full."})"); return; }

        if (sqlite3_exec(db,"BEGIN IMMEDIATE;",nullptr,nullptr,nullptr)!=SQLITE_OK) { sendJson(client,500,"Internal Server Error",R"({"message":"Could not start parking transaction."})"); return; }
        bool ok = false;
        if (sqlite3_prepare_v2(db, "INSERT INTO parking_sessions(vehicle_id,slot_id,entry_time,session_status) VALUES(?,?,datetime('now','localtime'),'ACTIVE');", -1, &stmt, nullptr)==SQLITE_OK) {
            sqlite3_bind_int(stmt,1,vehicleId); sqlite3_bind_int(stmt,2,slotId); ok = sqlite3_step(stmt)==SQLITE_DONE;
        }
        sqlite3_finalize(stmt);
        if (ok && sqlite3_prepare_v2(db,"UPDATE parking_slots SET status='OCCUPIED' WHERE slot_id=?;",-1,&stmt,nullptr)==SQLITE_OK) {
            sqlite3_bind_int(stmt,1,slotId); ok=sqlite3_step(stmt)==SQLITE_DONE;
        }
        sqlite3_finalize(stmt);
        if (ok) sqlite3_exec(db,"COMMIT;",nullptr,nullptr,nullptr); else sqlite3_exec(db,"ROLLBACK;",nullptr,nullptr,nullptr);
        if (!ok) { sendJson(client,500,"Internal Server Error",R"({"message":"Could not create the parking session."})"); return; }

        if (sqlite3_prepare_v2(db,"SELECT entry_time FROM parking_sessions WHERE session_id=last_insert_rowid();",-1,&stmt,nullptr)==SQLITE_OK) {
            if (sqlite3_step(stmt)==SQLITE_ROW) {
                std::string entry=reinterpret_cast<const char*>(sqlite3_column_text(stmt,0));
                sqlite3_finalize(stmt);
                sendJson(client,201,"Created","{\"message\":\"Arrival recorded.\",\"registration\":\""+jsonEscape(registration)+"\",\"slot\":\""+jsonEscape(slotNumber)+"\",\"entry_time\":\""+jsonEscape(entry)+"\"}");
                return;
            }
        }
        sqlite3_finalize(stmt);
        sendJson(client,201,"Created","{\"message\":\"Arrival recorded.\",\"registration\":\""+jsonEscape(registration)+"\",\"slot\":\""+jsonEscape(slotNumber)+"\"}");
        return;
    }

    if (request.method == "POST" && path == "/api/exit-quote") {
        std::string registration=normalizeRegistration(formValue(request.body,"registration"));
        if (!validRegistration(registration)) { sendJson(client,400,"Bad Request",R"({"message":"Enter a valid registration number."})"); return; }
        sqlite3_stmt* stmt=nullptr;
        const char* sql=R"SQL(SELECT ps.session_id,s.slot_number,ps.entry_time,
CAST((julianday('now','localtime')-julianday(ps.entry_time))*24*60 AS INTEGER) AS duration
FROM parking_sessions ps JOIN vehicles v ON v.vehicle_id=ps.vehicle_id JOIN parking_slots s ON s.slot_id=ps.slot_id
WHERE v.registration_number=? AND ps.session_status='ACTIVE' LIMIT 1;)SQL";
        if(sqlite3_prepare_v2(db,sql,-1,&stmt,nullptr)!=SQLITE_OK){sendJson(client,500,"Internal Server Error",R"({"message":"Database error."})");return;}
        sqlite3_bind_text(stmt,1,registration.c_str(),-1,SQLITE_TRANSIENT);
        if(sqlite3_step(stmt)!=SQLITE_ROW){sqlite3_finalize(stmt);sendJson(client,404,"Not Found",R"({"message":"No active parking session was found for this vehicle."})");return;}
        int sessionId=sqlite3_column_int(stmt,0); std::string slot=reinterpret_cast<const char*>(sqlite3_column_text(stmt,1)); long long duration=std::max(0LL,static_cast<long long>(sqlite3_column_int64(stmt,3)));
        sqlite3_finalize(stmt);
        int fee=calculateFee(duration);
        sendJson(client,200,"OK","{\"session_id\":"+std::to_string(sessionId)+",\"registration\":\""+jsonEscape(registration)+"\",\"slot\":\""+jsonEscape(slot)+"\",\"duration_minutes\":"+std::to_string(duration)+",\"amount_due\":"+std::to_string(fee)+"}");
        return;
    }

    if (request.method == "POST" && path == "/api/payment") {
        int sessionId=0; try { sessionId=std::stoi(formValue(request.body,"session_id")); } catch(...) {}
        if(sessionId<=0){sendJson(client,400,"Bad Request",R"({"message":"Invalid parking session."})");return;}
        sqlite3_stmt* stmt=nullptr;
        const char* sql=R"SQL(SELECT ps.vehicle_id,ps.slot_id,s.slot_number,
CAST((julianday('now','localtime')-julianday(ps.entry_time))*24*60 AS INTEGER)
FROM parking_sessions ps JOIN parking_slots s ON s.slot_id=ps.slot_id
WHERE ps.session_id=? AND ps.session_status='ACTIVE';)SQL";
        if(sqlite3_prepare_v2(db,sql,-1,&stmt,nullptr)!=SQLITE_OK){sendJson(client,500,"Internal Server Error",R"({"message":"Database error."})");return;}
        sqlite3_bind_int(stmt,1,sessionId);
        if(sqlite3_step(stmt)!=SQLITE_ROW){sqlite3_finalize(stmt);sendJson(client,404,"Not Found",R"({"message":"Active parking session not found."})");return;}
        int vehicleId=sqlite3_column_int(stmt,0), slotId=sqlite3_column_int(stmt,1); std::string slot=reinterpret_cast<const char*>(sqlite3_column_text(stmt,2)); long long duration=std::max(0LL,static_cast<long long>(sqlite3_column_int64(stmt,3)));
        sqlite3_finalize(stmt); int fee=calculateFee(duration);

        if(sqlite3_exec(db,"BEGIN IMMEDIATE;",nullptr,nullptr,nullptr)!=SQLITE_OK){sendJson(client,500,"Internal Server Error",R"({"message":"Could not start payment transaction."})");return;}
        bool ok=true;
        if(sqlite3_prepare_v2(db,"INSERT INTO payments(session_id,amount,payment_time,payment_status) VALUES(?,?,datetime('now','localtime'),'PAID') ON CONFLICT(session_id) DO UPDATE SET amount=excluded.amount,payment_time=excluded.payment_time,payment_status='PAID';",-1,&stmt,nullptr)==SQLITE_OK){sqlite3_bind_int(stmt,1,sessionId);sqlite3_bind_int(stmt,2,fee);ok=sqlite3_step(stmt)==SQLITE_DONE;}else ok=false;
        sqlite3_finalize(stmt);
        if(ok && sqlite3_prepare_v2(db,"UPDATE parking_sessions SET exit_time=datetime('now','localtime'),duration_minutes=?,amount_due=?,session_status='COMPLETED' WHERE session_id=?;",-1,&stmt,nullptr)==SQLITE_OK){sqlite3_bind_int64(stmt,1,duration);sqlite3_bind_int(stmt,2,fee);sqlite3_bind_int(stmt,3,sessionId);ok=sqlite3_step(stmt)==SQLITE_DONE;}else ok=false;
        sqlite3_finalize(stmt);
        if(ok && sqlite3_prepare_v2(db,"UPDATE parking_slots SET status='AVAILABLE' WHERE slot_id=?;",-1,&stmt,nullptr)==SQLITE_OK){sqlite3_bind_int(stmt,1,slotId);ok=sqlite3_step(stmt)==SQLITE_DONE;}else ok=false;
        sqlite3_finalize(stmt);
        if(ok)sqlite3_exec(db,"COMMIT;",nullptr,nullptr,nullptr);else sqlite3_exec(db,"ROLLBACK;",nullptr,nullptr,nullptr);
        if(!ok){sendJson(client,500,"Internal Server Error",R"({"message":"Payment could not be completed."})");return;}
        barrier.open();
        sendJson(client,200,"OK","{\"message\":\"Payment successful.\",\"amount\":"+std::to_string(fee)+",\"duration_minutes\":"+std::to_string(duration)+",\"slot\":\""+jsonEscape(slot)+"\",\"barrier\":\"OPEN\"}");
        barrier.close();
        return;
    }

    sendJson(client,404,"Not Found",R"({"message":"The requested resource was not found."})");
}

} // namespace

int main() {
    Database database("parking.db");
    if (!database.initialise()) {
        std::cerr << "Could not initialise database.\n";
        return 1;
    }

    Barrier barrier;
    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) { std::cerr << "Could not create server socket.\n"; return 1; }
    int yes=1;
    setsockopt(server,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));

    sockaddr_in address{};
    address.sin_family=AF_INET;
    address.sin_addr.s_addr=INADDR_ANY;
    int port = 8080;
    if (const char* envPort = std::getenv("PARKEASE_PORT")) {
        try { port = std::stoi(envPort); } catch (...) { port = 8080; }
    }
    address.sin_port=htons(static_cast<uint16_t>(port));

    if(bind(server,reinterpret_cast<sockaddr*>(&address),sizeof(address))<0 || listen(server,20)<0){
        std::cerr << "Could not start server on port 8080.\n";
        close(server); return 1;
    }

    std::cout << "ParkEase Kenya running at http://localhost:" << port << "\n";
    while(true){
        int client=accept(server,nullptr,nullptr);
        if(client<0) continue;
        HttpRequest request;
        if(readRequest(client,request)) handleRequest(client,request,database,barrier);
        else sendJson(client,400,"Bad Request",R"({"message":"Invalid HTTP request."})");
        close(client);
    }
    close(server);
    return 0;
}
