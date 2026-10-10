"""Optional Twilio SMS queue worker for GeoAlert.

Consumes Firebase Realtime Database /alerts/queue entries created by the dashboard.
Does not send until valid Twilio credentials and a Firebase Admin service account are set.
"""
from __future__ import annotations

import os
import time
from datetime import datetime, timezone
from pathlib import Path

from dotenv import load_dotenv
import firebase_admin
from firebase_admin import credentials, db
from twilio.rest import Client

ROOT = Path(__file__).resolve().parents[1]
load_dotenv(ROOT / ".env")

DATABASE_URL = os.getenv("FIREBASE_DATABASE_URL", "").strip()
SERVICE_PATH_RAW = os.getenv("FIREBASE_SERVICE_ACCOUNT_PATH", "private/firebase-service-account.json").strip()
TWILIO_SID = os.getenv("TWILIO_ACCOUNT_SID", "").strip()
TWILIO_TOKEN = os.getenv("TWILIO_AUTH_TOKEN", "").strip()
TWILIO_FROM = os.getenv("TWILIO_FROM_NUMBER", "").strip()
POLL_SECONDS = max(2, int(os.getenv("POLL_INTERVAL_SECONDS", "5")))


def now_utc() -> str:
    return datetime.now(timezone.utc).isoformat()


def main() -> int:
    missing = [name for name, value in {
        "FIREBASE_DATABASE_URL": DATABASE_URL,
        "TWILIO_ACCOUNT_SID": TWILIO_SID,
        "TWILIO_AUTH_TOKEN": TWILIO_TOKEN,
        "TWILIO_FROM_NUMBER": TWILIO_FROM,
    }.items() if not value]
    service_path = Path(SERVICE_PATH_RAW)
    if not service_path.is_absolute():
        service_path = ROOT / service_path
    if not service_path.exists():
        missing.append(f"Firebase service-account JSON at {service_path}")
    if missing:
        print("Twilio worker is not configured. Missing: " + ", ".join(missing))
        print("Use a fresh service-account JSON and configure .env locally; never commit credentials.")
        return 2

    cred_obj = credentials.Certificate(str(service_path))
    firebase_admin.initialize_app(cred_obj, {"databaseURL": DATABASE_URL})
    client = Client(TWILIO_SID, TWILIO_TOKEN)
    queue_ref = db.reference("/alerts/queue")
    print(f"GeoAlert Twilio worker started; polling every {POLL_SECONDS}s. Ctrl+C to stop.")

    while True:
        try:
            queue = queue_ref.get() or {}
            if isinstance(queue, dict):
                for key, item in queue.items():
                    if not isinstance(item, dict):
                        continue
                    status = str(item.get("status", "queued")).lower()
                    if status in {"sent", "processing", "failed", "cancelled"}:
                        continue
                    message = str(item.get("message", "")).strip()
                    to_number = str(item.get("to", "")).strip()
                    # Dashboard broadcasts are zone-based; explicit recipient mapping is not
                    # supplied by the current UI, so do not guess or bulk-message unknown users.
                    if not message or not to_number:
                        queue_ref.child(key).update({
                            "status": "failed",
                            "error": "Missing message or explicit 'to' phone number; no SMS sent.",
                            "updated_at": now_utc(),
                        })
                        print(f"Queue item {key}: failed validation (missing recipient).")
                        continue
                    queue_ref.child(key).update({"status": "processing", "updated_at": now_utc()})
                    try:
                        sent = client.messages.create(body=message, from_=TWILIO_FROM, to=to_number)
                        queue_ref.child(key).update({"status": "sent", "twilio_sid": sent.sid, "sent_at": now_utc()})
                        print(f"Queue item {key}: SMS submitted; status={sent.status}.")
                    except Exception as exc:  # avoid logging credentials
                        queue_ref.child(key).update({"status": "failed", "error": str(exc)[:500], "updated_at": now_utc()})
                        print(f"Queue item {key}: Twilio request failed: {type(exc).__name__}.")
        except KeyboardInterrupt:
            print("Twilio worker stopped.")
            return 0
        except Exception as exc:
            print(f"Queue polling error: {type(exc).__name__}: {exc}")
        time.sleep(POLL_SECONDS)


if __name__ == "__main__":
    raise SystemExit(main())
