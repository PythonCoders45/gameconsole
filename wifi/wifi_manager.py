#!/usr/bin/env python3
"""
wifi_manager_big.py

A comprehensive Wi-Fi manager for Linux (NetworkManager/nmcli).
Exposes a Flask HTTP API and a CLI facade for scanning, connecting,
profile management, IP info, hotspot creation, and diagnostics.

WARNING:
 - Many commands require root (sudo) because they change network state.
 - Do NOT expose this API to the public internet without additional auth & TLS.
 - Designed for systems using NetworkManager (nmcli). Some systems use other tools.

Run:
  sudo python3 wifi_manager_big.py

API:
  GET  /api/scan                 -> scan networks
  POST /api/connect              -> {"ssid":"...","password":"...","profile_name":"opt"}
  POST /api/disconnect           -> {"profile":"..."} (or active)
  GET  /api/status               -> internet/local IP/active
  GET  /api/public_ip            -> external/public IP (requests to icanhazip)
  POST /api/set_static_ip        -> {"iface":"wlan0","ip":"192.168.1.50/24","gw":"192.168.1.1","dns":["8.8.8.8"]}
  POST /api/create_hotspot       -> {"ssid":"MyAP","password":"pass1234"}
  POST /api/delete_hotspot       -> {}
  GET  /api/profiles             -> list saved profiles
  POST /api/profiles             -> {"name":"pname","ssid":"...","password":"...","type":"wifi"}
  DELETE /api/profiles/<name>    -> remove saved profile
  GET  /api/interfaces           -> list network interfaces and addresses
  GET  /api/logs                 -> last lines of log

Security:
 - Basic token auth via header "X-API-Token: <token>". Token generated at first run and saved as `api_token.txt`.
"""

import os, sys, json, subprocess, shutil, time, socket, threading, logging, datetime
from flask import Flask, request, jsonify, abort
from flask_cors import CORS
import requests

# -----------------------
# Config & paths
# -----------------------
BASE_DIR = os.path.abspath(os.path.dirname(__file__))
PROFILES_FILE = os.path.join(BASE_DIR, "wifi_profiles.json")
LOG_FILE = os.path.join(BASE_DIR, "wifi_manager.log")
API_TOKEN_FILE = os.path.join(BASE_DIR, "api_token.txt")

FLASK_HOST = "0.0.0.0"
FLASK_PORT = 5002

# -----------------------
# Logging
# -----------------------
logger = logging.getLogger("wifi_manager")
logger.setLevel(logging.DEBUG)
fh = logging.FileHandler(LOG_FILE)
fh.setFormatter(logging.Formatter("%(asctime)s [%(levelname)s] %(message)s"))
logger.addHandler(fh)
ch = logging.StreamHandler(sys.stdout)
ch.setFormatter(logging.Formatter("%(asctime)s [%(levelname)s] %(message)s"))
logger.addHandler(ch)

# -----------------------
# Helper functions
# -----------------------
def run_cmd(cmd, timeout=20, shell=True):
    """Run a shell command and return (rc, stdout, stderr)."""
    logger.debug(f"CMD: {cmd}")
    try:
        proc = subprocess.run(cmd, shell=shell, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout, text=True)
        out = proc.stdout.strip()
        err = proc.stderr.strip()
        logger.debug(f"RC={proc.returncode} OUT={out!r} ERR={err!r}")
        return proc.returncode, out, err
    except subprocess.TimeoutExpired:
        logger.warning(f"Command timeout: {cmd}")
        return 124, "", "timeout"

def ensure_profiles():
    if not os.path.exists(PROFILES_FILE):
        with open(PROFILES_FILE, "w") as f:
            json.dump({}, f, indent=2)

def load_profiles():
    ensure_profiles()
    try:
        with open(PROFILES_FILE, "r") as f:
            return json.load(f)
    except Exception as e:
        logger.exception("Failed to load profiles")
        return {}

def save_profiles(p):
    with open(PROFILES_FILE, "w") as f:
        json.dump(p, f, indent=2)

