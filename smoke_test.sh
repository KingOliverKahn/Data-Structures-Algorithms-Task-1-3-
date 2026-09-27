#!/usr/bin/env bash
set -euo pipefail

PORT=18080
BASE="http://127.0.0.1:${PORT}"
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"
rm -f parking.db
PARKEASE_PORT=$PORT "$ROOT_DIR/build/parking_server" > /tmp/parkease-smoke.log 2>&1 &
SERVER_PID=$!
trap 'kill "$SERVER_PID" 2>/dev/null || true; rm -f parking.db' EXIT
sleep 1

slots=$(curl -fsS "$BASE/api/slots")
grep -q '"total":12' <<< "$slots"
grep -q '"available":12' <<< "$slots"

entry=$(curl -fsS -X POST "$BASE/api/entry" -d 'registration=KDA%20123A&vehicle_type=Car')
grep -q '"slot":"P01"' <<< "$entry"

duplicate_code=$(curl -sS -o /tmp/duplicate.json -w '%{http_code}' -X POST "$BASE/api/entry" -d 'registration=KDA%20123A&vehicle_type=Car')
test "$duplicate_code" = "409"

grep -q 'already has an active parking session' /tmp/duplicate.json

python3 - <<'PY'
import sqlite3
c = sqlite3.connect('parking.db')
c.execute("UPDATE parking_sessions SET entry_time=datetime('now','localtime','-31 minutes') WHERE session_status='ACTIVE'")
c.commit()
c.close()
PY

quote=$(curl -fsS -X POST "$BASE/api/exit-quote" -d 'registration=KDA%20123A')
grep -q '"duration_minutes":31' <<< "$quote"
grep -q '"amount_due":50' <<< "$quote"

payment=$(curl -fsS -X POST "$BASE/api/payment" -d 'session_id=1')
grep -q '"amount":50' <<< "$payment"
grep -q '"barrier":"OPEN"' <<< "$payment"

final=$(curl -fsS "$BASE/api/slots")
grep -q '"available":12' <<< "$final"
grep -q '"occupied":0' <<< "$final"

echo "End-to-end smoke test passed."