def generate_token_if_missing():
    if not os.path.exists(API_TOKEN_FILE):
        token = os.urandom(16).hex()
        with open(API_TOKEN_FILE, "w") as f:
            f.write(token)
        logger.info(f"Generated API token and saved to {API_TOKEN_FILE}")
        return token
    else:
        with open(API_TOKEN_FILE, "r") as f:
            return f.read().strip()

API_TOKEN = generate_token_if_missing()
logger.info(f"API token: {API_TOKEN} (store securely)")

def has_nmcli():
    return shutil.which("nmcli") is not None

# -----------------------
# Network functions (nmcli-centered)
# -----------------------
def nmcli_scan():
    """Use nmcli to list wifi networks and parse output."""
    if not has_nmcli():
        return False, "nmcli not found"
    # use terse output for easier parsing
    cmd = "nmcli -t -f SSID,SIGNAL,SECURITY device wifi list --rescan yes"
    rc, out, err = run_cmd(cmd, timeout=10)
    if rc != 0:
        return False, err or out
    nets = []
    for line in out.splitlines():
        # format: SSID:SIGNAL:SECURITY
        parts = line.split(":")
        if len(parts) >= 1:
            ssid = parts[0]
            signal = parts[1] if len(parts) >= 2 else ""
            sec = parts[2] if len(parts) >= 3 else ""
            nets.append({"ssid": ssid, "signal": signal, "security": sec})
    return True, nets

def nmcli_connect(ssid, password=None, profile_name=None, timeout=25):
    """Connect to SSID, optionally create or reuse connection name (profile_name)."""
    if not has_nmcli():
        return False, "nmcli not found"
    try:
        if profile_name:
            # create connection (if not exists) and then up
            # Use nmcli connection add if connection missing; else use nmcli connection up
            # For WPA-PSK
            prof_list_rc, prof_list_out, _ = run_cmd(f"nmcli -t -f NAME connection show --active || true", timeout=5)
            # Try to add or modify connection
            if password:
                add_cmd = f"nmcli connection add type wifi ifname '*' con-name \"{profile_name}\" ssid \"{ssid}\" -- wifi-sec.key-mgmt wpa-psk wifi-sec.psk \"{password}\""
            else:
                add_cmd = f"nmcli connection add type wifi ifname '*' con-name \"{profile_name}\" ssid \"{ssid}\""
            # Attempt to add - if exists will fail; ignore failure and try bring up
            run_cmd(add_cmd, timeout=8)
            up_cmd = f"nmcli connection up \"{profile_name}\""
            rc, out, err = run_cmd(up_cmd, timeout=timeout)
            if rc == 0:
                return True, out
            else:
                # fallback: try nmcli device wifi connect
                pass

        # fallback simpler connect
        if password:
            cmd = f"nmcli device wifi connect \"{ssid}\" password \"{password}\""
        else:
            cmd = f"nmcli device wifi connect \"{ssid}\""
        rc, out, err = run_cmd(cmd, timeout=timeout)
        if rc == 0:
            return True, out
        else:
            return False, err or out
    except Exception as e:
        logger.exception("connect failed")
        return False, str(e)

def nmcli_disconnect(profile=None):
    if not has_nmcli():
        return False, "nmcli not found"
    if profile:
        cmd = f"nmcli connection down \"{profile}\""
    else:
        # disconnect all wifi devices
        cmd = "nmcli device disconnect wlan0 || nmcli device disconnect wlp2s0 || true"
    rc, out, err = run_cmd(cmd, timeout=8)
    if rc == 0:
        return True, out
    else:
        return False, err or out

def nmcli_list_connections():
    if not has_nmcli():
        return False, "nmcli not found"
    cmd = "nmcli -t -f NAME,TYPE,UUID connection show"
    rc, out, err = run_cmd(cmd, timeout=5)
    if rc != 0:
        return False, err or out
    conns = []
    for line in out.splitlines():
        parts = line.split(":")
        if len(parts) >= 1:
            name = parts[0]
            typ = parts[1] if len(parts) >= 2 else ""
            uid = parts[2] if len(parts) >= 3 else ""
            conns.append({"name": name, "type": typ, "uuid": uid})
    return True, conns

def nmcli_create_hotspot(ssid, password):
    if not has_nmcli():
        return False, "nmcli not found"
    # password must be >= 8 for WPA2
    if not password or len(password) < 8:
        return False, "hotspot password must be >= 8 chars"
    cmd = f"nmcli device wifi hotspot ifname wlan0 ssid \"{ssid}\" password \"{password}\""
    rc, out, err = run_cmd(cmd, timeout=10)
    if rc == 0:
        return True, out
    else:
        return False, err or out

def nmcli_delete_hotspot():
    # no direct nmcli hotspot delete, we bring down added connection or restore state
    # list connections and attempt to delete one with "Hotspot" pattern
    ok, conns = nmcli_list_connections()
    if not ok:
        return False, conns
    deleted = []
    for c in conns:
        if "hotspot" in c["name"].lower() or "Hotspot" in c["name"]:
            rc, out, err = run_cmd(f"nmcli connection delete \"{c['name']}\"")
            if rc == 0:
                deleted.append(c["name"])
    return True, deleted

# -----------------------
# IP & status helpers
# -----------------------
def get_local_interfaces():
    # use ip addr show
    rc, out, err = run_cmd("ip -4 addr show", timeout=4)
    if rc != 0:
        return False, err or out
    # rough parse
    interfaces = {}
    current_iface = None
    for line in out.splitlines():
        if line and not line.startswith(" "):
            # new iface: "2: enp3s0: <BROADCAST,...> mtu 1500 qdisc ..."
            parts = line.split(":")
            if len(parts) >= 2:
                current_iface = parts[1].strip().split()[0]
                interfaces[current_iface] = {"addrs": []}
        else:
            if "inet " in line and current_iface:
                # line like "    inet 192.168.1.10/24 brd ... scope global dynamic noprefixroute enp3s0"
                parts = line.strip().split()
                ipmask = parts[1]
                interfaces[current_iface]["addrs"].append(ipmask)
    return True, interfaces

def get_public_ip():
    # try several services
    services = ["https://icanhazip.com", "https://ifconfig.me/ip", "https://api.ipify.org"]
    for url in services:
        try:
            r = requests.get(url, timeout=4)
            if r.status_code == 200:
                return True, r.text.strip()
        except Exception as e:
            logger.debug(f"Public IP check failed for {url}: {e}")
    return False, "failed"

# -----------------------
# Flask API
# -----------------------
app = Flask("wifi_manager_big")
CORS(app)

# Simple token auth decorator
def require_token(fn):
    def wrapped(*args, **kwargs):
        token = request.headers.get("X-API-Token") or request.args.get("token")
        if not token or token != API_TOKEN:
            return jsonify({"success": False, "error": "unauthorized"}), 401
        return fn(*args, **kwargs)
    wrapped.__name__ = fn.__name__
    return wrapped

@app.route("/api/scan", methods=["GET"])
@require_token
def api_scan():
    ok, data = nmcli_scan()
    if not ok:
        return jsonify({"success": False, "error": data}), 500
    # filter duplicate empty SSIDs and sort by signal (if present)
    nets = [n for n in data if n.get("ssid")]
    try:
        nets.sort(key=lambda x: int(x.get("signal") or 0), reverse=True)
    except:
        pass
    return jsonify({"success": True, "networks": nets})

@app.route("/api/connect", methods=["POST"])
@require_token
def api_connect():
    d = request.json or {}
    ssid = d.get("ssid")
    password = d.get("password")
    profile_name = d.get("profile_name")
    if not ssid:
        return jsonify({"success": False, "error": "ssid required"}), 400
    ok, out = nmcli_connect(ssid, password, profile_name)
    if ok:
        # optionally save profile
        if d.get("save_profile"):
            profs = load_profiles()
            pname = profile_name or f"profile_{ssid}"
            profs[pname] = {"ssid": ssid, "password": password, "created": datetime.datetime.utcnow().isoformat()}
            save_profiles(profs)
        # check internet
        inet_ok, _ = get_public_ip()
        return jsonify({"success": True, "message": out, "internet": inet_ok})
    else:
        return jsonify({"success": False, "error": out}), 500

@app.route("/api/disconnect", methods=["POST"])
@require_token
def api_disconnect():
    d = request.json or {}
    profile = d.get("profile")
    ok, out = nmcli_disconnect(profile)
    if ok:
        return jsonify({"success": True, "message": out})
    else:
        return jsonify({"success": False, "error": out}), 500

@app.route("/api/status", methods=["GET"])
@require_token
def api_status():
    inet_ok, pub = get_public_ip()
    lok, ifs = get_local_interfaces()
    active = None
    if has_nmcli():
        rc, out, err = run_cmd("nmcli -t -f ACTIVE,SSID dev wifi | egrep '^yes' || true", timeout=3)
        if rc == 0 and out:
            parts = out.split(":")
            if len(parts) >= 2:
                active = parts[1]
    return jsonify({"success": True, "internet": inet_ok, "public_ip": pub if inet_ok else None, "interfaces": ifs if lok else {}, "active_ssid": active})

@app.route("/api/public_ip", methods=["GET"])
@require_token
def api_public_ip():
    ok, v = get_public_ip()
    if ok:
        return jsonify({"success": True, "public_ip": v})
    else:
        return jsonify({"success": False, "error": v}), 500

@app.route("/api/set_static_ip", methods=["POST"])
@require_token
def api_set_static_ip():
    d = request.json or {}
    iface = d.get("iface")
    ip = d.get("ip")  # e.g., 192.168.1.50/24
    gw = d.get("gw")
    dns = d.get("dns", [])
    if not iface or not ip or not gw:
        return jsonify({"success": False, "error": "iface, ip and gw required"}), 400
    if not has_nmcli():
        return jsonify({"success": False, "error": "nmcli not found"}), 500
    # modify connection for the given interface - find a connection bound to iface
    rc, out, err = run_cmd(f"nmcli -t -f NAME,DEVICE connection show --active | egrep ':{iface}$' || true", timeout=4)
    conn_name = None
    if rc == 0 and out:
        conn_name = out.split(":")[0]
    else:
        # fallback choose any wifi connection or create a temp one
        ok, conns = nmcli_list_connections()
        if ok and conns:
            conn_name = conns[0]["name"]
    if not conn_name:
        return jsonify({"success": False, "error": "no connection found for iface"}), 500
    # set ipv4.method manual and address/gateway/dns
    cmd = f"nmcli connection modify \"{conn_name}\" ipv4.method manual ipv4.addresses {ip} ipv4.gateway {gw}"
    rc, out, err = run_cmd(cmd, timeout=6)
    if rc != 0:
        return jsonify({"success": False, "error": err or out}), 500
    if dns:
        dns_str = ",".join(dns)
        rc2, out2, err2 = run_cmd(f"nmcli connection modify \"{conn_name}\" ipv4.dns \"{dns_str}\"", timeout=6)
        if rc2 != 0:
            logger.warning("Setting DNS failed: " + (err2 or out2))
    # bring connection down & up
    run_cmd(f"nmcli connection down \"{conn_name}\"", timeout=5)
    rc3, out3, err3 = run_cmd(f"nmcli connection up \"{conn_name}\"", timeout=8)
    if rc3 == 0:
        return jsonify({"success": True, "message": out3})
    else:
        return jsonify({"success": False, "error": err3 or out3}), 500

@app.route("/api/create_hotspot", methods=["POST"])
@require_token
def api_create_hotspot():
    d = request.json or {}
    ssid = d.get("ssid")
    password = d.get("password")
    if not ssid or not password:
        return jsonify({"success": False, "error": "ssid and password required"}), 400
    ok, out = nmcli_create_hotspot(ssid, password)
    if ok:
        return jsonify({"success": True, "message": out})
    else:
        return jsonify({"success": False, "error": out}), 500

@app.route("/api/delete_hotspot", methods=["POST"])
@require_token
def api_delete_hotspot():
    ok, out = nmcli_delete_hotspot()
    if ok:
        return jsonify({"success": True, "deleted": out})
    else:
        return jsonify({"success": False, "error": out}), 500

@app.route("/api/profiles", methods=["GET","POST"])
@require_token
def api_profiles():
    if request.method == "GET":
        return jsonify({"success": True, "profiles": load_profiles()})
    else:
        d = request.json or {}
        name = d.get("name")
        ssid = d.get("ssid")
        pwd = d.get("password")
        if not name or not ssid:
            return jsonify({"success": False, "error": "name & ssid required"}), 400
        profs = load_profiles()
        profs[name] = {"ssid": ssid, "password": pwd, "updated": datetime.datetime.utcnow().isoformat()}
        save_profiles(profs)
        return jsonify({"success": True, "profile": profs[name]})

@app.route("/api/profiles/<name>", methods=["DELETE"])
@require_token
def api_profile_delete(name):
    profs = load_profiles()
    if name in profs:
        del profs[name]
        save_profiles(profs)
        return jsonify({"success": True})
    else:
        return jsonify({"success": False, "error": "not found"}), 404

@app.route("/api/interfaces", methods=["GET"])
@require_token
def api_interfaces():
    ok, out = get_local_interfaces()
    if ok:
        return jsonify({"success": True, "interfaces": out})
    else:
        return jsonify({"success": False, "error": out}), 500

@app.route("/api/connections", methods=["GET"])
@require_token
def api_connections():
    ok, out = nmcli_list_connections()
    if ok:
        return jsonify({"success": True, "connections": out})
    else:
        return jsonify({"success": False, "error": out}), 500

@app.route("/api/logs", methods=["GET"])
@require_token
def api_logs():
    try:
        lines = []
        with open(LOG_FILE, "r") as f:
            lines = f.readlines()
        # return last 400 lines
        return jsonify({"success": True, "logs": "".join(lines[-400:])})
    except Exception as e:
        return jsonify({"success": False, "error": str(e)}), 500

@app.route("/api/ping", methods=["GET"])
def api_ping():
    return jsonify({"success": True, "message": "pong"})

# -----------------------
# CLI helpers (optional)
# -----------------------
def print_help():
    print("wifi_manager_big CLI")
    print("Usage:")
    print("  python wifi_manager_big.py scan")
    print("  python wifi_manager_big.py connect <ssid> [password]")
    print("  python wifi_manager_big.py status")
    print("  python wifi_manager_big.py public_ip")
    print("  python wifi_manager_big.py token")
    print("  python wifi_manager_big.py runserver")

def cli_entry():
    if len(sys.argv) <= 1:
        print_help()
        return
    cmd = sys.argv[1]
    if cmd == "scan":
        ok, res = nmcli_scan()
        print(json.dumps({"ok": ok, "networks": res}, indent=2, ensure_ascii=False))
    elif cmd == "connect":
        if len(sys.argv) < 3:
            print("Usage: connect <ssid> [password]")
            return
        ssid = sys.argv[2]
        pwd = sys.argv[3] if len(sys.argv) >= 4 else None
        ok, res = nmcli_connect(ssid, pwd)
        print({"success": ok, "out": res})
    elif cmd == "status":
        ok, out = get_local_interfaces()
        print(json.dumps({"ok": ok, "interfaces": out}, indent=2))
    elif cmd == "public_ip":
        ok, out = get_public_ip()
        print({"success": ok, "public_ip": out})
    elif cmd == "token":
        print(API_TOKEN)
    elif cmd == "runserver":
        start_server()
    else:
        print_help()

# -----------------------
# Server starter
# -----------------------
def start_server():
    logger.info(f"Starting server on http://{FLASK_HOST}:{FLASK_PORT}")
    app.run(host=FLASK_HOST, port=FLASK_PORT)

# -----------------------
# Main
# -----------------------
if __name__ == "__main__":
    # If invoked with CLI args, run CLI; else start Flask server
    if len(sys.argv) > 1 and sys.argv[1] != "runserver":
        cli_entry()
    else:
        # default: start server
        try:
            start_server()
        except Exception as e:
            logger.exception("Server failed")
